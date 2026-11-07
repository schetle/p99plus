#pragma once
// A handle to one of the game's UI windows or controls (a CXWnd). It is just the pointer; the
// game owns the object. Every method is a call into the game's code or a field read, so only use
// it once game::Available().
#include <cstdint>
#include <string>

namespace game::wnd {

class Wnd {
public:
    Wnd() = default;
    explicit Wnd(uintptr_t ptr) : p_(ptr) {}

    explicit operator bool() const { return p_ != 0; }
    uintptr_t ptr() const { return p_; }
    bool operator==(Wnd o) const { return p_ == o.p_; }
    bool operator!=(Wnd o) const { return p_ != o.p_; }

    // Builds a window from the XML screen `screen` the way the game builds its own, shown by
    // Activate(). Empty when the screen isn't part of the loaded skin (the half-built window is
    // deleted).
    static Wnd CreateSidl(const char* screen);
    // Is it still in the UI manager's list of live windows?
    bool Alive() const;

    // A child control by its ScreenID; empty when there is none.
    Wnd Child(const char* screenId) const;
    void SetText(const char* text) const;   // SetWindowText (ASCII)
    void Show(bool show) const;             // show or hide a control
    void Activate() const;                  // Show(true, true) of a top-level window
    bool Visible() const;
    // A <Button> with Style_Checkbox: drawn pressed in while checked; a click flips it.
    bool Checked() const;
    void SetChecked(bool checked) const;
    // Esc may close the window (the game's default for its windows).
    void SetCloseOnEsc(bool on) const;
    // Writes position and size to the character's UI ini, as the game does when it saves the UI.
    void StoreIni() const;
    // The deleting destructor.
    void Delete() const;

    // The window's text (a typed-in Editbox holds what it shows here); "" for none or a text over
    // `maxLength` characters.
    std::string Text(int maxLength = 100000) const;
    // A top-level window's whole rect on screen (right and bottom exclusive), and moving or sizing it
    // the way the game does (its children follow; a size under the window's minimum is refused).
    void Rect(int& left, int& top, int& right, int& bottom) const;
    void Move(int left, int top, int right, int bottom) const;
    // Any window's rect on screen (a child's too, placed from its parent); false for an empty one.
    bool ScreenRect(int& left, int& top, int& right, int& bottom) const;
    static void ScreenSize(int& width, int& height);   // the UI's screen, in its pixels
    void AddStyle(unsigned bits) const;   // sets bits in the style word (+0x64)

    // The keyboard focus: the window that gets the keys the player types.
    bool HasFocus() const;
    void TakeFocus() const;
    void DropFocus() const;               // if it has it: then nobody does (the keys go to the game)

    // The mouse, as the UI sees it.
    static Wnd  UnderCursor();            // the window under it (empty for the game world)
    static bool CursorPos(int& x, int& y);   // where it is on the UI's screen

    // An Editbox. SetMaxChars: its longest text (-1 = no limit); longer text set later is cut to it.
    void SetMaxChars(int n) const;
    // The selection in characters, start <= end (equal: just the cursor).
    void EditSelection(int& start, int& end) const;
    void EditSelect(int start, int end) const;
    void EditInsert(const char* text) const;   // replaces the selection (or inserts at the cursor)

    // A Listbox: one text column. ListAdd returns the new row.
    void ListClear() const;
    int  ListAdd(const char* text, uint32_t argb = 0xffffffff) const;
    int  ListSelection() const;            // -1 for none
    void ListSelect(int row) const;
    // An STMLbox: shows `stml` (a tiny HTML: <br>, <c "#rrggbb">...</c>), replacing what it showed.
    void SetStml(const char* stml) const;

private:
    template <class Fn> Fn Virtual(int offset) const;
    uintptr_t p_ = 0;
};

} // namespace game::wnd
