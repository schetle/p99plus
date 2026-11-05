#pragma once
// Mouse state collected from the game's DirectInput device, and the rules for what the game
// gets to see of it.
namespace input {

enum class Axis { X, Y, Z };

// ---- raw state (mouse_state.cpp) ----------------------------------------------------------
void SetButton(int i, bool down);
bool Button(int i);

// Movement taken from the game while the menu is open; drives the menu's cursor.
void MenuAdd(long dx, long dy, long dz);
void MenuTake(long& dx, long& dy, long& dz);

// ---- routing (router.cpp) -------------------------------------------------------------------
// Where one mouse movement goes. Returns true when the game must not see it (it was taken by
// the menu or the camera orbit); otherwise `delta` is what the game should get.
bool RouteAxis(Axis axis, long& delta);

// True when the game must not see this button (the middle button belongs to the camera orbit).
bool HidesButton(int i);

} // namespace input
