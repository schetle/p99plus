#include "overlay/menu.h"
#include "core/app.h"
#include "core/config.h"
#include "core/feature.h"
#include "overlay/overlay.h"
#include "overlay/widgets.h"
#include <cstdio>
#include <imgui.h>

namespace overlay {

namespace {

// One feature's tab: description, the enable checkbox, then its controls (greyed out while disabled).
void DrawFeaturePage(const Feature& f)
{
    if (f.description) {
        ui::Hint("%s", f.description);
        ImGui::Spacing();
    }
    if (f.enabled) {
        ImGui::Checkbox(f.enableLabel, f.enabled);
        ImGui::Separator();
    }
    ImGui::BeginDisabled(f.enabled && !*f.enabled);
    f.drawMenu();
    ImGui::EndDisabled();
}

} // namespace

void DrawMenu(bool* open)
{
    ImGui::SetNextWindowSize(ImVec2(420, 0), ImGuiCond_FirstUseEver);
    // "###<name>" keeps the window ID stable while the title text changes.
    const bool dirty = ConfigDirty();
    char title[96];
    snprintf(title, sizeof(title), "%s v%s%s###%s", kAppName, kAppVersion, dirty ? " *" : "", kAppName);
    if (!ImGui::Begin(title, open)) { ImGui::End(); return; }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("%.0f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);

    // Save / Reload, right-aligned on the FPS row above the tabs.
    {
        const ImGuiStyle& st = ImGui::GetStyle();
        const float saveW = ImGui::CalcTextSize("Save").x + st.FramePadding.x * 2.0f;
        const float reloadW = ImGui::CalcTextSize("Reload").x + st.FramePadding.x * 2.0f;
        const float unsavedW = dirty ? ImGui::CalcTextSize("* unsaved").x + st.ItemSpacing.x : 0.0f;
        ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - saveW - reloadW - unsavedW - st.ItemSpacing.x);
        if (dirty) {
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "* unsaved");
            ImGui::SameLine();
        }
        if (dirty) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.45f, 0.10f, 1.0f));
        if (ImGui::Button("Save")) ConfigSave();
        if (dirty) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save to %s", AppFileName(".ini").c_str());
        ImGui::SameLine();
        if (ImGui::Button("Reload")) ConfigLoad();   // discards unsaved edits
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Discard unsaved changes and re-read %s", AppFileName(".ini").c_str());
    }

    if (ImGui::BeginTabBar("##features")) {
        for (const Feature* f : feature::All()) {
            if (!f->title || !f->drawMenu) continue;
            if (ImGui::BeginTabItem(f->title)) {
                DrawFeaturePage(*f);
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    // Ctrl+click lets a slider take any typed value; keep each setting inside its ini range
    // (e.g. an FPS cap of 0). Runs before the frame limiter in the same Present.
    ConfigClamp();

    ImGui::End();
}

void DrawFps()
{
    ImGui::SetNextWindowPos(ImVec2(8, 8));
    ImGui::Begin("##fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                   ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                   ImGuiWindowFlags_NoBackground);   // text only: no fill, no border
    char fps[16];
    snprintf(fps, sizeof(fps), "%.0f", ImGui::GetIO().Framerate);
    const ImVec2 p = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(ImVec2(p.x + 1.0f, p.y + 1.0f), IM_COL32(0, 0, 0, 220), fps);   // drop shadow
    ImGui::TextUnformatted(fps);
    ImGui::End();
}

// Startup banner: shown at the top of the screen for a few seconds after the overlay loads.
void DrawBanner()
{
    constexpr double kShow = 10.0, kFade = 1.0;
    static const double start = ImGui::GetTime();   // first call = first frame after init
    const double t = ImGui::GetTime() - start;
    if (t >= kShow) return;

    const float alpha = t > kShow - kFade ? (float)((kShow - t) / kFade) : 1.0f;
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 12.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    ImGui::SetNextWindowBgAlpha(0.55f * alpha);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
    ImGui::Begin("##banner", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
    ImGui::Text("%s v%s loaded!  Press %s to open the menu.", kAppName, kAppVersion, keybind::Name(cfg.menu_key).c_str());
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace overlay
