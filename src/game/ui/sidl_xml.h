#pragma once
// Builds the XML of a custom window's screen (see custom_window.h): small writers for the pieces
// the game's skins are made of. Only what our windows use; each call returns one element,
// ready to be passed to Document().
#include <string>
#include <vector>

namespace game::wnd::sidl {

// The whole file: a full <XML> document holding `parts` in order.
std::string Document(const std::vector<std::string>& parts);

// A texture the screen draws from, with its size in pixels.
std::string Texture(const std::string& file, int cx, int cy);
// The five animations a checkbox button needs, from a strip of 16x16 frames in `texture` (frame 0
// normal, 1 hover, 2 pressed, 3 pressed + hover; disabled looks like normal). Named
// <prefix>Normal, Flyby, Pressed, PressedFlyby and Disabled.
std::string ButtonFrames(const std::string& prefix, const std::string& texture, int frameSize);

// Where a control sits in its window: the top/bottom offsets from the window's top, and the
// left/right offsets from the window's left (`leftFromLeft`) or right edge; right is always from
// the right edge. The control stretches with the window.
struct Anchors {
    int  top, bottom, left, right;
    bool leftFromLeft;
};
// A centred, single-line, white label in the skin's font.
std::string Label(const std::string& item, const std::string& screenId, const Anchors& a, const std::string& text);
// A checkbox button drawn with ButtonFrames(animPrefix, ...).
std::string CheckButton(const std::string& item, const std::string& screenId, const Anchors& a,
                        const std::string& animPrefix);

// A bordered, sizable window without a title bar (the skin's WDT_RoundedNoTitle) whose pieces are
// the items named in `pieces`.
struct Screen {
    std::string              item;
    int                      x, y, cx, cy;   // default position and size
    std::vector<std::string> pieces;
};
std::string BorderedScreen(const Screen& s);

} // namespace game::wnd::sidl
