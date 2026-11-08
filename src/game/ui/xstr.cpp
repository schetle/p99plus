#include "game/ui/xstr.h"
#include "game/eqgame.h"
#include "util/mem.h"
#include <windows.h>

namespace game {

namespace {
using XStrCtor_t = void (__thiscall*)(void* slot, const char* text);
using XStrCtorWide_t = void (__thiscall*)(void* slot, const wchar_t* text);
using XStrFree_t = void (__thiscall*)(void* slot, uintptr_t rep);
bool Ascii(const char* text)
{
    for (const char* p = text; *p; ++p)
        if ((unsigned char)*p >= 0x80) return false;
    return true;
}

} // namespace

std::string XStrToString(uintptr_t slot, int maxLength)
{
    const uintptr_t rep = mem::Read<uintptr_t>(slot);
    if (rep < 0x10000) return {};
    const int len = mem::Read<int>(rep + xstr::kLength);
    if (len <= 0 || len > maxLength) return {};
    if (mem::Read<int>(rep + xstr::kEncoding) != 1) return std::string((const char*)(rep + xstr::kChars), (size_t)len);
    // UTF-16: into the game's own 8-bit code page, which is what it shows and what we hand back.
    const wchar_t* wide = (const wchar_t*)(rep + xstr::kChars);
    const int n = WideCharToMultiByte(CP_ACP, 0, wide, len, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_ACP, 0, wide, len, &out[0], n, nullptr, nullptr);
    return out;
}

XStr::XStr(const char* text)
{
    // The game draws an 8-bit string's bytes over 0x7f as U+FFFD (a box), so text with any goes in
    // as UTF-16, from the game's own 8-bit code page (what XStrToString gives back).
    if (!text || Ascii(text)) {
        ((XStrCtor_t)addr::kXStrCtor)(&slot_, text);
        return;
    }
    const int n = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);   // with the terminating 0
    if (n <= 0) {
        ((XStrCtor_t)addr::kXStrCtor)(&slot_, text);
        return;
    }
    std::wstring wide((size_t)n, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, &wide[0], n);
    ((XStrCtorWide_t)addr::kXStrCtorWide)(&slot_, wide.c_str());
}

void FreeXStr(uintptr_t& slot)
{
    if (slot) ((XStrFree_t)addr::kXStrFree)(&slot, slot);
    slot = 0;
}

XStr::~XStr()
{
    FreeXStr(slot_);
}

} // namespace game
