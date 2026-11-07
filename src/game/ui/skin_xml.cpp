// Fitting our screens to the loaded skin (detail.h): the frame its titled windows use, and copies
// of its buttons (sidl::BorrowedButton). Read from the skin's own EQUI_*.xml files; a skin may
// leave files out (the game takes Default's), so what the skin folder hasn't got comes from Default.
#include "core/log.h"
#include "game/ui/detail.h"
#include "game/ui/sidl_xml.h"
#include "util/text.h"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

namespace game::wnd::detail {

namespace {

bool ContainsNoCase(const std::string& s, size_t from, size_t to, const char* what)
{
    const size_t n = strlen(what);
    for (size_t i = from; i + n <= to; ++i)
        if (_strnicmp(s.c_str() + i, what, n) == 0) return true;
    return false;
}

// Counts the frames (<DrawTemplate>) of the titled screens (<Style_Titlebar>true) in one XML file.
void CountTitledFrames(const std::string& xml, std::map<std::string, int>& counts)
{
    static const char kOpen[] = "<Screen ", kClose[] = "</Screen>", kTpl[] = "<DrawTemplate>";
    for (size_t at = xml.find(kOpen); at != std::string::npos; at = xml.find(kOpen, at)) {
        const size_t end = xml.find(kClose, at);
        if (end == std::string::npos) break;
        const size_t tpl = xml.find(kTpl, at);
        if (tpl < end && ContainsNoCase(xml, at, end, "<Style_Titlebar>true")) {
            const size_t from = tpl + sizeof(kTpl) - 1, to = xml.find('<', from);
            if (to < end) {
                const std::string name = text::Trim(xml.substr(from, to - from));
                if (!name.empty()) ++counts[name];
            }
        }
        at = end;
    }
}

// True for the files we write into a skin (the merged EQUI.xml, our screens).
bool OurFile(const char* name)
{
    if (_stricmp(name, MergedFile().c_str()) == 0) return true;
    for (const CustomWindow* w : Registry())
        if (_stricmp(name, ScreenFile(w->Def()).c_str()) == 0) return true;
    return false;
}

// The skin's own EQUI_*.xml files in `dir` (not ours), each read and passed to `fn` until it
// returns true.
template <class Fn> void ForEachSkinXml(const std::string& dir, Fn fn)
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((dir + "EQUI_*.xml").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        std::string xml;
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !OurFile(fd.cFileName) && ReadText(dir + fd.cFileName, xml) &&
            fn(xml))
            break;
    } while (FindNextFileA(h, &fd));
    FindClose(h);
}

// One of the skin's buttons, as far as BorrowedButton needs it.
struct SkinButton {
    int         cx = 0, cy = 0;
    std::string look;   // the whole <ButtonDrawTemplate> element
};

std::string Between(const std::string& s, size_t from, size_t to, const char* open, const char* close)
{
    const size_t a = s.find(open, from);
    if (a == std::string::npos || a >= to) return std::string();
    const size_t from2 = a + strlen(open), b = s.find(close, from2);
    if (b == std::string::npos || b >= to) return std::string();
    return s.substr(from2, b - from2);
}

bool FindButtonIn(const std::string& xml, const std::string& item, SkinButton& out)
{
    for (size_t at = xml.find("<Button "); at != std::string::npos; at = xml.find("<Button ", at + 1)) {
        const size_t head = xml.find('>', at), end = xml.find("</Button>", at);
        if (head == std::string::npos || end == std::string::npos) return false;
        if (xml.find("\"" + item + "\"", at) > head) continue;   // another button
        const std::string size = Between(xml, head, end, "<Size>", "</Size>");
        const std::string look = Between(xml, head, end, "<ButtonDrawTemplate>", "</ButtonDrawTemplate>");
        out.cx = atoi(Between(size, 0, size.size(), "<CX>", "</CX>").c_str());
        out.cy = atoi(Between(size, 0, size.size(), "<CY>", "</CY>").c_str());
        // Only the animation names are taken, written in our own layout (not the skin's text).
        out.look.clear();
        for (const char* state : { "Normal", "Pressed", "Flyby", "Disabled", "PressedFlyby" }) {
            const std::string open = std::string("<") + state + ">", close = std::string("</") + state + ">";
            const std::string anim = text::Trim(Between(look, 0, look.size(), open.c_str(), close.c_str()));
            if (!anim.empty()) out.look += "\t\t\t" + open + anim + close + "\n";
        }
        if (out.cx <= 0 || out.cy <= 0 || out.look.empty()) return false;
        out.look = "<ButtonDrawTemplate>\n" + out.look + "\t\t</ButtonDrawTemplate>";
        return true;
    }
    return false;
}

