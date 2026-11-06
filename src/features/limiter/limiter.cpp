// Simple high-precision frame limiter applied right before Present.
#include "features/limiter/limiter.h"
#include "overlay/widgets.h"
#include "util/timing.h"
#include <imgui.h>
#include <iterator>
#include <mmsystem.h>

namespace limiter {

Settings cfg;

namespace {

const Setting kSettings[] = {
    BoolSetting("limiter", "enabled", &cfg.enabled, "Cap the frame rate."),
    IntSetting ("limiter", "fps_cap", &cfg.fps_cap, 10, 1000, "Frame rate cap in frames per second."),
};

bool s_timerRes = false;   // timeBeginPeriod(1) is in effect

void DrawMenu()
{
    ui::SliderI("FPS cap", &cfg.fps_cap, 10, 240);
}

void Shutdown()
{
    if (s_timerRes) { timeEndPeriod(1); s_timerRes = false; }
}

} // namespace

void Wait()
{
    static LONGLONG next = 0;

    if (!cfg.enabled) { next = 0; return; }
    if (!s_timerRes) { timeBeginPeriod(1); s_timerRes = true; }

    const LONGLONG freq = timing::Frequency();
    const LONGLONG period = freq / cfg.fps_cap;
    LONGLONG now = timing::Ticks();

    if (next == 0 || now - next > period)
        next = now;                            // first frame, or fell way behind: resync
    next += period;

    for (;;) {
        now = timing::Ticks();
        const LONGLONG left = next - now;
        if (left <= 0) break;
        if (left * 1000 / freq > 2) Sleep(1);
        else YieldProcessor();
    }
}

const Feature feature = {
    "Limiter",
    "Caps the frame rate. MaxFPS in eqclient.ini must be at least as high.",
    "Enable frame limiter", &cfg.enabled,
    kSettings, std::size(kSettings), nullptr, DrawMenu, Shutdown,
};

} // namespace limiter
