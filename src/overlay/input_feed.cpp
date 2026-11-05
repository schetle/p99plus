#include "overlay/input_feed.h"
#include "input/input.h"
#include "overlay/overlay.h"
#include <imgui.h>

namespace overlay::feed {

namespace {
bool  s_prevButton[3] = {};
bool  s_prevKey[256] = {};
bool  s_centerCursor = true;
float s_cx = 0, s_cy = 0;       // virtual cursor
} // namespace

void Reset()
{
    long dx, dy, dz;
    input::MenuTake(dx, dy, dz);                   // discard stale movement
    for (int i = 0; i < 3; ++i) s_prevButton[i] = input::Button(i);
    for (int vk = 0; vk < 256; ++vk) s_prevKey[vk] = keybind::KeyDown(vk);
    s_centerCursor = true;
}

void Mouse()
{
    ImGuiIO& io = ImGui::GetIO();
    long dx, dy, dz;
    input::MenuTake(dx, dy, dz);

    if (s_centerCursor && io.DisplaySize.x > 0) {
        s_cx = io.DisplaySize.x * 0.5f;
        s_cy = io.DisplaySize.y * 0.5f;
        s_centerCursor = false;
    }
    s_cx += dx * cfg.cursor_speed;
    s_cy += dy * cfg.cursor_speed;
    if (s_cx < 0) s_cx = 0;
    if (s_cy < 0) s_cy = 0;
    if (s_cx > io.DisplaySize.x - 1) s_cx = io.DisplaySize.x - 1;
    if (s_cy > io.DisplaySize.y - 1) s_cy = io.DisplaySize.y - 1;
    io.AddMousePosEvent(s_cx, s_cy);

    if (dz) io.AddMouseWheelEvent(0.0f, dz / (float)WHEEL_DELTA);

    for (int i = 0; i < 3; ++i) {
        const bool b = input::Button(i);
        if (b != s_prevButton[i]) { io.AddMouseButtonEvent(i, b); s_prevButton[i] = b; }
    }
}

// ImGui's keyboard (Ctrl+click to type a value, Enter, Backspace...) is polled here.
void Keyboard(HWND gameWindow)
{
    ImGuiIO& io = ImGui::GetIO();
    if (GetForegroundWindow() != gameWindow) return;

    struct K { int vk; ImGuiKey key; };
    static const K keys[] = {
        { VK_LCONTROL, ImGuiKey_LeftCtrl }, { VK_RCONTROL, ImGuiKey_RightCtrl },
        { VK_LSHIFT, ImGuiKey_LeftShift },  { VK_RSHIFT, ImGuiKey_RightShift },
        { VK_LMENU, ImGuiKey_LeftAlt },     { VK_RMENU, ImGuiKey_RightAlt },
        { VK_TAB, ImGuiKey_Tab },   { VK_LEFT, ImGuiKey_LeftArrow }, { VK_RIGHT, ImGuiKey_RightArrow },
        { VK_UP, ImGuiKey_UpArrow }, { VK_DOWN, ImGuiKey_DownArrow },
        { VK_HOME, ImGuiKey_Home }, { VK_END, ImGuiKey_End }, { VK_DELETE, ImGuiKey_Delete },
        { VK_BACK, ImGuiKey_Backspace }, { VK_RETURN, ImGuiKey_Enter }, { VK_ESCAPE, ImGuiKey_Escape },
        { 'A', ImGuiKey_A }, { 'C', ImGuiKey_C }, { 'V', ImGuiKey_V }, { 'X', ImGuiKey_X }, { 'Z', ImGuiKey_Z },
    };
    const bool ctrl = keybind::KeyDown(VK_CONTROL), shift = keybind::KeyDown(VK_SHIFT), alt = keybind::KeyDown(VK_MENU);
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddKeyEvent(ImGuiMod_Shift, shift);
    io.AddKeyEvent(ImGuiMod_Alt, alt);
    for (const K& k : keys) io.AddKeyEvent(k.key, keybind::KeyDown(k.vk));   // repeats of the same state are ignored

    BYTE state[256] = {};
    if (shift) state[VK_SHIFT] = 0x80;
    if (GetKeyState(VK_CAPITAL) & 1) state[VK_CAPITAL] = 0x01;
    for (int vk = 0x20; vk < 0xFF; ++vk) {
        const bool down = keybind::KeyDown(vk);
        const bool pressed = down && !s_prevKey[vk];
        s_prevKey[vk] = down;
        if (!pressed || ctrl || alt || vk == cfg.menu_key.vk) continue;
        WCHAR buf[4];
        const int n = ToUnicode(vk, MapVirtualKeyA(vk, MAPVK_VK_TO_VSC), state, buf, 4, 0);
        for (int i = 0; i < n; ++i)
            if (buf[i] >= 0x20) io.AddInputCharacterUTF16(buf[i]);
    }
}

} // namespace overlay::feed
