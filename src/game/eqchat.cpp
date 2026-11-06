#include "game/eqchat.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "game/strings.h"
#include "util/hook.h"
#include <algorithm>
#include <cstring>
#include <vector>

namespace game::chat {

namespace {

using ChatPrint_t = int (__thiscall*)(void* self, const char* text, int color, int flag);
ChatPrint_t oChatPrint = nullptr;

std::vector<Listener> s_listeners;
bool s_tried = false;
bool s_ok = false;

int __fastcall Hk_ChatPrint(void* self, void* /*edx*/, const char* text, int color, int flag)
{
    hook::Busy busy;
    if (text)
        for (Listener fn : s_listeners) fn(text, color);
    return oChatPrint(self, text, color, flag);
}

// The entry's text; `needArg`: one that has no %1 isn't the entry we mean.
const char* Pattern(const Template& t, bool needArg)
{
    const char* p = game::String(t.id);
    return (p && (!needArg || strstr(p, "%1"))) ? p : t.english;
}

} // namespace

bool Init()
{
    if (s_tried) return s_ok;
    if (!game::Available()) return false;
    s_tried = true;
    s_ok = hook::Install("chat", (void*)addr::kChatPrint, (void*)&Hk_ChatPrint, (void**)&oChatPrint,
                         kChatPrintPrologue) == hook::Result::Ok;
    if (!s_ok) Log("chat: unavailable");
    return s_ok;
}

bool Ready() { return s_ok; }

void Listen(Listener fn)
{
    if (std::find(s_listeners.begin(), s_listeners.end(), fn) == s_listeners.end()) s_listeners.push_back(fn);
}

bool Is(const char* text, const Template& t)
{
    return strcmp(text, Pattern(t, false)) == 0;
}

bool Match(const char* text, const Template& t, std::string& arg)
{
    const char* pattern = Pattern(t, true);
    const char* first = strchr(pattern, '%');
    const char* last = strrchr(pattern, '%');
    if (!first || !last || !last[1]) return false;
    const size_t pre = (size_t)(first - pattern);
    const char* suffix = last + 2;
    const size_t post = strlen(suffix);
    const size_t n = strlen(text);
    if (n <= pre + post || strncmp(text, pattern, pre) != 0 || strcmp(text + n - post, suffix) != 0) return false;
    arg.assign(text + pre, n - pre - post);
    return true;
}

} // namespace game::chat
