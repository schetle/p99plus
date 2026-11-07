// Getting the custom screens into the skin: the UI loader hook writes their XML next to the skin's
// files and has the game load a copy of EQUI.xml that includes them.
#include "core/app.h"
#include "core/host.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "game/ui/detail.h"
#include "game/ui/driver.h"
#include "game/ui/xstr.h"
#include "util/hook.h"
#include "util/mem.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace game::wnd::detail {

InjectState g_inject = {};

std::string MergedFile() { return std::string("EQUI_") + kAppName + ".xml"; }
std::string ScreenFile(const CustomWindowDef& d) { return "EQUI_" + d.screen + ".xml"; }
std::string DefaultSkin() { return g_gameDir + "UIFiles\\Default\\"; }

bool ReadText(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

namespace {

using Loader_t = int (__thiscall*)(void* self, const void* dir, const void* second, const void* file);
Loader_t oLoader = nullptr;

struct SkinStash { char path[260]; };
constexpr const char* kSkinKey = "wnd.skin";

unsigned Hash(const char* data, size_t n, unsigned h)
{
    for (size_t i = 0; i < n; ++i) h = (h ^ (unsigned char)data[i]) * 16777619u;   // FNV-1a
    return h;
}

// What a UI load takes in of a window: its XML.
unsigned Hash(const CustomWindowDef& d) { return Hash(d.xml.data(), d.xml.size(), 2166136261u); }

void NoteLoaded()
{
    g_inject.numLoaded = 0;
    for (CustomWindow* w : Registry()) {
        Access::Stale(*w) = false;
        if (g_inject.numLoaded == kMaxWindows) continue;
        LoadedScreen& l = g_inject.loaded[g_inject.numLoaded++];
        strncpy(l.screen, w->Def().screen.c_str(), sizeof(l.screen) - 1);
        l.screen[sizeof(l.screen) - 1] = 0;
        l.hash = Hash(w->Def());
    }
}

// Replaces the file only when its content differs, so a normal start touches nothing.
bool WriteIfChanged(const std::string& path, const std::string& content)
{
    std::string current;
    if (ReadText(path, current) && current == content) return true;
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << content;
    return f.good();
}

// The skin directory the game passes to the loader, as a path with a trailing '\' under the game
// folder. Only "UIFiles\<skin>" is accepted: we never write anywhere else.
bool SkinPath(std::string dir, std::string& path)
{
    std::replace(dir.begin(), dir.end(), '/', '\\');
    while (!dir.empty() && dir.back() == '\\') dir.pop_back();
    while (dir.rfind(".\\", 0) == 0) dir.erase(0, 2);
    static const char kRoot[] = "uifiles\\";
    std::string lower = dir;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (lower.rfind(kRoot, 0) != 0 || dir.size() == sizeof(kRoot) - 1 || dir.find("..") != std::string::npos ||
        dir.find(':') != std::string::npos)
        return false;
    path = g_gameDir + dir + "\\";
    return true;
}

// Writes the screens and the copy of EQUI.xml that includes them into the skin directory.
bool WriteInjected(const std::string& dir)
{
    std::string skin;
    if (!SkinPath(dir, skin)) {
        Log("custom windows: unexpected skin directory '%s'; not adding windows", dir.c_str());
        return false;
    }

    std::string equi;
    if (!ReadText(skin + "EQUI.xml", equi) && !ReadText(DefaultSkin() + "EQUI.xml", equi)) {
        Log("custom windows: EQUI.xml not found in '%s'", dir.c_str());
        return false;
    }
    const size_t end = equi.rfind("</Composite>");
    if (end == std::string::npos) {
        Log("custom windows: this skin's EQUI.xml has no </Composite>; not adding windows");
        return false;
    }

    std::string includes;
    for (const CustomWindow* w : Registry()) includes += "\t\t<Include>" + ScreenFile(w->Def()) + "</Include>\n";
    const std::string merged = equi.substr(0, end) + includes + equi.substr(end);

    const std::string frame = TitledFrame(skin);
    static std::string s_loggedFrame;
    if (frame != s_loggedFrame) {
        Log("custom windows: titled windows use this skin's %s", frame.c_str());
        s_loggedFrame = frame;
    }
    bool ok = WriteIfChanged(skin + MergedFile(), merged);
    for (const CustomWindow* w : Registry()) ok = WriteIfChanged(skin + ScreenFile(w->Def()), ForSkin(w->Def().xml, frame, skin)) && ok;
    if (!ok) Log("custom windows: could not write to '%s'", skin.c_str());
    return ok;
}

int __fastcall Hk_Loader(void* self, void* /*edx*/, const void* dir, const void* second, const void* file)
{
    hook::Busy busy;
    const std::string name = XStrToString((uintptr_t)file);
    if (_stricmp(name.c_str(), "EQUI.xml") == 0) {
        std::string skin;
        SkinStash st = {};
        if (SkinPath(XStrToString((uintptr_t)dir), skin) && skin.size() < sizeof(st.path)) {
            memcpy(st.path, skin.c_str(), skin.size());
            stash::PutValue(kSkinKey, st);
        }
    }
    if (Registry().empty() || g_inject.failed || _stricmp(name.c_str(), "EQUI.xml") != 0)
        return oLoader(self, dir, second, file);

    if (!WriteInjected(XStrToString((uintptr_t)dir))) return oLoader(self, dir, second, file);

    const XStr merged(MergedFile().c_str());
    const int r = oLoader(self, dir, second, merged.get());
    if (mem::Read<unsigned char>((uintptr_t)self + sidlmgr::kLoaderFailed) == 0) {
        Log("custom windows: UI loaded with %zu added window(s)", Registry().size());
        NoteLoaded();
        return r;
    }

    // The game rejected our XML. Hand the failure back: the caller (0x442d80) then tears the UI down
    // and loads the Default skin, which passes through here untouched now. Loading the plain skin
    // again here, into the half-filled loader with no reset, failed as well and left it corrupt, so
    // the game's own fallback failed too and the client quit ("Error in your GUI XML files").
    g_inject.failed = true;
    g_inject.numLoaded = 0;
    Log("custom windows: the game rejected the added windows (\"%s\"); custom windows are off until the game restarts, "
        "and the game falls back to the Default skin",
        XStrToString((uintptr_t)self + sidlmgr::kLoaderError).c_str());
    return r;
}

} // namespace

bool InstallInject()
{
    return hook::Install("UI loader", (void*)addr::kUiLoader, (void*)&Hk_Loader, (void**)&oLoader,
                         kUiLoaderPrologue) == hook::Result::Ok;
}

void CheckStale()
{
    for (CustomWindow* w : Registry())
        for (int i = 0; i < g_inject.numLoaded; ++i) {
            if (strcmp(g_inject.loaded[i].screen, w->Def().screen.c_str()) != 0) continue;
            const bool stale = g_inject.loaded[i].hash != Hash(w->Def());
            if (stale && !Access::Stale(*w))
                Log("custom window %s: its layout changed since the UI was loaded; reload the UI (/loadskin) to use it",
                    w->Def().screen.c_str());
            Access::Stale(*w) = stale;
        }
}

} // namespace game::wnd::detail

namespace game::wnd {

std::string SkinFolder()
{
    detail::SkinStash st = {};
    if (!stash::GetValue(detail::kSkinKey, st)) return {};
    st.path[sizeof(st.path) - 1] = 0;
    return st.path;
}

} // namespace game::wnd
