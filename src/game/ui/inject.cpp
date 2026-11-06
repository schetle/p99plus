// Getting the custom screens into the skin: the UI loader hook writes their XML next to the skin's
// files and has the game load a copy of EQUI.xml that includes them.
#include "core/app.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "game/ui/detail.h"
#include "game/ui/xstr.h"
#include "util/hook.h"
#include "util/mem.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace game::wnd::detail {

InjectState g_inject = {};

namespace {

using Loader_t = int (__thiscall*)(void* self, const void* dir, const void* second, const void* file);
Loader_t oLoader = nullptr;

const std::string kMergedFile = std::string("EQUI_") + kAppName + ".xml";

std::string FileName(const CustomWindowDef& d) { return "EQUI_" + d.screen + ".xml"; }

unsigned Hash(const char* data, size_t n, unsigned h)
{
    for (size_t i = 0; i < n; ++i) h = (h ^ (unsigned char)data[i]) * 16777619u;   // FNV-1a
    return h;
}

// What a UI load takes in of a window: its XML and the files next to it.
unsigned Hash(const CustomWindowDef& d)
{
    unsigned h = Hash(d.xml.data(), d.xml.size(), 2166136261u);
    for (const CustomFile& f : d.files) h = Hash(f.content.data(), f.content.size(), h);
    return h;
}

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

bool ReadFile(const std::string& path, std::string& out)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

// Replaces the file only when its content differs, so a normal start touches nothing.
bool WriteIfChanged(const std::string& path, const std::string& content)
{
    std::string current;
    if (ReadFile(path, current) && current == content) return true;
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
    if (!ReadFile(skin + "EQUI.xml", equi) && !ReadFile(g_gameDir + "UIFiles\\Default\\EQUI.xml", equi)) {
        Log("custom windows: EQUI.xml not found in '%s'", dir.c_str());
        return false;
    }
    const size_t end = equi.rfind("</Composite>");
    if (end == std::string::npos) {
        Log("custom windows: this skin's EQUI.xml has no </Composite>; not adding windows");
        return false;
    }

    std::string includes;
    for (const CustomWindow* w : Registry()) includes += "\t\t<Include>" + FileName(w->Def()) + "</Include>\n";
    const std::string merged = equi.substr(0, end) + includes + equi.substr(end);

    bool ok = WriteIfChanged(skin + kMergedFile, merged);
    for (const CustomWindow* w : Registry()) {
        const CustomWindowDef& d = w->Def();
        ok = WriteIfChanged(skin + FileName(d), d.xml) && ok;
        for (const CustomFile& f : d.files) ok = WriteIfChanged(skin + f.name, f.content) && ok;
    }
    if (!ok) Log("custom windows: could not write to '%s'", skin.c_str());
    return ok;
}

int __fastcall Hk_Loader(void* self, void* /*edx*/, const void* dir, const void* second, const void* file)
{
    hook::Busy busy;
    const std::string name = XStrToString((uintptr_t)file);
    if (Registry().empty() || g_inject.failed || _stricmp(name.c_str(), "EQUI.xml") != 0)
        return oLoader(self, dir, second, file);

    if (!WriteInjected(XStrToString((uintptr_t)dir))) return oLoader(self, dir, second, file);

    const XStr merged(kMergedFile.c_str());
    const int r = oLoader(self, dir, second, merged.get());
    if (mem::Read<unsigned char>((uintptr_t)self + sidlmgr::kLoaderFailed) == 0) {
        Log("custom windows: UI loaded with %zu added window(s)", Registry().size());
        NoteLoaded();
        return r;
    }

    // The game rejected our XML. Load the skin as it is, and leave it alone from now on.
    g_inject.failed = true;
    g_inject.numLoaded = 0;
    Log("custom windows: the game rejected the added windows; loading the plain skin (custom windows off)");
    return oLoader(self, dir, second, file);
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
