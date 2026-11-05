#pragma once
// Small math helpers shared by the camera, mouse and game-access code.
#include <cmath>

namespace math {

inline constexpr float kPi        = 3.14159265f;
inline constexpr float kDegToRad  = kPi / 180.0f;
inline constexpr float kEqCircle  = 512.0f;                    // EQ angle units in a full circle
inline constexpr float kEqUnitRad = 2.0f * kPi / kEqCircle;    // one EQ angle unit in radians

template <class T> constexpr T Clamp(T v, T lo, T hi) { return v < lo ? lo : v > hi ? hi : v; }

inline float SmoothStep(float t) { t = Clamp(t, 0.0f, 1.0f); return t * t * (3.0f - 2.0f * t); }

// Radians into (-pi, pi].
inline float WrapPi(float a)
{
    while (a > kPi) a -= 2 * kPi;
    while (a <= -kPi) a += 2 * kPi;
    return a;
}

// EQ angle units into [0, 512).
inline float WrapEq(float a)
{
    a = std::fmod(a, kEqCircle);
    return a < 0 ? a + kEqCircle : a;
}

// Difference of two EQ angles, into [-256, 256].
inline float WrapEqDelta(float d)
{
    while (d > kEqCircle / 2) d -= kEqCircle;
    while (d < -kEqCircle / 2) d += kEqCircle;
    return d;
}

struct Vec3 {
    float x = 0, y = 0, z = 0;

    static Vec3 Load(const float* p) { return { p[0], p[1], p[2] }; }
    void Store(float* p) const { p[0] = x; p[1] = y; p[2] = z; }

    Vec3 operator+(Vec3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator-(Vec3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3& operator+=(Vec3 o) { x += o.x; y += o.y; z += o.z; return *this; }

    float Dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    float Length() const { return std::sqrt(Dot(*this)); }
};

// `v` with its component along the unit vector `up` removed, normalized. False when v is
// (nearly) parallel to `up`, in which case `out` is unusable.
inline bool HorizontalDir(Vec3 v, Vec3 up, Vec3& out)
{
    out = v - up * v.Dot(up);
    const float len = out.Length();
    if (len < 0.05f) return false;
    out = out * (1.0f / len);
    return true;
}

} // namespace math
