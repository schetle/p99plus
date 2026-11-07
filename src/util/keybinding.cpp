#include "util/keybinding.h"
#include "core/log.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace keybind {

namespace {

KeyBinding* s_target = nullptr;
bool s_prev[256] = {};

bool IsModifierVk(int vk)
{
    switch (vk) {
    case VK_SHIFT: case VK_CONTROL: case VK_MENU: case VK_LSHIFT: case VK_RSHIFT:
    case VK_LCONTROL: case VK_RCONTROL: case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN:
        return true;
    }
    return false;
}

} // namespace

bool KeyDown(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }

int HeldMods()
{
    return (KeyDown(VK_CONTROL) ? kCtrl : 0) | (KeyDown(VK_SHIFT) ? kShift : 0) | (KeyDown(VK_MENU) ? kAlt : 0);
}

bool IsDown(const KeyBinding& k) { return KeyDown(k.vk) && HeldMods() == k.mods; }

std::string Name(const KeyBinding& k)
{
    std::string s;
    if (k.mods & kCtrl)  s += "Ctrl+";
    if (k.mods & kShift) s += "Shift+";
    if (k.mods & kAlt)   s += "Alt+";

    LONG lp = (LONG)(MapVirtualKeyA(k.vk, MAPVK_VK_TO_VSC) << 16);
    switch (k.vk) {   // these share scan codes with the numpad; the extended bit picks the right name
    case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
    case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_NUMLOCK: case VK_DIVIDE:
    case VK_RCONTROL: case VK_RMENU:
        lp |= 1 << 24;
        break;
    }
    char name[64];
    if (GetKeyNameTextA(lp, name, sizeof(name)) > 0) return s + name;
    snprintf(name, sizeof(name), "VK 0x%X", k.vk);
    return s + name;
}

std::string Format(const KeyBinding& k)
{
    std::string s;
    if (k.mods & kCtrl)  s += "ctrl+";
    if (k.mods & kShift) s += "shift+";
    if (k.mods & kAlt)   s += "alt+";
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%X", k.vk);
    return s + buf;
}

bool Parse(const std::string& text, KeyBinding& out)
{
    KeyBinding k;
    size_t pos = 0;
    for (;;) {
        size_t plus = text.find('+', pos);
        std::string tok = text.substr(pos, plus == std::string::npos ? std::string::npos : plus - pos);
        while (!tok.empty() && tok.front() == ' ') tok.erase(0, 1);
        while (!tok.empty() && tok.back() == ' ') tok.pop_back();
        for (char& c : tok) c = (char)tolower((unsigned char)c);

        if (plus != std::string::npos) {                      // a modifier
            if (tok == "ctrl")       k.mods |= kCtrl;
            else if (tok == "shift") k.mods |= kShift;
            else if (tok == "alt")   k.mods |= kAlt;
            else return false;
            pos = plus + 1;
            continue;
        }
        char* end = nullptr;                                   // the key itself
        long vk = strtol(tok.c_str(), &end, 0);
        if (tok.empty() || *end || vk < 0x08 || vk > 0xFE) return false;
        k.vk = (int)vk;
        out = k;
        return true;
    }
}

void BeginCapture(KeyBinding* target)
{
    s_target = target;
    for (int vk = 0; vk < 256; ++vk) s_prev[vk] = KeyDown(vk);
}

bool Capturing() { return s_target != nullptr; }
void CancelCapture() { s_target = nullptr; }

bool PollCapture(HWND gameWindow)
{
    if (!s_target || GetForegroundWindow() != gameWindow) return false;
    for (int vk = 0x08; vk <= 0xFE; ++vk) {
        const bool down = KeyDown(vk);
        const bool pressed = down && !s_prev[vk];
        s_prev[vk] = down;
        if (!pressed || IsModifierVk(vk)) continue;

        KeyBinding* t = s_target;
        s_target = nullptr;
        if (vk == VK_ESCAPE) return false;
        t->vk = vk;
        t->mods = HeldMods();
        Log("key binding set to %s (%s)", Name(*t).c_str(), Format(*t).c_str());
        return true;
    }
    return false;
}

bool Press::Poll(const KeyBinding& k, bool allowed)
{
    const bool down = allowed && IsDown(k);
    const bool capturing = Capturing();
    const bool pressed = down && !prev_ && !capturing && !wasCapturing_;
    prev_ = down;
    wasCapturing_ = capturing;
    return pressed;
}

} // namespace keybind
