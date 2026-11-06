#include "features/mouse/mouse.h"
#include "game/eqgame.h"
#include "overlay/widgets.h"
#include <imgui.h>
#include <iterator>

namespace mouse {

Settings cfg;

namespace {

const Setting kSettings[] = {
    BoolSetting ("mouse", "enabled", &cfg.enabled, "Apply the mouse fixes below."),
    FloatSetting("mouse", "x_sensitivity", &cfg.x_sensitivity, 0.1f, 10.0f, "Horizontal sensitivity multiplier on top of the game's own setting."),
    FloatSetting("mouse", "y_sensitivity", &cfg.y_sensitivity, 0.1f, 10.0f, "Vertical sensitivity multiplier."),
    BoolSetting ("mouse", "fps_independent_turn", &cfg.fps_independent_turn, "Horizontal mouselook turns the same at any frame rate (as the game does at 60 FPS)."),
    BoolSetting ("mouse", "remove_deadzone", &cfg.remove_deadzone, "Remove the game's mouselook axis dead-zone that makes diagonal moves jumpy."),
};

void DrawMenu()
{
    ui::SliderF("X sensitivity", &cfg.x_sensitivity, 0.1f, 10.0f, "%.2fx");
    ui::SliderF("Y sensitivity", &cfg.y_sensitivity, 0.1f, 10.0f, "%.2fx");
    if (ImGui::Button("Reset mouse")) { cfg.x_sensitivity = cfg.y_sensitivity = 1.0f; ResetAccumulators(); }
    if (game::Available()) {
        ImGui::Checkbox("Frame-rate independent mouselook", &cfg.fps_independent_turn);
        ui::Hint("Turning speed no longer drops at high FPS.");
        ImGui::Checkbox("Remove mouselook axis dead-zone", &cfg.remove_deadzone);
        ui::Hint("Diagonal mouselook no longer snaps to one axis.");
    }
}

} // namespace

const Feature feature = {
    "Mouse",
    "Mouselook fixes.",
    "Enable mouse fixes", &cfg.enabled,
    kSettings, std::size(kSettings), ApplyGamePatches, DrawMenu,
};

} // namespace mouse
