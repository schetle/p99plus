#pragma once
// Mouse fixes: sensitivity multipliers, frame-rate independent horizontal turning, and
// removal of the game's mouselook dead-zone.
#include "core/feature.h"

namespace mouse {

struct Settings {
    bool  enabled = true;               // master switch: off = the game's mouse handling is untouched
    float x_sensitivity = 1.0f;         // multiplies the game's horizontal mouse input
    float y_sensitivity = 1.0f;
    bool  fps_independent_turn = true; // horizontal turning no longer shrinks as FPS rises
    bool  remove_deadzone = true;      // the game's 20% mouselook axis dead-zone
};
extern Settings cfg;
extern const Feature feature;

// The sensitivity multipliers as applied (1 while the feature is off).
inline float XScale() { return cfg.enabled ? cfg.x_sensitivity : 1.0f; }
inline float YScale() { return cfg.enabled ? cfg.y_sensitivity : 1.0f; }

// Sensitivity scaling for the game's mouse input (fractional remainder carried over).
long FilterX(long raw);
long FilterY(long raw);
void ResetAccumulators();

// Patches and hooks inside eqgame.exe (mouselook.cpp); called every frame.
void ApplyGamePatches();

} // namespace mouse
