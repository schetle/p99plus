#pragma once
// Shared by the files of the custom-window module (game/ui/); not for features.
#include "game/ui/custom_window.h"
#include <string>
#include <vector>

namespace game::wnd::detail {

constexpr int kMaxWindows = 8;     // windows the stash can carry across a core reload
constexpr int kScreenLen = 48;     // longest screen name (with its terminator) the stash keeps

// Every CustomWindow, in definition order (a function-local static, so it exists whenever a
// CustomWindow is constructed).
std::vector<CustomWindow*>& Registry();

// What the module may do to a window that features may not.
struct Access {
    static Wnd  Handle(const CustomWindow& w) { return w.wnd_; }
    static void Attach(CustomWindow& w, Wnd wnd) { w.Attach(wnd); }
    static bool Wanted(const CustomWindow& w) { return w.wanted_; }
    static bool& Failed(CustomWindow& w) { return w.failed_; }
    static bool& Stale(CustomWindow& w) { return w.stale_; }
};

// ---- inject.cpp: getting the screens into the skin ----

// What the game's last UI load parsed of each screen, by a hash of its XML. A core
// reload with a changed XML can't change what the game has loaded, so a window adopted or built
// later still has the old screen's controls until the next UI load.
struct LoadedScreen { char screen[kScreenLen]; unsigned hash; };
struct InjectState {
    bool         failed;            // the game rejected the injected XML; custom windows are off
    int          numLoaded;
    LoadedScreen loaded[kMaxWindows];
};
extern InjectState g_inject;        // kept across a core reload (custom_window.cpp stashes it)

bool InstallInject();               // hooks the UI loader
void CheckStale();                  // sets/clears each window's `stale` from g_inject.loaded

std::string MergedFile();                          // our copy of EQUI.xml, as written into the skin
std::string ScreenFile(const CustomWindowDef& d);  // a screen's file, as written into the skin
std::string DefaultSkin();                         // the Default skin's folder, with a trailing '\'
bool ReadText(const std::string& path, std::string& out);

// ---- skin_xml.cpp: fitting the screens to the loaded skin (folder `skin`) ----

// The frame (DrawTemplate) most of the skin's own titled windows use.
std::string TitledFrame(const std::string& skin);
// A screen's XML as written into the skin: sidl::kTitledFrame replaced with `frame`, and
// sidl::BorrowedButton's markers with the skin's button (its size, centred in the box, and art).
std::string ForSkin(std::string xml, const std::string& frame, const std::string& skin);

// ---- placement.cpp: where each window was left, in our own file ----
// The game keeps a window's place in the character's UI ini, but only stores it at some moments
// (a title-bar drag, quitting), which misses body drags and our deletion at a skin load. So the
// module keeps its own copy (AppFile("_windows.ini"), one section per screen) and puts it back.

// A window was just built: move it where it was left (kept on screen), or, with nothing saved, to
// the middle of the screen. A sizable window gets its size back too; any other keeps its XML size.
void RestorePlacement(const CustomWindow& w);
// Every frame: once the window was moved or sized and the mouse is let go, write it.
void TrackPlacement(const CustomWindow& w);
// Before the window is deleted (or the core unloads): write what changed, now.
void SavePlacement(const CustomWindow& w);
// The window is there already (kept from the previous core): what it shows is what is saved.
void AdoptPlacement(const CustomWindow& w);

// ---- body_drag.cpp ----

bool InstallBodyDrag();             // patch the window manager so dragByBody windows move from the client area

} // namespace game::wnd::detail
