// The camera offset (up / forward / right, per camera mode) and the zoom fade.
//
// The offset eye is written into the engine camera object's stored eye, so everything that reads
// it (name tags, spell effects, mouse picking, the view matrix) agrees. Per camera object we
// keep the real eye and the one we wrote (CameraRecord::eye), so reading our value back
// (GetPosition -> SetPosition, or the builder seeing an unchanged eye) never stacks the offset.
#include "features/camera/internal.h"
#include "game/eqgame.h"
#include <cstring>

namespace camera {

namespace {

using math::Vec3;

constexpr float kMainCameraRange = 600.0f;   // farther from the player than this = not the world camera
const Vec3 kUp = { 0, 0, 1 };                // world up is +Z

float  s_headHeight = 5.0f;                  // eye height above the player's z, learned in first person
Status s_status = { false, false, -1.0f, 100.0f };

// Settings for the current camera mode. First person uses its own set; every other (third
// person) mode uses the third-person set, optionally scaled.
ModeSettings PickSet(bool firstPerson, float scale)
{
    if (firstPerson) return cfg.offset.first_person;
    ModeSettings s = cfg.offset.third_person;
    s.up *= scale; s.forward *= scale; s.right *= scale;
    return s;
}

// Distance from `eye` to the player's head, or <0 if the player isn't available. In first
// person the eye height above the player's z is learned (smoothed).
float ZoomDistance(bool firstPerson, Vec3 eye)
{
    Vec3 p;
    if (!game::PlayerPos(p)) return -1.0f;
    if (firstPerson) {
        const float h = eye.z - p.z;
        if (h > 0.0f && h < 15.0f) s_headHeight += (h - s_headHeight) * 0.05f;
    }
    return (eye - Vec3{ p.x, p.y, p.z + s_headHeight }).Length();
}

} // namespace

bool ComputeEyeOffset(bool firstPerson, float zoomScale, Vec3& d)
{
    const ModeSettings set = PickSet(firstPerson, zoomScale);
    d = {};
    if (!cfg.offset.enabled || (set.up == 0 && set.forward == 0 && set.right == 0)) return false;

    d = kUp * set.up;
    Vec3 right, fwd, h;
    if (view::Axes(right, fwd)) {
        if (set.forward != 0 && math::HorizontalDir(fwd, kUp, h)) d += h * set.forward;
        if (set.right != 0 && math::HorizontalDir(right, kUp, h)) d += h * set.right;
    }
    return true;
}

void EyeOffset(void* cam, const float in[3], float native[3], float out[3])
{
    view::SetEngineActive();
    CameraRecord& r = Record(cam);
    r.eye.Resolve(in, native);
    memcpy(out, native, sizeof(float) * 3);

    const bool fp = game::IsFirstPerson();
    const float dist = ZoomDistance(fp, Vec3::Load(native));
    // Only the world camera, i.e. the one near the player (or any camera when the player is unknown).
    r.main = dist <= kMainCameraRange;
    if (!r.main) return;
    if (orbit::Active()) orbit::ApplyToEye(out, fp);

    float scale = 1.0f;
    Status st = { fp, false, dist, 100.0f };
    if (!fp && cfg.offset.zoom_fade && dist >= 0.0f) {
        scale = math::SmoothStep(dist / (cfg.offset.zoom_fade_distance < 1.0f ? 1.0f : cfg.offset.zoom_fade_distance));
        st.fadeActive = true;
        st.percent = scale * 100.0f;
    }
    s_status = st;

    Vec3 d;
    if (ComputeEyeOffset(fp, scale, d)) (Vec3::Load(out) + d).Store(out);
}

void EyeCommit(void* cam, const float native[3], const float out[3])
{
    Record(cam).eye.Commit(native, out);
}

Status GetStatus() { return s_status; }

} // namespace camera
