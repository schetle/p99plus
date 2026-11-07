// The "Overlay" settings and menu section.
#include "overlay/overlay.h"
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

    ui::KeyBindingRow("Menu toggle key", cfg.menu_key, Settings{}.menu_key);
    ui::Hint("Click, then press a key or combo.");

    ImGui::Spacing();
    ImGui::Separator();
    if (ImGui::Button("Reload core")) g_host->requestReload();
    ui::Hint("Also happens automatically when the DLL changes.");
}

} // namespace

const Feature feature = {
    "Overlay",
    nullptr,
    nullptr, nullptr,   // always on
    kSettings, std::size(kSettings), nullptr, DrawMenu, Shutdown,
};

} // namespace overlay
