#pragma once
// Menu widgets shared by every feature's section.
namespace game::wnd { class CustomWindow; }

namespace ui {

// Label on the left, control filling the rest of the row. All rows share one label column so
// the controls line up. IDs come from the label, so wrap repeated labels (e.g. "Up" in two
// sections) in PushID/PopID.
void RowLabel(const char* label);
bool SliderF(const char* label, float* v, float lo, float hi, const char* fmt);
bool SliderI(const char* label, int* v, int lo, int hi);

// Greyed-out text that wraps at the window edge (printf-style). Use for descriptions and hints;
// don't add manual line breaks.
void Hint(const char* fmt, ...);

// For a feature that has a custom game window: a hint when the window can't be shown (why, and what
// to do). False when it can, so the caller can print its own hint instead.
bool WindowStatusHint(const game::wnd::CustomWindow& w);

// A switchable group of controls inside a tab: heading, description, the enable checkbox, then
// the controls up to EndSection, greyed out while the switch is off. `enabled` may be nullptr
// for a group that can't be switched off.
void BeginSection(const char* title, const char* description, const char* enableLabel, bool* enabled);
void EndSection();

} // namespace ui
