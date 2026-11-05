#include "core/app.h"
#include "core/config.h"
#include "core/log.h"
#include "render/d3d9.h"
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

        LogOpen(AppFile(".log"));
        Log("%s v%s loaded from %s", kAppName, kAppVersion, path);

        ConfigLoad();

        if (MH_Initialize() != MH_OK)
            Log("MH_Initialize failed");

        // Never LoadLibrary / create devices inside DllMain (loader lock).
        d3d9::InstallAsync();
    }
    else if (reason == DLL_PROCESS_DETACH) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        Log("%s unloaded", kAppName);
    }
    return TRUE;
}
