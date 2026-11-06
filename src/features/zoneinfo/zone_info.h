#pragma once
// A small game window with the zone you are in, the result of your last Sense Heading and your last /loc.
#include "core/feature.h"

namespace zoneinfo {

struct Settings {
    bool enabled = true;
    bool zone_name = true;   // the lines of the window, each on its own
    bool heading = true;
    bool location = true;
    bool pin_location = true;   // the pin button on the Last Location line
};
extern Settings cfg;
extern const Feature feature;

} // namespace zoneinfo
