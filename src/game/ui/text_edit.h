#pragma once
// A multi-line text box (sidl::Editbox) in one of our windows, with the editing the game's own box
// lacks: Enter starts a new line, copy, cut, paste, select all, undo and redo (Ctrl+C/X/V/A/Z,
// Ctrl+Y or Ctrl+Shift+Z; the box ignores Ctrl+letter, and the game's own paste key turns line
// breaks into spaces), moving the cursor (the arrows, Home and End; Shift selects, Ctrl moves by
// word or to the ends of the text; the box itself ignores them, and Up/Down only tell its parent),
// and a length limit that grows with the text.
//
// The box is where the player types; TextEdit keeps what it last read there (Text()) and the undo
// history. An undo step is the text from before a burst of typing (a pause of a second ends it), a
// cut, a paste or an undo/redo itself.
#include "game/ui/custom_window.h"
#include "util/keybinding.h"
#include <string>
#include <vector>

namespace game::wnd {

class TextEdit {
public:
    TextEdit(CustomWindow& window, const char* id) : window_(window), id_(id) {}

    Wnd Box() const { return window_.Child(id_); }   // empty while the window doesn't exist
    const std::string& Text() const { return text_; }

    // Puts `text` into the box. `newDocument` (another note, say) also forgets the undo history.
    void Load(const std::string& text, bool newDocument);
    // Reads the box back now. True when its text changed since the last look.
    bool Sync();
    // Every frame while the box is shown: reads it back a few times a second and runs the
    // shortcuts while it has the keyboard and `keysAllowed` (say, no menu over the game). True when
    // the text changed.
    bool Update(bool keysAllowed);

private:
    struct Step { std::string text; int cursor; };
    enum Action { kCopy, kCut, kPaste, kSelectAll, kUndo, kRedo };
    static constexpr int kNumShortcuts = 7;   // Update's table (redo has two keys)

    void Navigate(bool keys);             // the arrows, Home and End, with their key repeat
    void Move(int vk, bool shift, bool ctrl);

    // Each true when the box's text changed.
    bool Run(Action a);
    bool Insert(const std::string& text, size_t shownChars);   // replaces the selection: one step
    bool Apply(std::vector<Step>& from, std::vector<Step>& to); // undo (from undo_) or redo
    void Put(const std::string& text);    // into the box, and Text()
    void Grow(size_t chars);              // the limit fits a text of `chars`
    static void Push(std::vector<Step>& steps, Step step);   // at most kMaxSteps kept
    void ClearHistory();

    CustomWindow&      window_;
    const char*        id_;
    Wnd                loaded_;              // the box Load filled (a rebuilt window has an empty one)
    std::string        text_;
    int                limit_ = 0;           // the box's max length
    std::vector<Step>  undo_, redo_;
    bool               newStep_ = true;      // the next change starts a step of its own
    unsigned long long lastChange_ = 0;      // GetTickCount64() of the last change seen
    unsigned long long nextPoll_ = 0;
    int                lastCursor_ = 0;      // where the cursor was at the last look: a step goes back there
    keybind::Press     keys_[kNumShortcuts];
    int                anchor_ = 0, caret_ = 0;   // the selection as last moved by Move: where it started, where the cursor is
    int                goalColumn_ = -1;      // Up/Down keep to this column across short lines; -1 for none
    int                navKey_ = 0;           // the navigation key held, 0 for none
    unsigned long long navNext_ = 0;          // GetTickCount64() of its next repeat
};

// While `control` has the keyboard focus, the game gets no key presses at all, not even the ones
// the control passes on (an Editbox lets Ctrl and Alt combinations through, so Ctrl+A would turn
// the character). Escape, or a click where no window is (the game world), takes the focus away
// and gives the keys back; a click on another window moves the focus there (the game does that).
// The arrows, Home and End don't reach the control either (TextEdit moves its cursor; the box's
// own Shift+Left/Right would collapse the selection).
// Call every frame; an empty Wnd for none.
void GuardKeys(Wnd control);

} // namespace game::wnd
