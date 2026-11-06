// dinput8.dll entry point. Everything heavy waits: the d3d9 hooks go in from a thread, and the
// core is loaded on the first Present (never LoadLibrary inside DllMain: loader lock).
#include "core/app.h"
#include "core/log.h"
#include "host/host.h"
#include <MinHook.h>

HMODULE g_module = nullptr;
std::string g_gameDir;

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        g_module = hinst;

        char path[MAX_PATH] = {};
        GetModuleFileNameA(hinst, path, MAX_PATH);
        g_gameDir = path;
        g_gameDir.erase(g_gameDir.find_last_of("\\/") + 1);

        host::LogOpen(AppFile(".log"));
        Log("%s v%s host loaded from %s", kAppName, kAppVersion, path);

        if (MH_Initialize() != MH_OK)
            Log("MH_Initialize failed");

        host::d3d9::InstallAsync();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        // The process is exiting: the core is not called back (loader lock), its hooks die with it.
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("%s unloaded", kAppName);
    }
    return TRUE;
}