bool FindButton(const std::string& skin, const std::string& item, SkinButton& out)
{
    for (const std::string& dir : { skin, DefaultSkin() }) {
        bool found = false;
        ForEachSkinXml(dir, [&](const std::string& xml) { return found = FindButtonIn(xml, item, out); });
        if (found) return true;
    }
    return false;
}

// The attribute `name="..."` inside a marker comment.
std::string Attr(const std::string& s, size_t from, size_t to, const char* name)
{
    return Between(s, from, to, (std::string(name) + "=\"").c_str(), "\"");
}

// Replaces BorrowedButton's markers with the skin button's place (its size, centred in the box)
// and look. A button the skin and Default both lack keeps the box and no art.
void FillBorrowed(std::string& xml, const std::string& skin)
{
    std::map<std::string, SkinButton> found;
    const auto lookup = [&](const std::string& item) -> const SkinButton* {
        auto it = found.find(item);
        if (it == found.end()) {
            SkinButton b;
            if (!FindButton(skin, item, b)) Log("custom windows: this skin has no button %s to copy", item.c_str());
            it = found.emplace(item, b).first;
        }
        return it->second.look.empty() ? nullptr : &it->second;
    };
    for (size_t at = xml.find("<!--"); at != std::string::npos; at = xml.find("<!--", at)) {
        const size_t end = xml.find("-->", at);
        if (end == std::string::npos) break;
        const std::string what = Between(xml, at, end, "<!--", " ");
        std::string with;
        if (what == sidl::kBorrowPlace) {
            int x = 0, y = 0, cx = 0, cy = 0;
            sscanf(Attr(xml, at, end, "box").c_str(), "%d %d %d %d", &x, &y, &cx, &cy);
            const SkinButton* b = lookup(Attr(xml, at, end, "from"));
            const int w = b ? std::min(b->cx, cx) : cx, h = b ? std::min(b->cy, cy) : cy;
            with = "<Location><X>" + std::to_string(x + (cx - w) / 2) + "</X><Y>" + std::to_string(y + (cy - h) / 2) +
                   "</Y></Location><Size><CX>" + std::to_string(w) + "</CX><CY>" + std::to_string(h) + "</CY></Size>";
        } else if (what == sidl::kBorrowLook) {
            const SkinButton* b = lookup(Attr(xml, at, end, "from"));
            with = b ? b->look : std::string();
        } else {
            at = end;
            continue;
        }
        xml.replace(at, end + 3 - at, with);
        at += with.size();
    }
}

} // namespace

std::string TitledFrame(const std::string& skin)
{
    for (const std::string& dir : { skin, DefaultSkin() }) {
        std::map<std::string, int> counts;
        ForEachSkinXml(dir, [&](const std::string& xml) { CountTitledFrames(xml, counts); return false; });
        const auto best = std::max_element(counts.begin(), counts.end(),
                                           [](const auto& a, const auto& b) { return a.second < b.second; });
        if (best != counts.end()) return best->first;
    }
    return "WDT_Rounded";
}

std::string ForSkin(std::string xml, const std::string& frame, const std::string& skin)
{
    const std::string mark = std::string("<DrawTemplate>") + sidl::kTitledFrame + "</DrawTemplate>";
    const std::string with = "<DrawTemplate>" + frame + "</DrawTemplate>";
    for (size_t at = xml.find(mark); at != std::string::npos; at = xml.find(mark, at + with.size()))
        xml.replace(at, mark.size(), with);
    FillBorrowed(xml, skin);
    return xml;
}

} // namespace game::wnd::detail
