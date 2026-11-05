#pragma once
// Shared between the camera feature's source files; nothing outside features/camera includes this.
#include "features/camera/camera.h"
#include "util/math.h"
#include "util/readback.h"
#include <cstdint>
#include <d3d9.h>

namespace camera {

// ---- the view and projection the game last set through Direct3D (view_tracker.cpp) ----------
namespace view {
void NoteView(const D3DMATRIX& m);
void NoteProjection(const D3DMATRIX& m);
const D3DMATRIX* View();           // nullptr until the game has set one
const D3DMATRIX* Projection();
// The camera's right and forward vectors in world space; false until a view is known.
bool Axes(math::Vec3& right, math::Vec3& forward);
// True once the engine camera hooks are driving the camera (the Direct3D fallback then stands down).
void SetEngineActive();
bool EngineActive();
} // namespace view

// ---- per engine camera object (records.cpp) ---------------------------------------------------
struct CameraRecord {
    ReadBack<1> fov;       // half-FOV
    ReadBack<3> eye;
    ReadBack<3> ang;       // heading, pitch, roll
    bool main = false;     // the world camera (near the player), as opposed to e.g. a UI preview camera
};
CameraRecord& Record(void* cam);

// ---- eye offset (offset.cpp) -------------------------------------------------------------------
// `in` is the eye the game set or the one currently stored; `native` receives the real eye and
// `out` the eye to store. After a successful store, call EyeCommit.
void EyeOffset(void* cam, const float in[3], float native[3], float out[3]);
void EyeCommit(void* cam, const float native[3], const float out[3]);
// World-space offset for the current camera mode. False when there is none.
bool ComputeEyeOffset(bool firstPerson, float zoomScale, math::Vec3& d);

// ---- orbit (orbit.cpp) ---------------------------------------------------------------------------
namespace orbit {
bool Active();          // the orbit is currently turned away from the default view
bool Ready();           // heading direction learned, so the orbit may be used
// Turn the finished eye by the orbit, unless the game already placed the orbited eye this frame.
void ApplyToEye(float eye[3], bool firstPerson);
// The game's own camera placement (game_hooks.cpp). Between Begin and End, RotateDesiredEye turns
// the eye the camera controller asks for (once) before the game sweeps it for collision.
void BeginGameCamera(uintptr_t spawn);
void RotateDesiredEye(float to[3]);
void EndGameCamera();
// Same for the camera's angles (heading, pitch, roll; 512 per circle). Call after EyeOffset.
void AngOffset(void* cam, const float in[3], float native[3], float out[3]);
void AngCommit(void* cam, const float native[3], const float out[3]);
// After the builder ran with unmodified angles: learn which way heading/pitch turn the view.
void LearnAngles(const float native[3]);
void OnFrame();         // once per Present: mouse input and swing-back
} // namespace orbit

// ---- FOV (fov.cpp) ---------------------------------------------------------------------------------
void ApplyFov(void* cam);   // rewrite the camera's half-FOV; call before the builder runs

// ---- engine camera hooks (engine_hooks.cpp) -------------------------------------------------------
void EngineHooksOnFrame();

// ---- eqgame.exe camera placement hooks (game_hooks.cpp) -------------------------------------------
void GameHooksOnFrame();

} // namespace camera
