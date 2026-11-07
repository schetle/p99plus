#pragma once
// Floating combat text: damage, heals and misses shown over the spawn they happen to, in the
// game's own font, fading out after a moment.
#include "core/feature.h"

namespace combattext {

struct Settings {
    bool  enabled = true;
    float show_time = 1.0f;   // seconds at full strength
    float fade_time = 1.0f;   // seconds to fade out after that
    int   rise = 30;          // pixels the text drifts up over its lifetime
    int   height = 0;         // pixels above the default start (just over the spawn's name label)
    int   font = 3;           // game font index (as in /chatfontsize)
    bool  on_you = true;      // hits and misses on you, over your head
    bool  on_enemies = true;  // your (and your pet's) hits and misses on others, over them
    bool  heals = true;       // your own HP gains, over your head
    int   heal_min = 10;      // smallest HP gain shown (keeps most regen ticks out)
};
extern Settings cfg;
extern const Feature feature;

} // namespace combattext
