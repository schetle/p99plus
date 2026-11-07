#pragma once
// Spell icons drawn straight through Direct3D, depth-tested (render/screen_quads.h), so they hide
// behind the scene the way the engine's name labels do (the game's own UI blits can't be: the UI
// has no depth). The images are the skin's spells01..07.tga: 256x256 sheets of 6 x 6 icons of
// 40 px, the cells a spell's new_icon numbers (what the game's A_SpellIcons animation shows).
#include <cstdint>
#include <string>

namespace render::icons {

struct Icon {
    int      cell;         // the spell's icon number
    float    x, y, size;   // top-left corner and side in screen pixels
    float    depth;        // post-projection z to test against (0 near .. 1 far)
    uint32_t argb;         // tint (multiplies the image); 0xffffffff = as drawn
};

// Reads the sheets from `skinDir` (a folder with a trailing '\'), else from UIFiles\Default.
// Once per device; true when at least one sheet loaded.
bool Load(const std::string& skinDir);

// Draws the icons with the z-test on and no z-writes. Call where the world's depth buffer is bound
// (game/labels.h AfterLabels); the device state is put back afterwards.
void Draw(const Icon* icons, int count);

void Release();              // at unload (the textures are managed and survive a device reset)

} // namespace render::icons
