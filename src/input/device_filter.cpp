// What the game sees of its mouse and keyboard. The host's DirectInput hooks (host/dinput8.cpp)
// call these after each successful GetDeviceState / GetDeviceData on the game's devices:
// mouse movement goes through RouteAxis, the orbit button is hidden, and everything is blocked
// while the menu is open.
#include "input/input.h"
#include "overlay/overlay.h"
#include <cstring>
#include <dinput.h>

namespace input {

void FilterState(bool mouse, DWORD cb, void* data)
{
    if (!mouse) {
        if (overlay::Visible()) memset(data, 0, cb);
        return;
    }
    if (cb < sizeof(DIMOUSESTATE)) return;

    auto* ms = (DIMOUSESTATE*)data;
    const bool wide = cb >= sizeof(DIMOUSESTATE2);
    BYTE* buttons = wide ? ((DIMOUSESTATE2*)data)->rgbButtons : ms->rgbButtons;
    const int nbtn = wide ? 8 : 4;
    for (int i = 0; i < nbtn; ++i) SetButton(i, (buttons[i] & 0x80) != 0);

    const bool menu = overlay::Visible();
    long x = ms->lX, y = ms->lY, z = ms->lZ;
    ms->lX = RouteAxis(Axis::X, x) ? 0 : x;
    ms->lY = RouteAxis(Axis::Y, y) ? 0 : y;
    ms->lZ = RouteAxis(Axis::Z, z) ? 0 : z;
    if (menu) { memset(data, 0, cb); return; }   // the menu hides buttons too
    for (int i = 0; i < nbtn; ++i)
        if (HidesButton(i)) buttons[i] = 0;
}

void FilterData(bool mouse, DWORD cbObj, DIDEVICEOBJECTDATA* rgdod, DWORD* inOut)
{
    const bool menu = overlay::Visible();

    if (mouse) {
        DWORD kept = 0;
        for (DWORD i = 0; i < *inOut; ++i) {
            auto* e = (DIDEVICEOBJECTDATA*)((BYTE*)rgdod + i * cbObj);
            bool drop = false;
            if (e->dwOfs >= DIMOFS_BUTTON0 && e->dwOfs <= DIMOFS_BUTTON7) {
                const int button = (int)(e->dwOfs - DIMOFS_BUTTON0);
                SetButton(button, (e->dwData & 0x80) != 0);
                drop = HidesButton(button);
            } else if (e->dwOfs == DIMOFS_X || e->dwOfs == DIMOFS_Y || e->dwOfs == DIMOFS_Z) {
                const Axis axis = e->dwOfs == DIMOFS_X ? Axis::X
                                : e->dwOfs == DIMOFS_Y ? Axis::Y : Axis::Z;
                long v = (long)e->dwData;
                drop = RouteAxis(axis, v);
                e->dwData = (DWORD)v;
            }
            if (drop) continue;
            if (kept != i) memmove((BYTE*)rgdod + kept * cbObj, e, cbObj);
            ++kept;
        }
        *inOut = kept;
    }

    if (menu) *inOut = 0;   // events were consumed from the buffer; the game sees none
}

} // namespace input
