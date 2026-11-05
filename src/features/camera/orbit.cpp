// Middle-button camera orbit.
//
// The orbit is a rigid rotation of the camera around the character's head: the eye is rotated
// about the head and the camera's angles get the same turn, so the camera keeps looking at you.
//   s_yaw:   world rotation about +Z in radians (positive = counter-clockwise from above)
//   s_pitch: radians, positive = look up (the eye drops below, looking up at you)
// Which way EQ's heading/pitch numbers turn the view isn't known for sure, so it is learned
// from the view matrix the game builds (LearnAngles) before the orbit is allowed.
#include "features/camera/internal.h"
#include "core/log.h"
#include "features/mouse/mouse.h"
#include "game/eqgame.h"
#include "input/input.h"
#include "overlay/overlay.h"
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

int   s_yawSign = 0, s_pitchSign = 0;              // 0 = not learned yet
int   s_yawVote = 0, s_yawVotes = 0, s_pitchVote = 0, s_pitchVotes = 0;
float s_lastAng[3] = {}, s_lastPhi = 0.0f, s_lastFz = 0.0f;
bool  s_haveLast = false;

void Vote(int sign, int& vote, int& votes, int& locked, const char* what)
{
    if (locked) return;
    if (sign == vote) ++votes; else { vote = sign; votes = 1; }
    if (votes >= 8) {
        locked = vote;
        Log("camera: %s direction learned: %+d", what, locked);
    }
}

} // namespace

bool Enabled() { return cfg.orbit.enabled; }
bool Dragging() { return Enabled() && input::Button(2) && !overlay::Visible(); }

void AddMouse(long dx, long dy)
{
    InterlockedExchangeAdd(&s_mouseX, dx);
    InterlockedExchangeAdd(&s_mouseY, dy);
}

bool Active() { return s_yaw != 0.0f || s_pitch != 0.0f; }
bool Ready()  { return s_yawSign != 0; }      // tilt also needs s_pitchSign

State GetState()
{
    if (!Enabled() || !view::EngineActive()) return State::Off;
    if (!Ready()) return State::Setup;
    return s_pitchSign ? State::Ready : State::NoTilt;
}

// Rotate the eye about the head by the orbit. Sets s_pitchEff (the pitch the angles must match).
void ApplyToEye(float eye[3], bool firstPerson)
{
    s_pitchEff = s_pitchSign ? s_pitch : 0.0f;
    Vec3 head;
    if (!HeadPosition(head)) return;

    const Vec3 v = Vec3::Load(eye) - head;
    const float len = v.Length();
    if (firstPerson || len < 1.0f) {                   // free look: angles only
        s_pitchEff = math::Clamp(s_pitchEff, -80 * kDegToRad, 80 * kDegToRad);
        return;
    }
    const float horiz = std::sqrt(v.x * v.x + v.y * v.y);
    const float hz  = std::atan2(v.y, v.x) + s_yaw;
    const float elv = std::atan2(v.z, horiz);
    // Keep the new elevation between 75 deg below and 85 deg above the head.
    s_pitchEff = math::Clamp(s_pitchEff, elv - 85 * kDegToRad, elv + 75 * kDegToRad);
    const float e2 = elv - s_pitchEff;
    Vec3{ head.x + len * std::cos(e2) * std::cos(hz),
          head.y + len * std::cos(e2) * std::sin(hz),
          head.z + len * std::sin(e2) }.Store(eye);
}

void AngOffset(void* cam, const float in[3], float native[3], float out[3])
{
    CameraRecord& r = Record(cam);
    r.ang.Resolve(in, native);
    memcpy(out, native, sizeof(float) * 3);
    if (!r.main || !Active() || !Ready()) return;
    out[0] = math::WrapEq(native[0] + s_yawSign * s_yaw / kEqUnitRad);
    if (s_pitchSign) out[1] = native[1] + s_pitchSign * s_pitchEff / kEqUnitRad;
}

void AngCommit(void* cam, const float native[3], const float out[3])
{
    Record(cam).ang.Commit(native, out);
}

// Compare how the view's forward vector moved with how the game's heading/pitch moved
// (only while the orbit adds nothing, so the view comes from the game's own angles).
void LearnAngles(const float ang[3])
{
    if ((s_yawSign && s_pitchSign) || Active()) return;
    Vec3 right, f;
    if (!view::Axes(right, f)) return;

    const float phi = std::atan2(f.y, f.x);
    if (s_haveLast) {
        const float dh = math::WrapEqDelta(ang[0] - s_lastAng[0]);
        const float dphi = math::WrapPi(phi - s_lastPhi);
        if (std::fabs(dh) > 0.3f && std::fabs(dh) < 60.0f && std::fabs(dphi) > 1e-4f && std::fabs(f.z) < 0.95f)
            Vote((dh > 0) == (dphi > 0) ? 1 : -1, s_yawVote, s_yawVotes, s_yawSign, "orbit heading");
        const float dp = ang[1] - s_lastAng[1];
        const float dz = f.z - s_lastFz;
        if (std::fabs(dp) > 0.3f && std::fabs(dp) < 60.0f && std::fabs(dz) > 1e-4f)
            Vote((dp > 0) == (dz > 0) ? 1 : -1, s_pitchVote, s_pitchVotes, s_pitchSign, "orbit pitch");
    }
    memcpy(s_lastAng, ang, sizeof(s_lastAng));
    s_lastPhi = phi;
    s_lastFz = f.z;
    s_haveLast = true;
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

    const bool drag = Dragging() && Ready();
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
        if (s_pitchSign) s_pitch -= dy * pitchK;            // mouse up: look up
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
