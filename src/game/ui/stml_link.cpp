#include "game/ui/stml_link.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "game/ui/xstr.h"
#include "util/hook.h"
#include <cstring>

namespace game::wnd {

namespace {

// What CStmlWnd::ActivateLink (addr::kStmlActivateLink) takes by value: the parser fills it from
// the <a> tag (0x5aaf..): 0 = HREF "FILE:///...", 5 = SOVREPORT, 6 = WNDNOTIFY, 8 = HREF "http...".
struct LinkInfo {
    int       type;
    int       unknown1;
    uintptr_t text;       // CXStr: the attribute's value; ActivateLink frees it
    int       unknown2;
};
static_assert(sizeof(LinkInfo) == 16, "ActivateLink takes 16 bytes (ret 0x10)");
constexpr int kLinkWndNotify = 6;

using ActivateLink_t = void (__thiscall*)(void* stml, LinkInfo link);
ActivateLink_t oActivateLink = nullptr;

uintptr_t   s_box = 0;        // the box of the link clicked last, 0 for none waiting
std::string s_target;

void __fastcall Hk_ActivateLink(void* stml, void* /*edx*/, LinkInfo link)
{
    hook::Busy busy;
    if (link.type == kLinkWndNotify) {
        const std::string value = XStrToString((uintptr_t)&link.text, 4096);
        const size_t n = sizeof(kLinkMarker) - 1;
        if (value.size() >= n && _strnicmp(value.c_str(), kLinkMarker, n) == 0) {
            s_box = (uintptr_t)stml;
            s_target = value.substr(n);
            FreeXStr(link.text);   // what the game would have done with it
            return;                // ours: the game never sees it
        }
    }
    oActivateLink(stml, link);
}

} // namespace

bool TakeLink(Wnd box, std::string& target)
{
    static bool tried = false;
    if (box && !tried && game::Available()) {
        tried = true;
        hook::Install("STML links", (void*)addr::kStmlActivateLink, (void*)&Hk_ActivateLink, (void**)&oActivateLink,
                      kStmlActivateLinkPrologue);
    }
    if (!box || !s_box || s_box != box.ptr()) return false;
    s_box = 0;
    target = s_target;
    return true;
}

} // namespace game::wnd
