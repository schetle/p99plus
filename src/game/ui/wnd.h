#pragma once
// A handle to one of the game's UI windows or controls (a CXWnd). It is just the pointer; the
// game owns the object. Every method is a call into the game's code or a field read, so only use
// it once game::Available().
#include <cstdint>

namespace game::wnd {

class Wnd {
public:
    Wnd() = default;
    explicit Wnd(uintptr_t ptr) : p_(ptr) {}

    explicit operator bool() const { return p_ != 0; }
    uintptr_t ptr() const { return p_; }
    bool operator==(Wnd o) const { return p_ == o.p_; }

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

private:
    template <class Fn> Fn Virtual(int offset) const;
    uintptr_t p_ = 0;
};

} // namespace game::wnd
