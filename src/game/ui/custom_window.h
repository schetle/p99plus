#pragma once
// Windows of our own, built the way the game builds its windows: an XML screen, loaded with the
// rest of the skin, and a CSidlScreenWnd made from it. They get the skin's look, drag like any
// game window and keep their position with the character's other windows.
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
#include <string>
#include <vector>

namespace game::wnd {

namespace detail { struct Access; }   // the module-internal view (detail.h)

// Another file the screen needs next to it in the skin directory (a texture its XML names).
struct CustomFile {
    std::string name;      // file name, as the XML refers to it
    std::string content;
};

struct CustomWindowDef {
    std::string screen;    // XML screen name; also the section in the character's UI ini
    std::string xml;       // the whole file: a full <XML> document defining that screen
    std::vector<CustomFile> files;
    bool dragByBody = false;  // moves when dragged anywhere, also when sizable (the game moves a
                              // sizable window only by its title bar)
    bool closeOnEsc = false;  // Esc may close it (the game's default for a window); ours stay up
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

    // Child controls by ScreenID. The control is looked up once per build of the window.
    void SetText(const char* id, const std::string& text);   // writes only when the text changed
    void ShowChild(const char* id, bool show);               // likewise
    // Keeps a Style_Checkbox button and `value` in sync. A change of `value` in code (or a new
    // window) sets the button; a click sets `value`. True when the player clicked.
    bool BindCheck(const char* id, bool& value);

private:
    friend struct detail::Access;
    struct Control {
        std::string id;
        Wnd         wnd;        // empty: the screen has no such control
        std::string text;       // what SetText wrote last
        int         shown = -1; // what ShowChild set last (-1: nothing yet)
        int         checked = -1;
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
