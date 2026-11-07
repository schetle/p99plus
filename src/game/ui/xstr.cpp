#include "game/ui/xstr.h"
#include "game/eqgame.h"
#include "util/mem.h"
#include <windows.h>

namespace game {

namespace {
using XStrCtor_t = void (__thiscall*)(void* slot, const char* text);
using XStrFree_t = void (__thiscall*)(void* slot, uintptr_t rep);
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
    ((XStrCtor_t)addr::kXStrCtor)(&slot_, text);
}

XStr::~XStr()
{
    if (slot_) ((XStrFree_t)addr::kXStrFree)(&slot_, slot_);
    slot_ = 0;
}

} // namespace game
