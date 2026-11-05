#pragma once
// Frame cap applied right before Present.
#include "core/feature.h"

namespace limiter {

struct Settings {
    bool enabled = false;
    int  fps_cap = 60;
};
extern Settings cfg;
extern const Feature feature;

void Wait();           // call right before the real Present

} // namespace limiter
