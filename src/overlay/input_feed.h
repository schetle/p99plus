#pragma once
// Feeds ImGui from DirectInput and polled keys. EQ's window gets neither useful mouse messages
// (the OS cursor is pinned under some compositors) nor WM_CHAR, so the menu's mouse and
// keyboard are driven from here. Both must run after ImGui_ImplWin32_NewFrame so our events
// are queued last and win.
#include <windows.h>

namespace overlay::feed {

void Reset();                    // the menu just opened: drop stale input, center the cursor
void Mouse();                    // virtual cursor from the movement input::RouteAxis diverted
void Keyboard(HWND gameWindow);  // modifiers, navigation keys and typed characters

} // namespace overlay::feed
