// The game window's subclass. Installed once and never removed, so a core can be swapped without
// touching the window: messages go to the loaded core first (the overlay), then to the game.
#include "core/log.h"
#include "host/host.h"

namespace host::window {

namespace {

HWND    s_hwnd = nullptr;
WNDPROC s_origWndProc = nullptr;
bool    s_unicode = false;
bool    s_tried = false;

LRESULT CALLBACK Hk_WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    {
        CoreCall core;
        LRESULT result = 0;
        if (core.api && core.api->wndProc(h, msg, w, l, &result)) return result;
    }
    return s_unicode ? CallWindowProcW(s_origWndProc, h, msg, w, l)
                     : CallWindowProcA(s_origWndProc, h, msg, w, l);
}

HWND FindWindowOf(IDirect3DDevice9* dev)
{
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    dev->GetCreationParameters(&cp);
    if (cp.hFocusWindow) return cp.hFocusWindow;

    HWND hwnd = nullptr;
    IDirect3DSwapChain9* sc = nullptr;
    if (SUCCEEDED(dev->GetSwapChain(0, &sc))) {
        D3DPRESENT_PARAMETERS pp = {};
        sc->GetPresentParameters(&pp);
        hwnd = pp.hDeviceWindow;
        sc->Release();
    }
    return hwnd;
}

} // namespace

void Attach(IDirect3DDevice9* dev)
{
    if (s_tried) return;
    s_tried = true;

    HWND hwnd = FindWindowOf(dev);
    if (!hwnd) { Log("window: no window for device %p; overlay disabled", dev); return; }

    s_unicode = IsWindowUnicode(hwnd);
    s_origWndProc = (WNDPROC)(s_unicode
        ? SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)Hk_WndProc)
        : SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)Hk_WndProc));
    s_hwnd = hwnd;
    Log("window: hwnd=%p unicode=%d", hwnd, s_unicode);
}

HWND Hwnd() { return s_hwnd; }

} // namespace host::window
