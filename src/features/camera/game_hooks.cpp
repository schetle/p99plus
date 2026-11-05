// Hooks into eqgame.exe's own camera placement (addresses in game/eqgame.h), so the orbit is
// applied before the game's camera collision instead of after it:
//   - the camera update: marks the span in which the current mode's camera controller builds
//     its eye around the character and hands it to the engine camera
//   - CollideLine: inside that span, the eye the controller wants is turned by the orbit
//     before the game sweeps it, so walls and terrain pull the orbited camera in as usual
// Without these hooks the orbit still works, turning the finished eye (no collision).
#include "features/camera/internal.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "util/hook.h"

namespace camera {

namespace {

using CameraUpdate_t = int (__fastcall*)(void* spawn, void* edx);
using CollideLine_t  = bool (__fastcall*)(void* display, void* edx, float fx, float fy, float fz,
                                          float tx, float ty, float tz, float* ox, float* oy, float* oz, int flags);

CameraUpdate_t oCameraUpdate = nullptr;
CollideLine_t  oCollideLine = nullptr;
bool s_tried = false;

int __fastcall Hk_CameraUpdate(void* spawn, void* edx)
{
    orbit::BeginGameCamera((uintptr_t)spawn);
    const int r = oCameraUpdate(spawn, edx);
    orbit::EndGameCamera();
    return r;
}

bool __fastcall Hk_CollideLine(void* display, void* edx, float fx, float fy, float fz,
                               float tx, float ty, float tz, float* ox, float* oy, float* oz, int flags)
{
    float to[3] = { tx, ty, tz };
    orbit::RotateDesiredEye(to);
    return oCollideLine(display, edx, fx, fy, fz, to[0], to[1], to[2], ox, oy, oz, flags);
}

} // namespace

void GameHooksOnFrame()
{
    if (s_tried || !game::Available()) return;
    s_tried = true;
    using namespace game;
    if (hook::Install("camera update", (void*)addr::kCameraUpdate, (void*)&Hk_CameraUpdate,
                      (void**)&oCameraUpdate, kCameraUpdatePrologue) != hook::Result::Ok ||
        hook::Install("camera collision", (void*)addr::kCollideLine, (void*)&Hk_CollideLine,
                      (void**)&oCollideLine, kCollideLinePrologue) != hook::Result::Ok)
        Log("camera: orbit ignores camera collision");
}

} // namespace camera
