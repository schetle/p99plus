// X/Y sensitivity multipliers for the game's mouse input. The fractional remainder is carried
// between reads so slow movement isn't lost.
#include "features/mouse/mouse.h"

namespace mouse {

namespace {

double s_accX = 0, s_accY = 0;

long Scale(long raw, float mult, double& acc)
{
    if (mult == 1.0f) { acc = 0; return raw; }
    // Direction reversal: drop the stale remainder so it doesn't fight the user.
    if ((raw > 0 && acc < 0) || (raw < 0 && acc > 0)) acc = 0;
    acc += raw * (double)mult;
    long out = (long)acc;   // truncate toward zero, keep the fraction
    acc -= out;
    return out;
}

} // namespace

long FilterX(long raw) { return Scale(raw, XScale(), s_accX); }
long FilterY(long raw) { return Scale(raw, YScale(), s_accY); }
void ResetAccumulators() { s_accX = s_accY = 0; }

} // namespace mouse
