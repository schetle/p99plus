// FOV control by driving the engine's own camera FOV (so name tags, spell effects etc. agree).
//
// The client designs for 4:3 and keeps its *horizontal* FOV fixed, so on a wider screen the
// vertical FOV shrinks and the top/bottom get cropped. Patching only the D3D projection fixes
// the 3D scene but not the things the game positions itself with its own camera numbers.
//
// The projection builder reads two floats from the camera object (game/eqgfx.h):
//     this+4 = half-FOV in EQ angle units (512 per circle; 45.0 = 31.6 deg)
//     this+8 = aspect ratio (from the game's resolution)
//     projection scale sx = 1 / tan(half-FOV * 2*pi/512),  sy = sx * aspect
// We rewrite the half-FOV before every build:
//   fov == 0:  Hor+. The vertical FOV stays what it is at the 4:3 design aspect and only the
//              horizontal FOV grows with the screen.
//   fov  > 0:  exactly that vertical FOV (first-person or third-person setting).
// The edit is left in the object, so any other code that reads the camera sees the same numbers.
// When the feature is switched off the game's own value is put back.
#include "features/camera/internal.h"
#include "game/eqgame.h"
#include "game/eqgfx.h"

namespace camera {

namespace {
constexpr float kDesignAspect = 4.0f / 3.0f;    // aspect the client's FOV was designed for
constexpr float kFovTolerance = 1e-3f;          // how close to what we wrote counts as "ours"
} // namespace

void ApplyFov(void* cam)
{
    float& half = gfx::HalfFov(cam);
    const float aspect = gfx::Aspect(cam);
    ReadBack<1>& rb = Record(cam).fov;

    // Did the game set a fresh value (not ours)?
    if (!rb.IsOurs(&half, kFovTolerance) && half > 1.0f && half < 120.0f) rb.SetNative(&half);
    if (!rb.valid || aspect < 0.5f || aspect > 8.0f) return;

    if (!cfg.fov.enabled) {
        if (rb.IsOurs(&half, kFovTolerance)) half = rb.native[0];   // hand the game its own value back
        return;
    }

    const float deg = game::IsFirstPerson() ? cfg.fov.first_person : cfg.fov.third_person;
    float sx;
    if (deg >= 1.0f && deg <= 170.0f) {
        const float sy = 1.0f / std::tan(deg * 0.5f * math::kDegToRad);
        sx = sy / aspect;
    } else {
        sx = 1.0f / std::tan(rb.native[0] * math::kEqUnitRad);
        if (aspect > kDesignAspect + 0.01f) sx *= kDesignAspect / aspect;
    }
    const float newHalf = math::Clamp(std::atan(1.0f / sx) / math::kEqUnitRad, 1.0f, 127.0f);
    half = newHalf;
    rb.SetWritten(&newHalf);
}

} // namespace camera
