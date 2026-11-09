#pragma once
// Newer (Luclin-era) player models, chosen per race and gender.
#include "core/feature.h"

namespace models {

inline constexpr int kRaces = 13;   // the classic races and Iksar, in models.cpp's kRace order

struct Settings {
    bool enabled = false;
    bool male[kRaces]   = { true, true, true, true, true, true, true, true, true, true, true, true, true };
    bool female[kRaces] = { true, true, true, true, true, true, true, true, true, true, true, true, true };
};
extern Settings cfg;
extern const Feature feature;

} // namespace models
