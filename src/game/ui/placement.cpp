// Where each custom window was left (detail.h): our own file, read when a window is built and
// written when the player has moved or sized one.
#include "core/app.h"
#include "core/log.h"
#include "game/ui/detail.h"
#include "game/ui/sidl_xml.h"
#include "input/input.h"
#include <windows.h>
#include <algorithm>
#include <climits>
#include <map>
#include <string>

namespace game::wnd::detail {

namespace {

struct Place { int left, top, right, bottom; };

// What each window showed when it was last saved or restored, by screen name.
std::map<std::string, Place> s_known;

std::string FilePath() { return AppFile("_windows.ini"); }

bool Same(const Place& a, const Place& b)
{
    return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
}

Place Current(Wnd w)
{
    Place p;
    w.Rect(p.left, p.top, p.right, p.bottom);
    return p;
}

bool Load(const std::string& screen, Place& p)
{
    const std::string path = FilePath();
    const char* s = screen.c_str();
    constexpr int kNone = INT_MIN;
    const int x = (int)GetPrivateProfileIntA(s, "x", kNone, path.c_str());
    const int y = (int)GetPrivateProfileIntA(s, "y", kNone, path.c_str());
    const int cx = (int)GetPrivateProfileIntA(s, "width", 0, path.c_str());
    const int cy = (int)GetPrivateProfileIntA(s, "height", 0, path.c_str());
    if (x == kNone || y == kNone || cx <= 0 || cy <= 0) return false;
    p = { x, y, x + cx, y + cy };
    return true;
}

void Write(const std::string& screen, const Place& p)
{
    const std::string path = FilePath();
    const char* s = screen.c_str();
    bool ok = true;
    const auto put = [&](const char* key, int v) {
        ok = WritePrivateProfileStringA(s, key, std::to_string(v).c_str(), path.c_str()) && ok;
    };
    put("x", p.left);
    put("y", p.top);
    put("width", p.right - p.left);
    put("height", p.bottom - p.top);
    if (!ok) Log("custom window %s: could not save its position to %s", s, path.c_str());
}

// Moves `p` (keeping its size) so the whole window is on a screen of `sw` x `sh`.
void KeepOnScreen(Place& p, int sw, int sh)
{
    const int cx = p.right - p.left, cy = p.bottom - p.top;
    p.left = std::max(0, std::min(p.left, sw - cx));
    p.top = std::max(0, std::min(p.top, sh - cy));
    p.right = p.left + cx;
    p.bottom = p.top + cy;
}

} // namespace

void RestorePlacement(const CustomWindow& cw)
{
    const Wnd w = Access::Handle(cw);
    if (!w) return;
    const std::string& screen = cw.Def().screen;
    const Place built = Current(w);
    int sw, sh;
    Wnd::ScreenSize(sw, sh);

    Place p;
    const bool saved = Load(screen, p);
    if (!saved || !sidl::IsSizable(cw.Def().xml)) {   // the XML's size
        const int left = saved ? p.left : (sw - (built.right - built.left)) / 2;
        const int top = saved ? p.top : (sh - (built.bottom - built.top)) / 2;
        p = { left, top, left + built.right - built.left, top + built.bottom - built.top };
    }
    if (sw > 0 && sh > 0) KeepOnScreen(p, sw, sh);
    if (!Same(p, built)) w.Move(p.left, p.top, p.right, p.bottom);
    s_known[screen] = Current(w);
    if (!saved) Log("custom window %s: no saved position; placed in the middle of the screen", screen.c_str());
}

void TrackPlacement(const CustomWindow& cw)
{
    if (input::Button(0) || input::Button(1)) return;   // still dragging or sizing
    SavePlacement(cw);
}

void SavePlacement(const CustomWindow& cw)
{
    const Wnd w = Access::Handle(cw);
    if (!w) return;
    const Place now = Current(w);
    if (now.right <= now.left || now.bottom <= now.top) return;
    const auto it = s_known.find(cw.Def().screen);
    if (it != s_known.end() && Same(it->second, now)) return;
    s_known[cw.Def().screen] = now;
    Write(cw.Def().screen, now);
}

void AdoptPlacement(const CustomWindow& cw)
{
    const Wnd w = Access::Handle(cw);
    if (!w) return;
    // The previous core saved anything it saw change; a move it didn't see is saved on the next Track.
    Place p;
    if (Load(cw.Def().screen, p)) s_known[cw.Def().screen] = p;
}

} // namespace game::wnd::detail
