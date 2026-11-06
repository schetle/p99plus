// Hooks IDirect3DDevice9::Present, ::Reset, ::SetTransform, ::BeginScene, ::SetVertexShader
// and ::SetVertexShaderConstantF, and hands them to the core.
//
// EQGraphicsDX9.dll loads d3d9.dll (DXVK under Proton). We wait for it to be
// loaded, create a throwaway device on a hidden window to read the device
// vtable, then MinHook the shared implementations.

#include "core/app.h"
#include "core/log.h"
#include "host/host.h"
#include "util/hook.h"
#include <MinHook.h>
#include <d3d9.h>

namespace {

using Present_t = HRESULT (WINAPI*)(IDirect3DDevice9*, const RECT*, const RECT*, HWND, const RGNDATA*);
using Reset_t   = HRESULT (WINAPI*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using SetTransform_t = HRESULT (WINAPI*)(IDirect3DDevice9*, D3DTRANSFORMSTATETYPE, const D3DMATRIX*);
using BeginScene_t   = HRESULT (WINAPI*)(IDirect3DDevice9*);
using SetVS_t        = HRESULT (WINAPI*)(IDirect3DDevice9*, IDirect3DVertexShader9*);
using SetVSConstF_t  = HRESULT (WINAPI*)(IDirect3DDevice9*, UINT, const float*, UINT);
using Create9_t = IDirect3D9* (WINAPI*)(UINT);

// IDirect3DDevice9 vtable indices
constexpr int kReset        = 16;
constexpr int kPresent      = 17;
constexpr int kBeginScene   = 41;
constexpr int kSetTransform = 44;
constexpr int kSetVS        = 92;
constexpr int kSetVSConstF  = 94;

Present_t      oPresent      = nullptr;
Reset_t        oReset        = nullptr;
SetTransform_t oSetTransform = nullptr;
BeginScene_t   oBeginScene   = nullptr;
SetVS_t        oSetVS        = nullptr;
SetVSConstF_t  oSetVSConstF  = nullptr;

// Each hook hands the call to the loaded core (render/dispatch.cpp in the core); with no core
// loaded the game's call goes through untouched.

HRESULT WINAPI Hk_Present(IDirect3DDevice9* dev, const RECT* src, const RECT* dst, HWND wnd, const RGNDATA* dirty)
{
    host::window::Attach(dev);
    host::loader::Tick();   // before any core code runs this frame: the only point a core is swapped
    {
        host::CoreCall core;
        if (core.api) core.api->onPresent(dev);
    }
    return oPresent(dev, src, dst, wnd, dirty);
}

HRESULT WINAPI Hk_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
    host::CoreCall core;
    if (core.api) core.api->preReset();
    HRESULT hr = oReset(dev, pp);
    if (core.api) core.api->postReset();
    return hr;
}

HRESULT WINAPI Hk_SetTransform(IDirect3DDevice9* dev, D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    host::CoreCall core;
    if (core.api) m = core.api->onSetTransform(state, m);
    return oSetTransform(dev, state, m);
}

HRESULT WINAPI Hk_BeginScene(IDirect3DDevice9* dev)
{
    HRESULT hr = oBeginScene(dev);
    host::CoreCall core;
    if (core.api && oSetTransform)
        if (const D3DMATRIX* v = core.api->viewForBeginScene())
            oSetTransform(dev, D3DTS_VIEW, v);
    return hr;
}

HRESULT WINAPI Hk_SetVS(IDirect3DDevice9* dev, IDirect3DVertexShader9* vs)
{
    host::CoreCall core;
    if (core.api) core.api->onSetVertexShader(vs);
    return oSetVS(dev, vs);
}

HRESULT WINAPI Hk_SetVSConstF(IDirect3DDevice9* dev, UINT reg, const float* data, UINT count)
{
    host::CoreCall core;
    if (core.api) data = core.api->onSetVSConstants(reg, data, count);
    return oSetVSConstF(dev, reg, data, count);
}

bool GetDeviceVtable(void**& vtbl)
{
    auto create9 = (Create9_t)GetProcAddress(GetModuleHandleA("d3d9.dll"), "Direct3DCreate9");
    if (!create9) { Log("Direct3DCreate9 not found"); return false; }

    const std::string className = AppFileName("_dummy");
    WNDCLASSEXA wc = { sizeof(wc) };
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = g_module;
    wc.lpszClassName = className.c_str();
    RegisterClassExA(&wc);
    HWND wnd = CreateWindowExA(0, wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64,
                               nullptr, nullptr, g_module, nullptr);

    bool ok = false;
    IDirect3D9* d3d = create9(D3D_SDK_VERSION);
    if (d3d) {
        D3DPRESENT_PARAMETERS pp = {};
        pp.Windowed         = TRUE;
        pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_UNKNOWN;
        pp.hDeviceWindow    = wnd;
        IDirect3DDevice9* dev = nullptr;
        HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, wnd,
                                       D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                                       &pp, &dev);
        if (SUCCEEDED(hr) && dev) {
            vtbl = *(void***)dev;
            ok = true;
            dev->Release();
        } else {
            Log("dummy CreateDevice failed hr=0x%08lX", hr);
        }
        d3d->Release();
    }
    DestroyWindow(wnd);
    UnregisterClassA(wc.lpszClassName, g_module);
    return ok;
}

DWORD WINAPI InstallThread(LPVOID)
{
    // Wait (up to ~2 min) for the game's graphics DLL to pull in d3d9.
    for (int i = 0; i < 1200 && !GetModuleHandleA("d3d9.dll"); ++i) Sleep(100);
    if (!GetModuleHandleA("d3d9.dll")) { Log("d3d9.dll never loaded; overlay disabled"); return 0; }

    void** vtbl = nullptr;
    if (!GetDeviceVtable(vtbl)) return 0;
    // Note: vtbl points into d3d9.dll's code/rdata, which stays valid after the dummy device dies.

    // Created first, enabled together below so the game never runs with half of them in place.
    if (hook::Install("d3d9 Present", vtbl[kPresent], (void*)&Hk_Present, (void**)&oPresent, {}, false) != hook::Result::Ok ||
        hook::Install("d3d9 Reset",   vtbl[kReset],   (void*)&Hk_Reset,   (void**)&oReset,   {}, false) != hook::Result::Ok)
        return 0;

    // Camera hooks are optional: the overlay still works if these fail.
    hook::Install("d3d9 SetTransform",            vtbl[kSetTransform], (void*)&Hk_SetTransform, (void**)&oSetTransform, {}, false);
    hook::Install("d3d9 BeginScene",              vtbl[kBeginScene],   (void*)&Hk_BeginScene,   (void**)&oBeginScene,   {}, false);
    hook::Install("d3d9 SetVertexShader",         vtbl[kSetVS],        (void*)&Hk_SetVS,        (void**)&oSetVS,        {}, false);
    hook::Install("d3d9 SetVertexShaderConstantF", vtbl[kSetVSConstF],  (void*)&Hk_SetVSConstF,  (void**)&oSetVSConstF,  {}, false);

    MH_EnableHook(MH_ALL_HOOKS);
    Log("d3d9 hooks enabled");
    return 0;
}

} // namespace

namespace host::d3d9 {

void InstallAsync()
{
    HANDLE h = CreateThread(nullptr, 0, InstallThread, nullptr, 0, nullptr);
    if (h) CloseHandle(h);
}

} // namespace host::d3d9
