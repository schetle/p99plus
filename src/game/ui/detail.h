#pragma once
// Shared by the files of the custom-window module (game/ui/); not for features.
#include "game/ui/custom_window.h"
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

// What the game's last UI load parsed of each screen, by a hash of its XML and files. A core
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

// ---- body_drag.cpp ----

bool InstallBodyDrag();             // patch the window manager so dragByBody windows move from the client area

} // namespace game::wnd::detail
