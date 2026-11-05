#pragma once
#include <windows.h>

namespace timing {

inline LONGLONG Frequency()
{
    static LONGLONG f = [] { LARGE_INTEGER v; QueryPerformanceFrequency(&v); return v.QuadPart; }();
    return f;
}

inline LONGLONG Ticks() { LARGE_INTEGER v; QueryPerformanceCounter(&v); return v.QuadPart; }

// Measures the time between successive Tick() calls (0 on the first).
class FrameTimer {
public:
    float Tick(float maxSeconds = 0.25f)
    {
        const LONGLONG now = Ticks();
        float dt = last_ ? (float)((now - last_) / (double)Frequency()) : 0.0f;
        last_ = now;
        return dt > maxSeconds ? maxSeconds : dt;
    }
private:
    LONGLONG last_ = 0;
};

} // namespace timing
