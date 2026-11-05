#pragma once
// Bookkeeping for a value we overwrite inside the engine while the game keeps reading it.
//
// We store `written` in place of the game's own `native` value. When the game reads our value
// back and sets it again (GetPosition -> SetPosition, or a per-frame rebuild from an unchanged
// field), IsOurs/Resolve recognize it so the change is never applied on top of itself.
#include <cmath>
#include <cstring>

template <int N>
struct ReadBack {
    float native[N] = {};
    float written[N] = {};
    bool  valid = false;      // native (and written) have been recorded

    // True when `v` is what we last wrote (within `tol` per component).
    bool IsOurs(const float* v, float tol = 0.0f) const
    {
        if (!valid) return false;
        for (int i = 0; i < N; ++i)
            if (!(std::fabs(v[i] - written[i]) <= tol)) return false;
        return true;
    }

    // The game's real value, given what the engine currently holds.
    void Resolve(const float* current, float* nativeOut, float tol = 0.0f) const
    {
        memcpy(nativeOut, IsOurs(current, tol) ? native : current, sizeof(float) * N);
    }

    void SetNative(const float* n)  { memcpy(native, n, sizeof(native)); valid = true; }
    void SetWritten(const float* w) { memcpy(written, w, sizeof(written)); }
    void Commit(const float* n, const float* w) { SetNative(n); SetWritten(w); }
};
