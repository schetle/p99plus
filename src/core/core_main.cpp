// The core DLL's entry: CoreInit is called by the host right after it loads this DLL, and
// CoreApi::shutdown right before it frees it (see shared/core_api.h).
//
// Init does only cheap setup. Hooks go in from the features' per-frame work, as before.
// Shutdown has to leave the game as if the core had never been there: every feature's
// onShutdown runs (state worth keeping goes into the stash), byte patches are restored and
// MinHook removes every hook this DLL installed.
#include "core/app.h"
#include "core/config.h"
#include "core/feature.h"
#include "core/host.h"
#include "core/log.h"
#include "input/input.h"
#include "overlay/overlay.h"
#include "render/dispatch.h"
#include "util/hook.h"
#include "util/mem.h"
#include <MinHook.h>

HMODULE g_module = nullptr;
std::string g_gameDir;
const HostApi* g_host = nullptr;

namespace {

void Shutdown()
{
    Log("core: shutting down");
    feature::Shutdown();
    ConfigStash();
    mem::RestoreCode();
    MH_Uninitialize();   // disables and removes every hook this DLL made
    Log("core: unloaded");
}

void FilterState(bool mouse, DWORD cb, void* data) { input::FilterState(mouse, cb, data); }
void FilterData(bool mouse, DWORD cbObj, DIDEVICEOBJECTDATA* d, DWORD* inOut) { input::FilterData(mouse, cbObj, d, inOut); }

const CoreApi kApi = {
    kCoreApiVersion,
    &hook::g_busy,
    Shutdown,
    render::OnPresent,
    render::PreReset,
    render::PostReset,
    render::OnSetTransform,
    render::ViewForBeginScene,
    render::OnSetVertexShader,
    render::OnSetVSConstants,
    FilterState,
    FilterData,
    overlay::WndProc,
};

} // namespace

extern "C" __declspec(dllexport) const CoreApi* CoreInit(const HostApi* host)
{
    if (!host || host->version != kCoreApiVersion) return nullptr;   // the host reports it
    g_host = host;
    g_gameDir = host->gameDir;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&CoreInit, &g_module);

    char path[MAX_PATH] = {};
    GetModuleFileNameA(g_module, path, MAX_PATH);
    Log("%s v%s core loaded (#%d) from %s", kAppName, kAppVersion, host->loadCount, path);

    if (MH_Initialize() != MH_OK)
        Log("core: MH_Initialize failed");

    ConfigLoad();
    ConfigUnstash();   // after a reload: the settings as they were, unsaved edits included
    return &kApi;
}
