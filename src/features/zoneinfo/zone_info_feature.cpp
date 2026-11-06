// The zone info window: a bordered, sizable game window (see game/ui/custom_window.h) with up to
// three lines, each with its own switch: the zone's name, the result of the player's last Sense
// Heading and the result of the player's last /loc.
//
// Sense Heading and /loc are decided by the client and only printed to the chat, so those lines come
// from the chat line (game/eqchat.h). We never use the skill or the command: a line only changes
// when the player does. The Last Location line has a pin button: while it is pressed in, the line
// keeps the location it showed when pinned, through new /loc results and zoning.
#include "features/zoneinfo/zone_info.h"
#include "core/host.h"
#include "game/eqchat.h"
#include "game/eqgame.h"
#include "game/strings.h"
#include "game/ui/custom_window.h"
#include "game/ui/sidl_xml.h"
#include "overlay/widgets.h"
#include <imgui.h>
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

namespace zoneinfo {

Settings cfg;

namespace {

namespace sidl = game::wnd::sidl;

// The label slots: the enabled lines are packed from the top, so switching one off leaves no gap.
// Each slot also has a checkbox at its right end, drawn with the skin's own checkbox art; only the
// one on the Last Location line is shown, as its pin.
constexpr int kSlots = 3;
const char* const kSlotIds[kSlots] = { "Line1", "Line2", "Line3" };
const char* const kPinIds[kSlots]  = { "Pin1", "Pin2", "Pin3" };

constexpr char kScreen[]  = "P99PlusZoneInfoWnd";   // also the window's section in the character's UI ini
constexpr int  kLineTop = 2, kLineHeight = 16, kLineStep = 17;

// The labels stretch with the window (AutoStretch + anchors, like the stock group search window's).
std::string BuildXml()
{
    std::vector<std::string> parts;
    sidl::Screen screen = { kScreen, 1160, 10, 300, 63, {} };
    std::vector<std::string> labels, pins;
    for (int i = 0; i < kSlots; i++) {
        const int top = kLineTop + kLineStep * i, bottom = top + kLineHeight;
        const std::string n = std::to_string(i + 1);
        labels.push_back(sidl::Label("P99PlusZoneInfoLine" + n, kSlotIds[i], { top, bottom, 22, 22, true }, "-"));
        pins.push_back(sidl::CheckButton("P99PlusZoneInfoPin" + n, kPinIds[i], { top + 2, top + 14, 16, 4, false }));   // 12x12, centred on the line
        screen.pieces.push_back("P99PlusZoneInfoLine" + n);
    }
    for (int i = 0; i < kSlots; i++) screen.pieces.push_back("P99PlusZoneInfoPin" + std::to_string(i + 1));
    parts.insert(parts.end(), labels.begin(), labels.end());
    parts.insert(parts.end(), pins.begin(), pins.end());
    parts.push_back(sidl::BorderedScreen(screen));
    return sidl::Document(parts);
}

game::wnd::CustomWindowDef MakeDef()
{
    game::wnd::CustomWindowDef d;
    d.screen = kScreen;
    d.xml = BuildXml();
    d.dragByBody = true;   // no title bar, so the body is the only handle
    return d;
}

game::wnd::CustomWindow s_window(MakeDef());

// The last result of a chat-line command (Sense Heading, /loc). Kept across a core reload.
struct Result {
    char      text[64];  // the part of the line that varies; only meaningful when `known`
    ULONGLONG when;      // GetTickCount64() of the result
    int       zoneId;    // the zone it came in; a zone change forgets it
    bool      known;
};
struct Results {
    Result heading;
    Result loc;        // the latest /loc (in this zone)
    Result pinnedLoc;  // what the Last Location line keeps while pinned
    bool   pinned;
};
Results s_results = {};
bool s_restored = false;
constexpr const char* kStashKey = "zoneinfo.results";

// The game's texts for the results, with English for when its string table can't be read.
const game::chat::Template kSenseHeading = { game::str::kSenseHeading, "You think you are heading %1." };
const game::chat::Template kSenseFail    = { game::str::kSenseFail, "You have no idea what direction you are facing." };
const game::chat::Template kLocation     = { game::str::kLocation, "Your Location is %1, %2, %3" };

void Remember(Result& r, const std::string& text)
{
    snprintf(r.text, sizeof(r.text), "%s", text.c_str());
    r.when = GetTickCount64();
    r.zoneId = game::ZoneId();
    r.known = true;
}

// Every chat line: is it a Sense Heading or /loc result?
void OnChat(const char* text, int /*color*/)
{
    std::string arg;
    if (game::chat::Is(text, kSenseFail)) Remember(s_results.heading, "unsure");
    else if (game::chat::Match(text, kSenseHeading, arg)) Remember(s_results.heading, arg);
    else if (game::chat::Match(text, kLocation, arg)) Remember(s_results.loc, arg);
}

std::string AgeText(ULONGLONG when)
{
    const ULONGLONG secs = (GetTickCount64() - when) / 1000;
    char age[32];
    if (secs < 60)        snprintf(age, sizeof(age), "%llus", secs);
    else if (secs < 3600) snprintf(age, sizeof(age), "%llum", secs / 60);
    else                  snprintf(age, sizeof(age), "%lluh", secs / 3600);
    return age;
}

std::string ResultText(const char* label, const Result& r)
{
    if (!r.known) return std::string(label) + ": ?";
    return std::string(label) + ": " + r.text + " (" + AgeText(r.when) + " ago)";
}

void UpdateWindow()
{
    if (!s_window.Exists()) return;
    if (!cfg.pin_location) s_results.pinned = false;   // switched off: back to the latest /loc

    std::string lines[kSlots];
    int n = 0, locSlot = -1;
    if (cfg.zone_name) {
        std::string zone;
        lines[n++] = game::ZoneName(zone) ? zone : "?";
    }
    if (cfg.heading) lines[n++] = ResultText("Heading", s_results.heading);
    if (cfg.location) {
        locSlot = n;
        lines[n++] = ResultText("Last Location", s_results.pinned ? s_results.pinnedLoc : s_results.loc);
    }
    for (int i = 0; i < kSlots; i++) s_window.SetText(kSlotIds[i], lines[i]);

    // The pin button is on the Last Location line. Pinning keeps the location shown now; unpinning
    // goes back to the latest /loc.
    const int pinSlot = cfg.pin_location ? locSlot : -1;
    for (int i = 0; i < kSlots; i++) s_window.ShowChild(kPinIds[i], i == pinSlot);
    if (pinSlot >= 0 && s_window.BindCheck(kPinIds[pinSlot], s_results.pinned) && s_results.pinned)
        s_results.pinnedLoc = s_results.loc;
}

void OnFrame()
{
    if (!s_restored) {
        s_restored = true;
        stash::GetValue(kStashKey, s_results);
        stash::Clear(kStashKey);
        game::chat::Listen(OnChat);
    }
    s_window.SetWanted(cfg.enabled && (cfg.zone_name || cfg.heading || cfg.location));

    // A result from another zone means nothing here. A pinned location is kept apart and stays.
    const int zone = game::ZoneId();
    if (zone) {
        if (s_results.heading.known && zone != s_results.heading.zoneId) s_results.heading.known = false;
        if (s_results.loc.known && zone != s_results.loc.zoneId) s_results.loc.known = false;
    }
    UpdateWindow();
}

void OnShutdown()
{
    stash::PutValue(kStashKey, s_results);
}

const Setting kSettings[] = {
    BoolSetting("zone_info", "enabled", &cfg.enabled,
                "Show a small game window with the zone you are in, your last Sense Heading result and your last /loc."),
    BoolSetting("zone_info", "zone_name", &cfg.zone_name, "Show the name of the zone you are in."),
    BoolSetting("zone_info", "heading", &cfg.heading, "Show the result of your last Sense Heading."),
    BoolSetting("zone_info", "location", &cfg.location, "Show the result of your last /loc."),
    BoolSetting("zone_info", "pin_location", &cfg.pin_location,
                "Show a pin button on the Last Location line that keeps the location shown while pressed in."),
};

void DrawMenu()
{
    ui::WindowStatusHint(s_window);
    ImGui::Checkbox("Zone name", &cfg.zone_name);
    ImGui::Checkbox("Heading", &cfg.heading);
    ImGui::Checkbox("Last Location", &cfg.location);
    ImGui::Indent();
    ImGui::BeginDisabled(!cfg.location);
    ImGui::Checkbox("Enable Pin Location", &cfg.pin_location);
    ImGui::EndDisabled();
    ImGui::Unindent();
    if (!game::chat::Ready()) ui::Hint("Heading and location unavailable: the chat hook failed.");
}

} // namespace

const Feature feature = {
    "Zone info",
    "A game window with your zone, heading and last /loc.",
    "Show the zone info window", &cfg.enabled,
    kSettings, std::size(kSettings), OnFrame, DrawMenu, OnShutdown,
};

} // namespace zoneinfo
