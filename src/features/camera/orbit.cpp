// Middle-button camera orbit.
//
// The orbit is a rigid rotation of the camera around where the character stands: the eye is
// rotated about that point and the camera's angles get the same turn, so the character stays put
// on screen.
// Normally the game itself places the orbited eye: inside its camera update, the eye the camera
// controller asks for is turned before the game sweeps it for collision (game_hooks.cpp), so
// walls still pull the camera in. Without those hooks the finished eye is turned afterwards
// (ApplyToEye), and collision only applies to the unorbited direction.
//   s_yaw:   world rotation about +Z in radians (positive = counter-clockwise from above)
//   s_pitch: radians, positive = look up (the eye drops below, looking up at you)
// Which way EQ's heading/pitch numbers turn the view is fixed for this engine build
// (gfx::kHeadingSign, gfx::kPitchSign).
#include "features/camera/internal.h"
#include "core/log.h"
#include "features/mouse/mouse.h"
#include "game/eqgame.h"
#include "game/eqgfx.h"
#include "input/input.h"
#include "overlay/overlay.h"
#include "util/mem.h"
#include "util/timing.h"
#include <cstring>

namespace camera::orbit {

namespace {

using math::Vec3;
using math::kDegToRad;
using math::kEqUnitRad;

volatile LONG s_mouseX = 0, s_mouseY = 0;          // diverted mouse movement, taken once per frame

float s_yaw = 0.0f, s_pitch = 0.0f;
float s_pitchEff = 0.0f;                           // pitch actually applied this frame (clamped)
bool  s_returning = false, s_wasDragging = false;

// The game's camera update (game_hooks.cpp).
uintptr_t s_updSpawn = 0;          // the spawn being placed, while inside the update
bool s_updRotated = false;         // this update's collision sweep got the orbited eye
bool s_gameRotates = false;        // the last update did, so the game's eye already includes the orbit
bool s_addYaw = true, s_addPitch = true;   // the game's angles don't already follow the orbited eye

} // namespace

bool Enabled() { return cfg.orbit.enabled; }
bool Dragging() { return Enabled() && input::Button(2) && !overlay::Visible(); }

void AddMouse(long dx, long dy)
{
    InterlockedExchangeAdd(&s_mouseX, dx);
    InterlockedExchangeAdd(&s_mouseY, dy);
}

bool Active() { return s_yaw != 0.0f || s_pitch != 0.0f; }

namespace {

// Rotate `eye` about `pivot` by the orbit and set s_pitchEff (the pitch the angles must match).
// False when the eye is on the pivot, which leaves nothing to rotate.
bool Rotate(float eye[3], Vec3 pivot)
{
    s_pitchEff = s_pitch;
    const Vec3 v = Vec3::Load(eye) - pivot;
    const float len = v.Length();
    if (len < 1.0f) return false;
    const float horiz = std::sqrt(v.x * v.x + v.y * v.y);
    const float hz  = std::atan2(v.y, v.x) + s_yaw;
    const float elv = std::atan2(v.z, horiz);
    // Keep the new elevation between 75 deg below and 85 deg above the pivot.
    s_pitchEff = math::Clamp(s_pitchEff, elv - 85 * kDegToRad, elv + 75 * kDegToRad);
    const float e2 = elv - s_pitchEff;
    Vec3{ pivot.x + len * std::cos(e2) * std::cos(hz),
          pivot.y + len * std::cos(e2) * std::sin(hz),
          pivot.z + len * std::sin(e2) }.Store(eye);
    return true;
}

void FreeLook()   // angles only (first person, or no room to orbit)
{
    s_pitchEff = math::Clamp(s_pitch, -80 * kDegToRad, 80 * kDegToRad);
}

bool GameRotatesNow() { return s_updSpawn ? s_updRotated : s_gameRotates; }

// Inside the game's update, after its collision sweep got the orbited eye: which of the angles
// the controller set already follow that eye? A heading copied from the spawn and a pitch taken
// from the mode's fixed pitch (or the spawn's) don't, so the orbit is added to them; a look-at
// computed from the eye does, so adding the orbit would turn it twice.
void NoteGameAngles(const float native[3])
{
    using namespace game;
    const uintptr_t sp = s_updSpawn;
    const bool yaw = native[0] == mem::Read<float>(sp + spawn::kHeading);

    const float drop = mem::Read<float>(addr::kPitchDrop);
    float fixed[4] = { mem::Read<float>(sp + spawn::kPitch), 0, 0, 0 };
    int n = 1;
    const int mode = CameraMode();
    if (mode >= 0 && mode < 9)
        if (const uintptr_t ctl = mem::Read<uintptr_t>(addr::kCameraCtrls + mode * 4); ctl > 0x10000)
            fixed[n++] = mem::Read<float>(ctl + camctl::kFixedPitch);
    for (int i = 0, m = n; i < m; ++i) fixed[n++] = fixed[i] - drop;
    bool pitch = false;
    for (int i = 0; i < n; ++i) pitch |= std::fabs(native[1] - fixed[i]) < 1e-3f;

    static int s_loggedMode = -1, s_logged = -1;
    const int key = (yaw ? 1 : 0) | (pitch ? 2 : 0);
    if (mode != s_loggedMode || key != s_logged) {
        Log("camera: orbit in camera mode %d: heading %s, pitch %s", mode,
            yaw ? "added" : "follows the eye", pitch ? "added" : "follows the eye");
        s_loggedMode = mode;
        s_logged = key;
    }
    s_addYaw = yaw;
    s_addPitch = pitch;
}

} // namespace

// The fallback when the game didn't place the orbited eye itself: turn the finished eye.
void ApplyToEye(float eye[3], bool firstPerson)
{
    if (firstPerson) { FreeLook(); return; }
    if (GameRotatesNow()) return;                     // already orbited, with collision
    Vec3 pivot;
    if (!game::SpawnCameraPos(game::PlayerBase(), pivot) || !Rotate(eye, pivot)) FreeLook();
}

void BeginGameCamera(uintptr_t spawn)
{
    s_updSpawn = spawn;
    s_updRotated = false;
}

void RotateDesiredEye(float to[3])
{
    if (!s_updSpawn || s_updRotated || !Active() || game::IsFirstPerson()) return;
    Vec3 pivot;
    if (game::SpawnCameraPos(s_updSpawn, pivot) && Rotate(to, pivot)) s_updRotated = true;
}

void EndGameCamera()
{
    s_gameRotates = s_updRotated;
    if (!s_updRotated) s_addYaw = s_addPitch = true;
    s_updSpawn = 0;
}

void AngOffset(void* cam, const float in[3], float native[3], float out[3])
{
    CameraRecord& r = Record(cam);
    r.ang.Resolve(in, native);
    memcpy(out, native, sizeof(float) * 3);
    if (!r.main || !Active()) return;
    if (s_updSpawn && s_updRotated) NoteGameAngles(native);      // the game's SetOrientation in its update
    const bool game = GameRotatesNow();
    if (!game || s_addYaw) out[0] = math::WrapEq(native[0] + gfx::kHeadingSign * s_yaw / kEqUnitRad);
    if (!game || s_addPitch) out[1] = native[1] + gfx::kPitchSign * s_pitchEff / kEqUnitRad;
}

void AngCommit(void* cam, const float native[3], const float out[3])
{
    Record(cam).ang.Commit(native, out);
}

void OnFrame()
{
    static timing::FrameTimer timer;
    static Vec3 lastPos;
    static float lastHeading = 0.0f;
    static bool havePos = false;

    const float dt = timer.Tick();
    long dx = InterlockedExchange(&s_mouseX, 0);
    long dy = InterlockedExchange(&s_mouseY, 0);
    if (!Enabled()) { s_yaw = s_pitch = 0.0f; s_returning = s_wasDragging = false; return; }

    // Did the character move or turn since last frame? (that ends a held orbit)
    bool moved = false;
    Vec3 pos;
    if (game::PlayerPos(pos)) {
        const float heading = game::PlayerHeading();
        if (havePos) {
            const Vec3 delta = pos - lastPos;
            moved = delta.Dot(delta) > 0.01f * 0.01f || std::fabs(math::WrapEqDelta(heading - lastHeading)) > 0.05f;
        }
        lastPos = pos;
        lastHeading = heading;
        havePos = true;
    }

    const bool drag = Dragging();
    if (drag) {
        s_returning = false;
        // Same turn per count as right-click mouselook, including the X/Y sensitivity multipliers.
        float yawK = 0.15f * kDegToRad, pitchK = 0.15f * kDegToRad;
        bool invert = false;
        game::MouselookRates(yawK, pitchK, invert);
        yawK *= mouse::XScale();
        pitchK *= mouse::YScale();
        if (invert) dy = -dy;
        // Mouse right must turn the view toward screen right. Whether that is a clockwise or
        // counter-clockwise turn about +Z depends on the world's handedness, so check it
        // against the view: turning counter-clockwise moves forward along (-f.y, f.x).
        float turn = -1.0f;
        Vec3 right, f;
        if (view::Axes(right, f) && -f.y * right.x + f.x * right.y > 0.0f) turn = 1.0f;  // counter-clockwise is toward screen right
        s_yaw = math::WrapPi(s_yaw + turn * dx * yawK);
        s_pitch -= dy * pitchK;                             // mouse up: look up
        s_pitch = math::Clamp(s_pitch, -85 * kDegToRad, 85 * kDegToRad);
    } else if (Active()) {
        // Right-click mouselook always takes the camera back.
        if ((s_wasDragging && cfg.orbit.return_on_release) || (moved && cfg.orbit.return_on_move) || input::Button(1))
            s_returning = true;
    }
    s_wasDragging = drag;

    if (s_returning && !drag) {
        const float f = 1.0f - std::exp(-dt / 0.12f);               // swing back over ~0.3 s
        s_yaw -= s_yaw * f;
        s_pitch -= s_pitch * f;
        if (std::fabs(s_yaw) < 0.002f && std::fabs(s_pitch) < 0.002f) {
            s_yaw = s_pitch = 0.0f;
            s_returning = false;
        }
    }
}

} // namespace camera::orbit
