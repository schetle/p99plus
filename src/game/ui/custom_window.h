#pragma once
// Windows of our own, built the way the game builds its windows: an XML screen, loaded with the
// rest of the skin, and a CSidlScreenWnd made from it. They get the skin's look, drag like any
// game window and keep their position and size in our own file (detail.h, placement.cpp); one with
// nothing saved yet starts in the middle of the screen.
//
// A feature defines a window once, as a static CustomWindow; it registers itself. Each frame the
// feature calls SetWanted() and then the control methods below, which do nothing while the window
// doesn't exist and only touch the game when a value changed. The module creates and deletes the
// window to match (driver.h).
//
// The game only knows a screen if it came in with the EQUI.xml load, so we write the screen's XML
// next to the skin's files and add one <Include> line for it to a copy of EQUI.xml, which the game
// is then told to load instead. A window is built when the in-world UI is built and deleted
// whenever the game deletes its own windows (a skin change, going back to character select).
#include "game/ui/wnd.h"
#include <optional>
#include <string>
#include <vector>

namespace game::wnd {

namespace detail { struct Access; }   // the module-internal view (detail.h)

struct CustomWindowDef {
    std::string screen;    // XML screen name; also the section in the character's UI ini
    std::string xml;       // the whole file: a full <XML> document defining that screen
    bool dragByBody = false;  // moves when dragged anywhere, also when sizable (the game moves a
                              // sizable window only by its title bar)
    bool closeOnEsc = false;  // Esc may close it (the game's default for a window); ours stay up
    bool startHidden = false; // built hidden and kept that way across a reload: the feature opens it
                              // with SetShown (and the game hides it with its close box)
};

enum class Status {
    Unavailable,   // the game's window code wasn't found, or the game isn't running yet
    Rejected,      // the game refused the injected XML; custom windows are off for the session
    NotLoaded,     // the screen isn't in the loaded skin; a UI load (/loadskin) fixes it
    Stale,         // the game has an older version of the XML loaded (the core was reloaded with a
                   // changed screen); a UI load fixes it
    NotInWorld,    // no window yet: it appears once the in-world UI is built
    Shown,
};

class CustomWindow {
public:
    explicit CustomWindow(CustomWindowDef def);   // registers it
    CustomWindow(const CustomWindow&) = delete;
    CustomWindow& operator=(const CustomWindow&) = delete;

    void SetWanted(bool wanted) { wanted_ = wanted; }   // every frame; the window follows it
    bool Exists() const { return (bool)wnd_; }
    Status GetStatus() const;
    const CustomWindowDef& Def() const { return def_; }

    // For a window the feature opens and closes itself (`startHidden`). The game also hides it
    // (a close box), so ask Visible() rather than keeping a flag.
    bool Visible() const { return wnd_ && wnd_.Visible(); }
    void SetShown(bool shown);

    // Child controls by ScreenID. The control is looked up once per build of the window; each setter
    // only touches the game when its value changed since the last call (or the window was rebuilt).
    // SetText also writes with `force` (for a text box the player has typed in, whose content isn't
    // what was last set).
    void SetText(const char* id, const std::string& text, bool force = false);
    std::string Text(const char* id, int maxLength = 100000);   // what the control holds now ("" for none)
    Wnd  Child(const char* id);                                 // the control itself (empty for none)
    void ShowChild(const char* id, bool show);
    void SetMaxChars(const char* id, int n);                    // a text box's longest text
    void AddStyle(const char* id, unsigned bits);               // style bits the XML can't set
    // An STML box (sidl::StmlBox): shows `stml`, replacing what it showed.
    void SetStml(const char* id, const std::string& stml);
    // A list (sidl::Listbox): its rows, and which one is selected (-1 none). The selection is also
    // written when the rows were.
    void SetList(const char* id, const std::vector<std::string>& rows, int selected);
    int  ListSelection(const char* id);                         // the selected row now: the player clicks change it

    // A button (sidl::PushButton, sidl::BorrowedButton): a click is a press and release of the left
    // button over it, reported once. Call every frame the window is up.
    bool Clicked(const char* id);
    // A button whose checked look code owns (sidl::BorrowedButton): shows `on`, except while the
    // mouse holds the button (the game draws it pressed then). Call after Clicked.
    void SetCheck(const char* id, bool on);
    // Keeps a Style_Checkbox button and `value` in sync. A change of `value` in code (or a new
    // window) sets the button; a click sets `value`. True when the player clicked.
    bool BindCheck(const char* id, bool& value);

private:
    friend struct detail::Access;
    struct Control {
        std::string id;
        Wnd         wnd;        // empty: the screen has no such control
        // What the setters wrote last (empty: nothing yet).
        std::string                             text;
        std::optional<bool>                     shown, checked;
        std::optional<int>                      maxChars, selected;
        std::optional<std::string>              stml;
        std::optional<std::vector<std::string>> rows;
        unsigned                                styled = 0;   // bits AddStyle set
        // Clicked: the left button's state at the last look, and whether it went down over the control.
        bool mouseWasDown = false, armed = false;
    };
    Control* Find(const char* id);   // null while there is no window
    void Attach(Wnd w);              // the window was built, adopted or deleted: forget the controls

    CustomWindowDef      def_;
    bool                 wanted_ = true;
    Wnd                  wnd_;           // the window while it exists
    bool                 failed_ = false;// the screen wasn't loaded; not retried until the next UI load
    bool                 stale_ = false;
    std::vector<Control> controls_;
};

} // namespace game::wnd
