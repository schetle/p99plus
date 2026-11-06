#pragma once
// The interface between the two DLLs.
//
//   dinput8.dll (src/host/)       loaded by the game, never unloaded. Owns everything the game
//                                 holds pointers to: the DirectInput proxy and vtable patches, the
//                                 d3d9 hooks and the window subclass. Loads the core and swaps it
//                                 for a new build when the file changes.
//   <app>_core.dll (rest of src/) every feature, the game hooks, ImGui and the menu. Can be
//                                 unloaded at any frame boundary.
//
// The host calls the core only through CoreApi and the core calls the host only through HostApi.
// Bump kCoreApiVersion whenever either struct changes: the host refuses a core built against a
// different version.
#include <windows.h>
#include <d3d9.h>
#include <dinput.h>
#include <cstddef>

inline constexpr int kCoreApiVersion = 1;

struct HostApi {
    int         version;
    const char* gameDir;                          // directory of eqgame.exe, with trailing '\'
    HWND        hwnd;                             // the game window
    int         loadCount;                        // 1 for the first core of the session, 2 after one reload, ...
    void (*log)(const char* line);                // one line, without newline; timestamped by the host
    void (*requestReload)();                      // reload the core at the next frame
    // Values that survive a reload, keyed by name. stash() copies `data`; unstash() returns the
    // host's copy (valid until the next stash of that key) or nullptr if there is none.
    void (*stash)(const char* key, const void* data, size_t size);
    const void* (*unstash)(const char* key, size_t* size);
};

struct CoreApi {
    int version;
    volatile LONG* busy;                          // > 0 while a hook detour inside the core is running
    void (*shutdown)();                           // undo every hook and patch; the module is freed next

    // Direct3D (the host's device hooks)
    void (*onPresent)(IDirect3DDevice9* dev);     // per-frame work, overlay and frame cap; before the real Present
    void (*preReset)();
    void (*postReset)();
    const D3DMATRIX* (*onSetTransform)(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m);
    const D3DMATRIX* (*viewForBeginScene)();      // after the real BeginScene: a view to set again, or nullptr
    void (*onSetVertexShader)(IDirect3DVertexShader9* vs);
    const float* (*onSetVSConstants)(UINT reg, const float* data, UINT count);

    // DirectInput: called after a successful real call on the game's mouse or keyboard
    void (*filterState)(bool mouse, DWORD cb, void* data);
    void (*filterData)(bool mouse, DWORD cbObj, DIDEVICEOBJECTDATA* data, DWORD* inOut);

    // Window messages: true = handled, the window procedure returns *result
    bool (*wndProc)(HWND hwnd, UINT msg, WPARAM w, LPARAM l, LRESULT* result);
};

// The core's only export: extern "C" const CoreApi* CoreInit(const HostApi*).
using CoreInit_t = const CoreApi* (*)(const HostApi* host);
inline constexpr const char* kCoreInitName = "CoreInit";
