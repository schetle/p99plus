#include "game/ui/xstr.h"
#include "game/eqgame.h"
#include "util/mem.h"

namespace game {

namespace {
using XStrCtor_t = void (__thiscall*)(void* slot, const char* text);
using XStrFree_t = void (__thiscall*)(void* slot, uintptr_t rep);
} // namespace

std::string XStrToString(uintptr_t slot)
{
    const uintptr_t rep = mem::Read<uintptr_t>(slot);
    if (rep < 0x10000) return {};
    const int len = mem::Read<int>(rep + xstr::kLength);
    if (len <= 0 || len > 512) return {};
    return std::string((const char*)(rep + xstr::kChars), (size_t)len);
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
