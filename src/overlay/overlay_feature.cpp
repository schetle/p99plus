// The "Overlay" settings and menu section.
#include "overlay/overlay.h"
#include "core/app.h"
#include "core/host.h"
#include "overlay/widgets.h"
#include <imgui.h>
#include <iterator>

namespace overlay {

Settings cfg;

namespace {

const Setting kSettings[] = {
    KeySetting  ("overlay", "menu_key", &cfg.menu_key, "Key that shows/hides the menu: optional ctrl+/shift+/alt+ prefixes, then a virtual-key code (0x2D = Insert)."),
    BoolSetting ("overlay", "show_fps", &cfg.show_fps, "Show a small FPS counter while the menu is hidden."),
    FloatSetting("overlay", "cursor_speed", &cfg.cursor_speed, 0.1f, 20.0f, "Menu cursor speed in pixels per mouse count."),
};

void DrawMenu()
{
    ImGui::Checkbox("Show FPS when menu hidden", &cfg.show_fps);
    ui::SliderF("Cursor speed", &cfg.cursor_speed, 0.1f, 5.0f, "%.2f");

    const bool capturing = keybind::Capturing();
    const std::string label = (capturing ? "Press a key... (Esc cancels)" : keybind::Name(cfg.menu_key)) + "###menukey";
    ui::RowLabel("Menu toggle key");
    ImGui::SetNextItemWidth(0);
    if (ImGui::Button(label.c_str()) && !capturing) {
        ImGui::GetIO().ClearInputKeys();   // keyboard feed pauses during capture; don't leave keys held
        keybind::BeginCapture(&cfg.menu_key);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(capturing);
    if (ImGui::Button("Reset key")) cfg.menu_key = Settings{}.menu_key;
    ImGui::EndDisabled();
    ui::Hint("Click, then press a key. Hold Ctrl, Shift or Alt for a combo.");

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Reload core")) g_host->requestReload();
    ui::Hint("Loads %s again without restarting the game; it also reloads by itself when the file "
             "changes. Settings, including unsaved ones, are kept. Loaded %d time(s) this session.",
             AppFileName("_core.dll").c_str(), g_host->loadCount);
}

} // namespace

const Feature feature = {
    "Overlay",
    "Settings for this menu: the key that opens it, the FPS counter and the menu cursor speed.",
    nullptr, nullptr,   // always on
    kSettings, std::size(kSettings), nullptr, DrawMenu, Shutdown,
};

} // namespace overlay
