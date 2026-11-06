#include "game/strings.h"
#include "game/eqgame.h"
#include "util/mem.h"

namespace game {

const char* String(int id)
{
    if (!Available()) return nullptr;
    using GetString_t = const char* (__thiscall*)(void* table, int id, int flags);
    void* table = mem::Read<void*>(addr::kStringTable);
    if (!table) return nullptr;
    const char* text = ((GetString_t)addr::kGetString)(table, id, 0);
    return (text && *text) ? text : nullptr;
}

} // namespace game
