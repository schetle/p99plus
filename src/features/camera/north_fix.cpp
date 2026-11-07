// The game turns degrees into headings as degrees / 360 * 511.5 (game::addr::kHeadingScale), where
// a full circle is 512. The camera controllers point the camera with that function, so the camera
// heading is always 0.1 % short of the character's, and the gap (half a unit near north) closes all
// at once when the heading wraps through 0: the view jumps. Writing 512 there removes the jump;
// every other caller (facing a target, for one) gets the exact heading too.
#include "features/camera/internal.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "util/mem.h"

namespace camera {

namespace {
int s_applied = -1;     // -1 unknown, 0 the game's value, 1 fixed
bool s_checked = false;
bool s_ok = false;      // the constant is where and what we expect
} // namespace

void NorthFixOnFrame()
{
    if (!game::Available()) return;
    if (!s_checked) {
        s_checked = true;
        s_ok = mem::Read<float>(game::addr::kHeadingScale) == game::addr::kHeadingScaleGame;
        if (!s_ok) Log("camera: unexpected heading scale %g; north jump fix unavailable",
                       mem::Read<float>(game::addr::kHeadingScale));
    }
    if (!s_ok) return;
    const int want = cfg.fix_north_jump ? 1 : 0;
    if (want == s_applied) return;
    const float v = want ? 512.0f : game::addr::kHeadingScaleGame;
    mem::PatchCode((void*)game::addr::kHeadingScale, &v, sizeof(v));
    s_applied = want;
    Log("camera: heading scale %s", want ? "fixed (512)" : "back to the game's 511.5");
}

} // namespace camera
