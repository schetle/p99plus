// Loads the core DLL and swaps it for a new build while the game runs.
//
// The core file (<app>_core.dll next to eqgame.exe) is never loaded itself: each load copies it
// to <app>_core.live.<n>.dll first. Windows keeps a loaded DLL locked, and on Linux overwriting a
// mapped DLL changes the pages under the running image, so the original stays free to replace.
//
// Every Present starts with Tick(), before any core code has run in that frame. The core is
// swapped only there, and only when nothing is inside the core on any thread: g_coreCalls counts
// the host's calls into it, CoreApi::busy the core's own hook detours.
#include "core/app.h"
#include "core/log.h"
#include "host/host.h"
#include <map>
#include <string>
#include <vector>

namespace host {

const CoreApi* volatile g_core = nullptr;
volatile LONG g_coreCalls = 0;

namespace loader {

namespace {

constexpr DWORD kPollMs = 500;    // how often the core file is checked for a new build
constexpr DWORD kDrainMs = 200;   // how long to wait for other threads to leave the core

HMODULE     s_module = nullptr;   // the loaded shadow copy
std::string s_modulePath;
int         s_loadCount = 0;
int         s_shadowSeq = 0;
bool        s_requested = false;
bool        s_started = false;

struct Stamp {
    FILETIME time{};
    DWORD    size = 0;
    bool operator==(const Stamp& o) const
    { return time.dwLowDateTime == o.time.dwLowDateTime && time.dwHighDateTime == o.time.dwHighDateTime && size == o.size; }
    bool operator!=(const Stamp& o) const { return !(*this == o); }
};
Stamp s_loadedStamp;              // the file as of the last load attempt
Stamp s_pendingStamp;             // a changed file seen once; loaded when the next poll sees it unchanged
bool  s_pending = false;
DWORD s_lastPoll = 0;
DWORD s_lastDeferLog = 0;
bool  s_loggedMissing = false;

std::map<std::string, std::vector<char>> s_stash;
HostApi s_host;

std::string CorePath() { return g_gameDir + AppFileName("_core.dll"); }
std::string ShadowPath(int n) { return g_gameDir + AppFileName("_core.live.") + std::to_string(n) + ".dll"; }

bool GetStamp(Stamp& out)
{
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExA(CorePath().c_str(), GetFileExInfoStandard, &a)) return false;
    out.time = a.ftLastWriteTime;
    out.size = a.nFileSizeLow;
    return true;
}

// Removes shadow copies left by an earlier session (the game exited with a core loaded).
void DeleteStaleShadows()
{
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((g_gameDir + AppFileName("_core.live.*.dll")).c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do DeleteFileA((g_gameDir + fd.cFileName).c_str());
    while (FindNextFileA(h, &fd));
    FindClose(h);
}

bool WaitZero(volatile LONG* counter, DWORD ms)
{
    const DWORD start = GetTickCount();
    while (*counter != 0) {
        if (GetTickCount() - start >= ms) return false;
        Sleep(1);
    }
    return true;
}

// ---- HostApi callbacks -------------------------------------------------------------------------
void RequestReload() { s_requested = true; }

void Stash(const char* key, const void* data, size_t size)
{
    if (!data) { s_stash.erase(key); return; }
    s_stash[key].assign((const char*)data, (const char*)data + size);
}

const void* Unstash(const char* key, size_t* size)
{
    auto it = s_stash.find(key);
    if (it == s_stash.end()) return nullptr;
    if (size) *size = it->second.size();
    return it->second.data();
}

// ---- load / unload -------------------------------------------------------------------------------
bool Load()
{
    const std::string path = ShadowPath(++s_shadowSeq);
    if (!CopyFileA(CorePath().c_str(), path.c_str(), FALSE)) {
        Log("loader: cannot copy %s (error %lu)", CorePath().c_str(), GetLastError());
        return false;
    }
    HMODULE m = LoadLibraryA(path.c_str());
    if (!m) {
        Log("loader: LoadLibrary %s failed (error %lu)", path.c_str(), GetLastError());
        DeleteFileA(path.c_str());
        return false;
    }
    auto init = (CoreInit_t)GetProcAddress(m, kCoreInitName);
    if (!init) {
        Log("loader: %s has no %s export; not a core", AppFileName("_core.dll").c_str(), kCoreInitName);
        FreeLibrary(m);
        DeleteFileA(path.c_str());
        return false;
    }

    s_host.version       = kCoreApiVersion;
    s_host.gameDir       = g_gameDir.c_str();
    s_host.hwnd          = window::Hwnd();
    s_host.loadCount     = s_loadCount + 1;
    s_host.log           = LogLine;
    s_host.requestReload = RequestReload;
    s_host.stash         = Stash;
    s_host.unstash       = Unstash;

    const CoreApi* api = init(&s_host);
    if (!api || api->version != kCoreApiVersion) {
        Log("loader: the core was built for a different %s (interface version mismatch); "
            "install both DLLs from the same build and restart the game", "dinput8.dll");
        FreeLibrary(m);
        DeleteFileA(path.c_str());
        return false;
    }

    ++s_loadCount;
    s_module = m;
    s_modulePath = path;
    g_core = api;
    return true;
}

// False when another thread is still inside the core: the core stays loaded and in use.
bool Unload()
{
    const CoreApi* api = g_core;
    g_core = nullptr;                                 // new calls pass through from here on
    MemoryBarrier();
    if (!WaitZero(&g_coreCalls, kDrainMs)) {
        g_core = api;
        return false;
    }

    api->shutdown();                                  // removes the core's hooks and patches
    // A thread can have entered a detour just before its hook came out; give it time to leave.
    Sleep(5);
    if (WaitZero(api->busy, kDrainMs)) {
        FreeLibrary(s_module);
        DeleteFileA(s_modulePath.c_str());
    } else {
        Log("loader: a hook was still running in the old core; leaving it loaded");
    }
    s_module = nullptr;
    s_modulePath.clear();
    return true;
}

} // namespace

void Tick()
{
    if (!s_started) {
        s_started = true;
        DeleteStaleShadows();
        s_lastPoll = GetTickCount();
        Stamp st;
        if (GetStamp(st)) { s_loadedStamp = st; s_requested = true; }   // first load: right away
        else { Log("loader: %s not found; waiting for it", CorePath().c_str()); s_loggedMissing = true; }
    }

    const DWORD now = GetTickCount();
    if (now - s_lastPoll >= kPollMs) {
        s_lastPoll = now;
        Stamp st;
        if (!GetStamp(st)) {
            if (!s_loggedMissing) { Log("loader: %s is gone", CorePath().c_str()); s_loggedMissing = true; }
            s_pending = false;
        } else if (st != s_loadedStamp) {
            // Changed (or appeared): wait one more poll so a file still being written isn't loaded.
            if (s_pending && st == s_pendingStamp) s_requested = true;
            else { s_pending = true; s_pendingStamp = st; }
        }
    }
    if (!s_requested) return;

    if (const CoreApi* api = g_core) {
        if (g_coreCalls != 0 || *api->busy != 0 || !Unload()) {
            if (now - s_lastDeferLog >= 1000) { s_lastDeferLog = now; Log("loader: core busy; reload deferred"); }
            return;
        }
    }

    s_requested = false;
    s_pending = false;
    s_loggedMissing = false;
    if (!GetStamp(s_loadedStamp)) {
        Log("loader: %s not found", CorePath().c_str());
        s_loggedMissing = true;
        return;
    }
    if (!Load()) {
        // The game now runs without a core, so state stashed by the last one may go stale
        // (windows deleted, the camera moved). The next core starts clean.
        s_stash.clear();
        Log("loader: running without a core until %s changes", AppFileName("_core.dll").c_str());
    }
}

} // namespace loader
} // namespace host
