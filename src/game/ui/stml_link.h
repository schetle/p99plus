#pragma once
// Links in our STML boxes. The game's STML has `<a WndNotify="code,data">text</a>`: a click on it
// tells the box's parent WndNotification(box, code, data). Ours carry a marker instead of a code,
// and a hook on the box's link activation takes them before the game sees them, so a feature can
// poll for the click (CustomWindow::LinkClicked).
#include "game/ui/wnd.h"
#include <string>

namespace game::wnd {

inline constexpr char kLinkMarker[] = "P99:";   // the WndNotify value of our links starts with it

// `stml` (already STML) as a link to `target`. The target can't hold `"`, `<` or `>` (dropped).
inline std::string StmlLink(const std::string& target, const std::string& stml)
{
    std::string value;
    for (char c : target)
        if (c != '"' && c != '<' && c != '>') value += c;
    return std::string("<a WndNotify=\"") + kLinkMarker + value + "\">" + stml + "</a>";
}

// The target of a link the player clicked in `box` since the last call. Call every frame the box
// is shown; the hook goes in on the first call.
bool TakeLink(Wnd box, std::string& target);

} // namespace game::wnd
