#include "game/actions.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "util/hook.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace game::actions {

namespace {

using SpellAction_t = int (__thiscall*)(void* target, const unsigned char* action);
SpellAction_t oSpellAction = nullptr;

std::vector<Listener> s_listeners;
bool s_tried = false;
bool s_ok = false;

// The action as the packet has it (31 bytes, packed): target id u16 +0, source id u16 +2, caster
// level u16 +4, type u8 +0x16, spell id u16 +0x1b, flags u8 +0x1e. The client's own actions are
// zeroed first and fill in only target, source, type, spell and the motion floats.
Action Decode(const unsigned char* p)
{
    unsigned short target, source, level, spell;
    memcpy(&target, p + 0x0, 2);
    memcpy(&source, p + 0x2, 2);
    memcpy(&level, p + 0x4, 2);
    memcpy(&spell, p + 0x1b, 2);
    return { target, source, level, p[0x16], spell, p[0x1e] };
}

int __fastcall Hk_SpellAction(void* target, void* /*edx*/, const unsigned char* action)
{
    hook::Busy busy;
    if (action) {
        const Action a = Decode(action);
        for (Listener fn : s_listeners) fn(a);
    }
    return oSpellAction(target, action);
}

} // namespace

bool Init()
{
    if (s_tried) return s_ok;
    if (!game::Available()) return false;
    s_tried = true;
    s_ok = hook::Install("spell action", (void*)addr::kSpellAction, (void*)&Hk_SpellAction, (void**)&oSpellAction,
                         kSpellActionPrologue) == hook::Result::Ok;
    if (!s_ok) Log("actions: unavailable");
    return s_ok;
}

bool Ready() { return s_ok; }

void Listen(Listener fn)
{
    if (std::find(s_listeners.begin(), s_listeners.end(), fn) == s_listeners.end()) s_listeners.push_back(fn);
}

} // namespace game::actions
