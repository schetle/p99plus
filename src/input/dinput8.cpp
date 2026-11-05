// dinput8.dll proxy.
//
// eqgame.exe imports DirectInput8Create from DINPUT8.dll. We export the same
// function, forward to the real system dinput8.dll, then patch the returned
// objects' vtables so we can see/modify mouse + keyboard input:
//
//   IDirectInput8::CreateDevice      -> remember which device is mouse / keyboard
//   IDirectInputDevice8::GetDeviceState / GetDeviceData
//                                    -> hand mouse movement to input::RouteAxis, hide the orbit
//                                       button, and block everything while the menu is open
//
// Vtable patching (rather than MinHook) is used here because Wine may use one
// vtable for several device types; we look up the original per-vtable.

#include "core/app.h"
#include "core/log.h"
#include "input/input.h"
#include "overlay/overlay.h"
#include "util/mem.h"
#include <dinput.h>

namespace {

using DI8Create_t      = HRESULT (WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
using CreateDevice_t   = HRESULT (WINAPI*)(void* self, REFGUID, void** dev, LPUNKNOWN);
using GetDeviceState_t = HRESULT (WINAPI*)(void* self, DWORD cb, LPVOID data);
using GetDeviceData_t  = HRESULT (WINAPI*)(void* self, DWORD cbObj, LPDIDEVICEOBJECTDATA, LPDWORD inOut, DWORD flags);

// IDirectInput8 vtable index
constexpr int kCreateDevice   = 3;
// IDirectInputDevice8 vtable indices
constexpr int kGetDeviceState = 9;
constexpr int kGetDeviceData  = 10;

DI8Create_t s_realCreate = nullptr;
void* s_mouse    = nullptr;
void* s_keyboard = nullptr;

// ---- tiny per-vtable original table ----------------------------------------
struct Patch { void** vtbl; int idx; void* orig; };
Patch s_patches[16];
int   s_numPatches = 0;

void* FindOrig(void* self, int idx)
{
    void** vtbl = *(void***)self;
    for (int i = 0; i < s_numPatches; ++i)
        if (s_patches[i].vtbl == vtbl && s_patches[i].idx == idx)
            return s_patches[i].orig;
    return nullptr;
}

void PatchVtbl(void* obj, int idx, void* hook)
{
    void** vtbl = *(void***)obj;
    if (vtbl[idx] == hook) return;              // already patched (shared vtable)
    if (s_numPatches >= (int)(sizeof(s_patches) / sizeof(s_patches[0]))) return;

    void* orig = vtbl[idx];
    if (!mem::Patch(&vtbl[idx], &hook, sizeof(hook))) {
        Log("patching vtbl %p[%d] failed", vtbl, idx);
        return;
    }
    s_patches[s_numPatches++] = { vtbl, idx, orig };
    Log("patched vtbl %p[%d]", vtbl, idx);
}

// ---- device hooks -------------------------------------------------------------
HRESULT WINAPI Hk_GetDeviceState(void* self, DWORD cb, LPVOID data)
{
    auto orig = (GetDeviceState_t)FindOrig(self, kGetDeviceState);
    HRESULT hr = orig(self, cb, data);
    if (FAILED(hr) || !data) return hr;

    if (self == s_mouse && cb >= sizeof(DIMOUSESTATE)) {
        auto* ms = (DIMOUSESTATE*)data;
        const bool wide = cb >= sizeof(DIMOUSESTATE2);
        BYTE* buttons = wide ? ((DIMOUSESTATE2*)data)->rgbButtons : ms->rgbButtons;
        const int nbtn = wide ? 8 : 4;
        for (int i = 0; i < nbtn; ++i) input::SetButton(i, (buttons[i] & 0x80) != 0);

        const bool menu = overlay::Visible();
        long x = ms->lX, y = ms->lY, z = ms->lZ;
        ms->lX = input::RouteAxis(input::Axis::X, x) ? 0 : x;
        ms->lY = input::RouteAxis(input::Axis::Y, y) ? 0 : y;
        ms->lZ = input::RouteAxis(input::Axis::Z, z) ? 0 : z;
        if (menu) { memset(data, 0, cb); return hr; }   // the menu hides buttons too
        for (int i = 0; i < nbtn; ++i)
            if (input::HidesButton(i)) buttons[i] = 0;
    }
    else if (self == s_keyboard && overlay::Visible()) {
        memset(data, 0, cb);
    }
    return hr;
}

HRESULT WINAPI Hk_GetDeviceData(void* self, DWORD cbObj, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inOut, DWORD flags)
{
    auto orig = (GetDeviceData_t)FindOrig(self, kGetDeviceData);
    HRESULT hr = orig(self, cbObj, rgdod, inOut, flags);
    if (FAILED(hr) || !inOut || !rgdod || (flags & DIGDD_PEEK)) return hr;
    if (self != s_mouse && self != s_keyboard) return hr;

    const bool menu = overlay::Visible();

    if (self == s_mouse) {
        DWORD kept = 0;
        for (DWORD i = 0; i < *inOut; ++i) {
            auto* e = (DIDEVICEOBJECTDATA*)((BYTE*)rgdod + i * cbObj);
            bool drop = false;
            if (e->dwOfs >= DIMOFS_BUTTON0 && e->dwOfs <= DIMOFS_BUTTON7) {
                const int button = (int)(e->dwOfs - DIMOFS_BUTTON0);
                input::SetButton(button, (e->dwData & 0x80) != 0);
                drop = input::HidesButton(button);
            } else if (e->dwOfs == DIMOFS_X || e->dwOfs == DIMOFS_Y || e->dwOfs == DIMOFS_Z) {
                const input::Axis axis = e->dwOfs == DIMOFS_X ? input::Axis::X
                                       : e->dwOfs == DIMOFS_Y ? input::Axis::Y : input::Axis::Z;
                long v = (long)e->dwData;
                drop = input::RouteAxis(axis, v);
                e->dwData = (DWORD)v;
            }
            if (drop) continue;
            if (kept != i) memmove((BYTE*)rgdod + kept * cbObj, e, cbObj);
            ++kept;
        }
        *inOut = kept;
    }

    if (menu) *inOut = 0;   // events were consumed from the buffer; the game sees none
    return hr;
}

// ---- factory hook -------------------------------------------------------------
HRESULT WINAPI Hk_CreateDevice(void* self, REFGUID guid, void** dev, LPUNKNOWN outer)
{
    auto orig = (CreateDevice_t)FindOrig(self, kCreateDevice);
    HRESULT hr = orig(self, guid, dev, outer);
    if (FAILED(hr) || !dev || !*dev) return hr;

    if (IsEqualGUID(guid, GUID_SysMouse)) {
        s_mouse = *dev;
        Log("CreateDevice: mouse %p", s_mouse);
    } else if (IsEqualGUID(guid, GUID_SysKeyboard)) {
        s_keyboard = *dev;
        Log("CreateDevice: keyboard %p", s_keyboard);
    } else {
        return hr; // joysticks etc. untouched
    }
    PatchVtbl(*dev, kGetDeviceState, (void*)&Hk_GetDeviceState);
    PatchVtbl(*dev, kGetDeviceData,  (void*)&Hk_GetDeviceData);
    return hr;
}

bool LoadReal()
{
    if (s_realCreate) return true;
    char sys[MAX_PATH];
    GetSystemDirectoryA(sys, MAX_PATH); // 32-bit process -> redirected to SysWOW64
    std::string path = std::string(sys) + "\\dinput8.dll";
    HMODULE real = LoadLibraryA(path.c_str());
    if (!real || real == g_module) {
        Log("failed to load real dinput8 from %s", path.c_str());
        return false;
    }
    s_realCreate = (DI8Create_t)GetProcAddress(real, "DirectInput8Create");
    Log("real dinput8: %s -> %p", path.c_str(), (void*)s_realCreate);
    return s_realCreate != nullptr;
}

} // namespace

extern "C" __declspec(dllexport)
HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD ver, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    if (!LoadReal()) return E_FAIL;
    HRESULT hr = s_realCreate(hinst, ver, riid, out, outer);
    Log("DirectInput8Create ver=0x%X hr=0x%08lX", ver, hr);
    if (SUCCEEDED(hr) && out && *out)
        PatchVtbl(*out, kCreateDevice, (void*)&Hk_CreateDevice); // same slot for A and W
    return hr;
}
