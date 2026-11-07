#pragma once
// The engine's name labels (EQGraphicsDX9.dll, see game/eqgfx.h): how big each one is drawn (or
// whether it is drawn at all), and which labels the engine really drew lately, since a label's
// stored screen rect is left as it was while the label isn't drawn.
#include <cstdint>

namespace game::labels {

// The size to draw the label of this engine actor at: 1 = the game's size, 0 = don't draw it.
using Policy = float (*)(uintptr_t actor);

void Init();                // installs the hooks once EQGraphicsDX9.dll is loaded (retried; call every frame)
bool Ready();               // the hooks are in
void Tick();                // once per Present (core/feature.cpp)
void SetPolicy(Policy fn);  // nullptr = every label at the game's size
// The engine drew this label (the object at [actor + 0x1a0]) this frame or the one before, so its
// rect is current. True when that can't be told (the hooks aren't in).
bool DrawnRecently(uintptr_t label);

// Runs right after the engine has drawn this frame's labels, with the device set up as it was for
// them (the world's depth buffer bound): what is drawn here can be depth-tested like the labels.
using AfterFn = void (*)();
void AfterLabels(AfterFn fn);
bool AfterLabelsReady();    // the hook behind AfterLabels is in

// A label depth (post-projection z, as at label + 0x3c) moved `units` world units toward the
// camera, so a mark drawn next to a label isn't hidden by the spawn the label hangs over.
// Unchanged when the camera's depth range can't be read.
float Nearer(float depth, float units);

} // namespace game::labels
