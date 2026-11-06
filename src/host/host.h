#pragma once
// The host DLL (dinput8.dll): the parts that stay loaded for the whole session and hand the
// game's calls to whichever core is loaded (see shared/core_api.h).
#include "shared/core_api.h"
#include <string>

namespace host {

// ---- the loaded core (loader.cpp) ----------------------------------------------------------
extern const CoreApi* volatile g_core;   // nullptr while no core is loaded
extern volatile LONG g_coreCalls;        // calls into the core in progress, on any thread

// Wraps one call into the core: `api` is the core to call, or nullptr to pass through. While a
// CoreCall exists the loader won't unload that core.
struct CoreCall {
    const CoreApi* api;
    CoreCall()  { InterlockedIncrement(&g_coreCalls); api = g_core; }
    ~CoreCall() { InterlockedDecrement(&g_coreCalls); }
    CoreCall(const CoreCall&) = delete;
    CoreCall& operator=(const CoreCall&) = delete;
};

namespace loader {
// At the top of every Present, before any core code runs on this frame: loads the core the
// first time, and swaps in a new build when the file changed or a reload was requested.
void Tick();
} // namespace loader

// ---- the game window (window.cpp) ------------------------------------------------------------
namespace window {
void Attach(IDirect3DDevice9* dev);   // first Present: find the window and subclass it
HWND Hwnd();                          // nullptr until attached
} // namespace window

// ---- d3d9 hooks (d3d9.cpp) --------------------------------------------------------------------
namespace d3d9 {
// Spawns a thread that waits for d3d9.dll, then hooks the device.
void InstallAsync();
} // namespace d3d9

// ---- log (host_log.cpp) -----------------------------------------------------------------------
void LogOpen(const std::string& path);
void LogLine(const char* line);       // HostApi::log

} // namespace host
