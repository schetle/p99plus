#pragma once
// An in-game notepad: a small icon opens a notes window (a list of notes on the left, the selected
// note on the right). Each note records when and where it was written; the notes are Markdown
// files in a folder next to the game.
#include "core/feature.h"
#include "util/keybinding.h"

namespace notepad {

struct Settings {
    bool       enabled = true;
    bool       show_icon = true;                                  // the small book icon that opens the notepad
    KeyBinding open_key = { 'N', keybind::kCtrl | keybind::kShift };   // opens and closes the notepad
};
extern Settings cfg;
extern const Feature feature;

} // namespace notepad
