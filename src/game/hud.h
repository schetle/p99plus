#pragma once
// Text drawn on screen in the game's own fonts, at the point of the frame where the game draws its
// own net-status text. Drawing is only valid inside a DrawFn.
#include <cstdint>

namespace game::hud {

using DrawFn = void (*)();

bool Init();                // patches the draw point once (needs game::Available()); false if unavailable
bool Ready();
void OnDraw(DrawFn fn);     // idempotent; called once per rendered frame
void Tick();                // once per Present (core/feature.cpp)

int  FontCount();           // 0 when the UI isn't up
// Size of `text` in font `font` (clamped to the fonts there are). False when there is no font.
bool Measure(int font, const char* text, int& width, int& height);
// Draws `text` with its top left at x, y, in ARGB, with a one-pixel shadow of the same alpha.
void Text(int x, int y, uint32_t argb, int font, const char* text);

} // namespace game::hud
