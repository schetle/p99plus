#pragma once
// A few fixed lines of text drawn next to the engine's name labels, depth-tested like them
// (render/screen_quads.h). The lines are rendered once with GDI (Arial, the font the engine draws
// its labels with) into one texture, white with a dark drop shadow, and each copy is tinted when
// drawn.
#include <cstdint>

namespace render::status_text {

inline constexpr int kMaxLines = 8;

// Renders the lines into the texture, once per core (later calls only report the result). Needs
// the device (render::Device()). True when the texture exists.
bool Load(const char* const* lines, int count);

struct Item {
    int      line;          // index into the lines given to Load
    float    centerX, top;  // screen pixels: horizontal centre and top of the text
    float    height;        // the text's line height in pixels (the shadow reaches a little past it)
    float    depth;         // post-projection z to test against (0 near .. 1 far)
    uint32_t argb;          // colour of the text (the shadow stays dark)
};

// Draws the items with the z-test on and no z-writes. Call where the world's depth buffer is
// bound (game/labels.h AfterLabels).
void Draw(const Item* items, int count);

void Release();             // at unload (the texture is managed and survives a device reset)

} // namespace render::status_text
