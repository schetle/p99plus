#include "input/input.h"
#include <windows.h>

namespace input {

namespace {
volatile LONG s_menuX = 0, s_menuY = 0, s_menuZ = 0;
volatile LONG s_buttons = 0;   // bitmask
} // namespace

void SetButton(int i, bool down)
{
    if (i < 0 || i >= 8) return;
    if (down) InterlockedOr(&s_buttons, 1L << i);
    else      InterlockedAnd(&s_buttons, ~(1L << i));
}

bool Button(int i) { return i >= 0 && i < 8 && (s_buttons & (1L << i)); }

void MenuAdd(long dx, long dy, long dz)
{
    InterlockedExchangeAdd(&s_menuX, dx);
    InterlockedExchangeAdd(&s_menuY, dy);
    InterlockedExchangeAdd(&s_menuZ, dz);
}

void MenuTake(long& dx, long& dy, long& dz)
{
    dx = InterlockedExchange(&s_menuX, 0);
    dy = InterlockedExchange(&s_menuY, 0);
    dz = InterlockedExchange(&s_menuZ, 0);
}

} // namespace input
