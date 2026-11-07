#pragma once
// The ImGui overlay: lifecycle, the menu toggle, and the "Overlay" menu section.
#include "core/feature.h"
#include "util/keybinding.h"
#include <windows.h>

struct IDirect3DDevice9;

namespace overlay {

struct Settings {
    KeyBinding menu_key = { VK_INSERT, 0 };   // shows/hides the menu
    bool  show_fps = false;                   // tiny FPS counter while the menu is hidden
    float cursor_speed = 1.0f;                // menu cursor pixels per DirectInput count
};
extern Settings cfg;
extern const Feature feature;

// While the menu is open the game gets no keyboard or mouse input.
bool Visible();
void SetVisible(bool visible);

// A ring drawn around a rectangle of the screen this frame (over the game's UI). Features call it
// from onFrame, which runs before the overlay draws.
void Highlight(float left, float top, float right, float bottom, unsigned int rgba);

// Called from the Direct3D hooks (render/dispatch.cpp).
void OnPresent(IDirect3DDevice9* dev);
void PreReset();
void PostReset();

// The game window's messages, from the host's subclass. True = handled, return *result.
bool WndProc(HWND h, UINT msg, WPARAM w, LPARAM l, LRESULT* result);

// The core is being unloaded: close the menu, give the game its cursor back, tear down ImGui.
void Shutdown();

} // namespace overlay
