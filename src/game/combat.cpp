#include "game/combat.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "util/hook.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace game::combat {

namespace {

using HitReport_t = int (__thiscall*)(void* self, const unsigned char* hit, int show, int extra);
HitReport_t oHitReport = nullptr;

std::vector<Listener> s_listeners;
bool s_tried = false;
bool s_ok = false;

// The hit as the client keeps it (packed): target id u16 +0, source id u16 +2, type u8 +4,
// spell id u16 +5, damage i32 +7.
Hit Decode(const unsigned char* p, bool printed)
{
    unsigned short target, source, spell;
    int damage;
    memcpy(&target, p + 0, 2);
    memcpy(&source, p + 2, 2);
    memcpy(&spell, p + 5, 2);
    memcpy(&damage, p + 7, 4);
    return { target, source, p[4], spell, damage, printed };
}

int __fastcall Hk_HitReport(void* self, void* /*edx*/, const unsigned char* hit, int show, int extra)
{
    hook::Busy busy;
    if (hit) {
        // `show` is a bool: the callers set only its low byte and push the whole stack slot, so
        // the upper bytes are leftovers (usually nonzero).
        const Hit h = Decode(hit, (show & 0xff) != 0);
        for (Listener fn : s_listeners) fn(h);
    }
    return oHitReport(self, hit, show, extra);
}

} // namespace

bool Init()
{
    if (s_tried) return s_ok;
    if (!game::Available()) return false;
    s_tried = true;
    s_ok = hook::Install("combat", (void*)addr::kHitReport, (void*)&Hk_HitReport, (void**)&oHitReport,
                         kHitReportPrologue) == hook::Result::Ok;
    if (!s_ok) Log("combat: unavailable");
    return s_ok;
}

bool Ready() { return s_ok; }

void Listen(Listener fn)
{
    if (std::find(s_listeners.begin(), s_listeners.end(), fn) == s_listeners.end()) s_listeners.push_back(fn);
}

} // namespace game::combat
