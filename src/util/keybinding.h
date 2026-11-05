#pragma once
// A keyboard shortcut (key plus exact Ctrl/Shift/Alt state), its ini text, and an in-game
// "press a key" capture.
#include <windows.h>
#include <string>

struct KeyBinding {
    int vk = 0;      // virtual-key code
    int mods = 0;    // keybind::kCtrl | kShift | kAlt that must be held (exact match)
};

namespace keybind {

enum { kCtrl = 1, kShift = 2, kAlt = 4 };

bool KeyDown(int vk);
int  HeldMods();
bool IsDown(const KeyBinding& k);              // key held with exactly the bound modifiers

std::string Name(const KeyBinding& k);         // for people: "Ctrl+Shift+O", "Insert"
std::string Format(const KeyBinding& k);       // for the ini: "ctrl+shift+0x4F"
bool        Parse(const std::string& text, KeyBinding& out);   // false (out untouched) if invalid

// Capture: the next non-modifier key pressed (with whatever modifiers are held) is stored in
// `target`; Escape cancels. Poll once per frame while capturing.
void BeginCapture(KeyBinding* target);
bool Capturing();
void CancelCapture();
bool PollCapture(HWND gameWindow);             // true on the frame a binding was just set

} // namespace keybind
