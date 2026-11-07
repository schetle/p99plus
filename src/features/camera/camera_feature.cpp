#include "features/camera/camera.h"
#include "features/camera/internal.h"
#include "overlay/widgets.h"
#include <imgui.h>
#include <iterator>

namespace camera {

Settings cfg;

namespace {

constexpr float kOffsetMax = 1000.0f;   // generous: the menu sliders are narrower, typed values can go further

const Setting kSettings[] = {
    BoolSetting ("offset", "enabled", &cfg.offset.enabled, "Apply the camera offsets below."),
    BoolSetting ("offset", "zoom_fade", &cfg.offset.zoom_fade, "Shrink the third-person offset as the camera zooms in toward the head."),
    FloatSetting("offset", "zoom_fade_distance", &cfg.offset.zoom_fade_distance, 1.0f, 1000.0f, "Distance from the head at which the full third-person offset applies."),

    FloatSetting("offset.first_person", "up", &cfg.offset.first_person.up, -kOffsetMax, kOffsetMax, "Eye offset along world up."),
    FloatSetting("offset.first_person", "forward", &cfg.offset.first_person.forward, -kOffsetMax, kOffsetMax, "Eye offset along the camera's horizontal facing."),
    FloatSetting("offset.first_person", "right", &cfg.offset.first_person.right, -kOffsetMax, kOffsetMax, "Eye offset along the camera's horizontal right."),

    FloatSetting("offset.third_person", "up", &cfg.offset.third_person.up, -kOffsetMax, kOffsetMax, "Eye offset along world up."),
    FloatSetting("offset.third_person", "forward", &cfg.offset.third_person.forward, -kOffsetMax, kOffsetMax, "Eye offset along the camera's horizontal facing."),
    FloatSetting("offset.third_person", "right", &cfg.offset.third_person.right, -kOffsetMax, kOffsetMax, "Eye offset along the camera's horizontal right."),

    BoolSetting ("orbit", "enabled", &cfg.orbit.enabled, "Holding the middle mouse button and dragging rotates the camera around the character."),
    BoolSetting ("orbit", "return_on_release", &cfg.orbit.return_on_release, "Swing back behind the character as soon as the button is released."),
    BoolSetting ("orbit", "return_on_move", &cfg.orbit.return_on_move, "Swing back when the character moves or turns (right-click always swings back)."),

    BoolSetting ("fov", "enabled", &cfg.fov.enabled, "Control the field of view; off leaves the game's own."),
    FloatSetting("fov", "first_person", &cfg.fov.first_person, 0.0f, 170.0f, "Vertical field of view in degrees in first person; 0 = default (widens horizontally to fit the screen)."),
    FloatSetting("fov", "third_person", &cfg.fov.third_person, 0.0f, 170.0f, "Vertical field of view in degrees in third person; 0 = default."),
    BoolSetting ("camera", "fix_north_jump", &cfg.fix_north_jump, "Fix the game's heading math that makes the camera jump a little when you turn through north."),
};

void DrawOffsets(const char* id, ModeSettings& m)
{
    ImGui::PushID(id);
    ui::SliderF("Up", &m.up, -50.0f, 50.0f, "%.1f");
    ui::SliderF("Forward", &m.forward, -50.0f, 50.0f, "%.1f");
    ui::SliderF("Right", &m.right, -50.0f, 50.0f, "%.1f");
    ImGui::PopID();
}

void DrawOffsetSection()
{
    OffsetSettings& o = cfg.offset;
    ui::BeginSection("Offset", "Moves the camera's eye point.",
                     "Enable camera offset", &o.enabled);

    ImGui::TextUnformatted("First person");
    DrawOffsets("fp", o.first_person);
    if (ImGui::Button("Reset first person")) o.first_person.up = o.first_person.forward = o.first_person.right = 0.0f;

    ImGui::Spacing();
    ImGui::TextUnformatted("Third person");
    DrawOffsets("tp", o.third_person);
    if (ImGui::Button("Reset third person")) o.third_person.up = o.third_person.forward = o.third_person.right = 0.0f;

    ImGui::Spacing();
    ImGui::Checkbox("Shrink offset as you zoom in", &o.zoom_fade);
    ImGui::BeginDisabled(!o.zoom_fade);
    ui::SliderF("Full offset beyond distance", &o.zoom_fade_distance, 5.0f, 100.0f, "%.0f");
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Closer than this, the offset shrinks toward your head.");
    if (o.zoom_fade) {
        const Status st = GetStatus();
        if (st.firstPerson)           ui::Hint("Now: first person");
        else if (st.distance >= 0.0f) ui::Hint("Now: distance %.0f, offset %.0f%%", st.distance, st.percent);
        else                          ui::Hint("Now: not in the world");
    }
    ui::EndSection();
}

void DrawOrbitSection()
{
    OrbitSettings& o = cfg.orbit;
    ui::BeginSection("Orbit", nullptr,
                     "Middle mouse drag rotates the camera", &o.enabled);
    ImGui::Checkbox("Return camera when you release", &o.return_on_release);
    ImGui::BeginDisabled(o.return_on_release);
    ImGui::Checkbox("Return camera when you move", &o.return_on_move);
    ImGui::EndDisabled();
    ui::EndSection();
}

void DrawFovSection()
{
    FovSettings& f = cfg.fov;
    ui::BeginSection("FOV", "Default widens the view to fit your screen.",
                     "Enable FOV control", &f.enabled);
    ui::SliderF("First-person FOV", &f.first_person, 30.0f, 120.0f, f.first_person > 0.0f ? "%.1f deg" : "default");
    if (ImGui::Button("Reset first-person FOV")) f.first_person = 0.0f;
    ui::SliderF("Third-person FOV", &f.third_person, 30.0f, 120.0f, f.third_person > 0.0f ? "%.1f deg" : "default");
    if (ImGui::Button("Reset third-person FOV")) f.third_person = 0.0f;
    if (const char* problem = FovProblem())
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", problem);
    ui::EndSection();
}

void DrawMenu()
{
    DrawFovSection();
    DrawOffsetSection();
    DrawOrbitSection();
    ImGui::SeparatorText("Fixes");
    ImGui::Checkbox("Fix camera jump facing north", &cfg.fix_north_jump);
}

void OnFrame()
{
    EngineHooksOnFrame();
    GameHooksOnFrame();
    orbit::OnFrame();
    NorthFixOnFrame();
}

} // namespace

const Feature feature = {
    "Camera",
    nullptr,
    nullptr, nullptr,
    kSettings, std::size(kSettings), OnFrame, DrawMenu, StashRecords,
};

} // namespace camera
