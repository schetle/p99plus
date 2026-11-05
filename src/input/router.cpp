// Decides what the game sees of the mouse. Both DirectInput read paths (immediate state and
// buffered data) go through here so they can't drift apart:
//   1. menu open          -> the menu takes everything (it drives the ImGui cursor)
//   2. middle button held -> the camera orbit takes X/Y
//   3. otherwise          -> the game gets it, scaled by the sensitivity settings
#include "input/input.h"
#include "features/camera/camera.h"
#include "features/mouse/mouse.h"
#include "overlay/overlay.h"

namespace input {

bool RouteAxis(Axis axis, long& delta)
{
    if (overlay::Visible()) {
        MenuAdd(axis == Axis::X ? delta : 0, axis == Axis::Y ? delta : 0, axis == Axis::Z ? delta : 0);
        return true;
    }
    if (axis != Axis::Z && camera::orbit::Dragging()) {
        camera::orbit::AddMouse(axis == Axis::X ? delta : 0, axis == Axis::Y ? delta : 0);
        return true;
    }
    if (axis == Axis::X)      delta = mouse::FilterX(delta);
    else if (axis == Axis::Y) delta = mouse::FilterY(delta);
    return false;
}

bool HidesButton(int i) { return i == 2 && camera::orbit::Enabled(); }

} // namespace input
