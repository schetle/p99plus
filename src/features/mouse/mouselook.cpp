// Patches and hooks inside eqgame.exe's mouselook (see game/eqgame.h for the addresses).
//
// Mouselook function (reads the per-frame mouse deltas):
//   - pitch:   player+0x5c -= dy * k            (applied directly, FPS independent)
//   - heading: player+0x58  = -(dx * k), +-8    (a turn *speed*, integrated later with frame time,
//                                                 which is why horizontal turning shrinks at high FPS)
//   - an axis dead-zone throws away pitch when |dy/dx| < 0.2 and heading when |dx/dy| < 0.2.
//     At high FPS the per-frame integer deltas are tiny, so this fires at random on diagonal moves
//     and the pitch snaps. Patching the two `jp` after the ratio test into `jmp` disables it.
//
// Measured (2026-11-03): the speed is reset every frame and integrated as
//   heading += speed * elapsed_ms * 0.02          (elapsed in whole milliseconds)
// so each frame's mouse dx turns by dx * frame time, and total turning falls as FPS rises.
// Direct turn: after the game sets the speed, apply it to the heading ourselves at a fixed
// 60 FPS step (16.67 ms) and zero the speed, so turning per mouse count no longer depends on FPS.

#include "features/mouse/mouse.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "util/hook.h"
#include "util/math.h"
#include "util/mem.h"

namespace mouse {

namespace {

using Mouselook_t = void (__thiscall*)(void* self, int arg);
Mouselook_t oMouselook = nullptr;

bool s_hookTried = false;
int  s_deadzoneApplied = -1;   // -1 unknown, 0 original, 1 patched

void __fastcall Hk_Mouselook(void* self, void* /*edx*/, int arg)
{
    hook::Busy busy;
    if (!cfg.enabled || !cfg.fps_independent_turn) { oMouselook(self, arg); return; }

    const uintptr_t p = game::PlayerBase();
    const float speedBefore = p ? mem::Read<float>(p + game::spawn::kTurnSpeed) : 0.0f;
    const long dx = mem::Read<long>(game::addr::kMouseDX);

    oMouselook(self, arg);

    if (!p || dx == 0) return;
    const float speed = mem::Read<float>(p + game::spawn::kTurnSpeed);
    if (speed == speedBefore) return;   // the mouse didn't set the player's turn speed this frame

    const float heading = mem::Read<float>(p + game::spawn::kHeading) + (speed - speedBefore) * game::kTurnPerSpeed;
    mem::Write<float>(p + game::spawn::kHeading, math::WrapEq(heading));
    mem::Write<float>(p + game::spawn::kTurnSpeed, speedBefore);
}

} // namespace

void ApplyGamePatches()
{
    if (!game::Available()) return;

    if (!s_hookTried) {
        s_hookTried = true;
        if (hook::Install("mouselook", (void*)game::addr::kMouselook, (void*)&Hk_Mouselook,
                          (void**)&oMouselook, game::kMouselookPrologue) != hook::Result::Ok)
            Log("mouselook: frame-rate independent mouselook unavailable");
    }

    const int want = cfg.enabled && cfg.remove_deadzone ? 1 : 0;
    if (want != s_deadzoneApplied) {
        const unsigned char op = want ? 0xeb : 0x7a;   // jmp : jp
        mem::PatchCode((void*)game::addr::kDeadzoneYaw, &op, 1);
        mem::PatchCode((void*)game::addr::kDeadzonePit, &op, 1);
        s_deadzoneApplied = want;
        Log("mouselook: axis dead-zone %s", want ? "removed" : "restored");
    }
}

} // namespace mouse
