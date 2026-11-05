// ImGui overlay lifecycle: init on the first Present, window subclassing, the menu toggle and
// the hidden game cursor. The menu contents are in menu.cpp, its input in input_feed.cpp.
#include "overlay/overlay.h"
#include "core/app.h"
#include "core/log.h"
#include "features/mouse/mouse.h"
#include "overlay/input_feed.h"
#include "overlay/menu.h"
#include "util/hook.h"
#include <d3d9.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace overlay {

namespace {

IDirect3DDevice9* s_dev = nullptr;
HWND    s_hwnd = nullptr;
WNDPROC s_origWndProc = nullptr;
bool    s_unicode = false;
bool    s_init = false;
bool    s_visible = false;
bool    s_toggleSwallow = false;      // a newly bound toggle key is still held: don't toggle on it
std::string s_imguiIni;

// ---- game cursor --------------------------------------------------------------
// EQ uses a normal Win32 cursor (SetCursor). While the menu is open we force it to
// NULL so only ImGui's cursor is visible, and restore the game's choice on close.
using SetCursor_t = HCURSOR (WINAPI*)(HCURSOR);
SetCursor_t oSetCursor = nullptr;
HCURSOR s_gameCursor = nullptr;

HCURSOR WINAPI Hk_SetCursor(HCURSOR h)
{
    HCURSOR prev = s_gameCursor;
    s_gameCursor = h;
    if (s_visible) { oSetCursor(nullptr); return prev; }
    return oSetCursor(h);
}

void HookSetCursor()
{
    void* target = (void*)GetProcAddress(GetModuleHandleA("user32.dll"), "SetCursor");
    if (!target || hook::Install("SetCursor", target, (void*)&Hk_SetCursor, (void**)&oSetCursor) != hook::Result::Ok) {
        Log("overlay: the game cursor will stay visible with the menu");
        oSetCursor = nullptr;
    }
}

void HideGameCursor(bool hide)
{
    if (!oSetCursor) return;
    oSetCursor(hide ? nullptr : s_gameCursor);
}

LRESULT CALLBACK Hk_WndProc(HWND h, UINT msg, WPARAM w, LPARAM l)
{
    if (s_visible && s_init) {
        const bool mouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSELEAVE;
        const bool keyMsg   = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);

        if (mouseMsg)
            return 0;   // menu mouse comes from DirectInput; keep window mouse away from ImGui and the game
        if (msg == WM_SETCURSOR) {
            if (oSetCursor) oSetCursor(nullptr);
            return TRUE;   // don't let the game put its cursor back
        }
        if (keyMsg) {
            // Menu keyboard is polled (feed::Keyboard); don't feed ImGui twice.
            if (ImGui::GetIO().WantCaptureKeyboard) return 0;
        } else {
            ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
        }
    }
    return s_unicode ? CallWindowProcW(s_origWndProc, h, msg, w, l)
                     : CallWindowProcA(s_origWndProc, h, msg, w, l);
}

void Init(IDirect3DDevice9* dev)
{
    D3DDEVICE_CREATION_PARAMETERS cp = {};
    dev->GetCreationParameters(&cp);
    s_hwnd = cp.hFocusWindow;
    if (!s_hwnd) {
        IDirect3DSwapChain9* sc = nullptr;
        if (SUCCEEDED(dev->GetSwapChain(0, &sc))) {
            D3DPRESENT_PARAMETERS pp = {};
            sc->GetPresentParameters(&pp);
            s_hwnd = pp.hDeviceWindow;
            sc->Release();
        }
    }
    if (!s_hwnd) { Log("overlay: no window for device"); return; }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    s_imguiIni = AppFile("_imgui.ini");
    io.IniFilename = s_imguiIni.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplDX9_Init(dev);

    s_unicode = IsWindowUnicode(s_hwnd);
    s_origWndProc = (WNDPROC)(s_unicode
        ? SetWindowLongPtrW(s_hwnd, GWLP_WNDPROC, (LONG_PTR)Hk_WndProc)
        : SetWindowLongPtrA(s_hwnd, GWLP_WNDPROC, (LONG_PTR)Hk_WndProc));

    HookSetCursor();

    s_dev = dev;
    s_init = true;
    Log("overlay: init hwnd=%p dev=%p unicode=%d", s_hwnd, dev, s_unicode);
}

void PollToggle()
{
    static bool prev = false;
    const bool down = keybind::IsDown(cfg.menu_key);
    if (keybind::Capturing()) { prev = down; return; }                // the key press is going to the capture
    if (s_toggleSwallow) { prev = true; s_toggleSwallow = false; }    // freshly bound key is still held
    if (down && !prev && GetForegroundWindow() == s_hwnd)
        SetVisible(!s_visible);
    prev = down;
}

} // namespace

bool Visible() { return s_visible; }

void SetVisible(bool visible)
{
    if (!s_init || visible == s_visible) return;
    s_visible = visible;
    mouse::ResetAccumulators();
    ImGuiIO& io = ImGui::GetIO();
    if (visible) {
        feed::Reset();
    } else {
        // Release anything ImGui thinks is held so it doesn't stick.
        for (int i = 0; i < 3; ++i) io.AddMouseButtonEvent(i, false);
        io.ClearInputKeys();
    }
    HideGameCursor(visible);
    io.MouseDrawCursor = visible;
    Log("overlay: %s", visible ? "shown" : "hidden");
}

void OnPresent(IDirect3DDevice9* dev)
{
    if (s_init && dev != s_dev) {
        // Game recreated its device: rebind the DX9 backend.
        Log("overlay: device changed %p -> %p", s_dev, dev);
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplDX9_Init(dev);
        s_dev = dev;
    }
    if (!s_init) { Init(dev); if (!s_init) return; }

    PollToggle();
    if (s_visible) { if (keybind::PollCapture(s_hwnd)) s_toggleSwallow = true; }
    else keybind::CancelCapture();   // closing the menu cancels a capture

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    if (s_visible) { feed::Mouse(); if (!keybind::Capturing()) feed::Keyboard(s_hwnd); }
    ImGui::NewFrame();

    bool closeRequested = false;
    if (s_visible) {
        bool open = true;
        DrawMenu(&open);
        closeRequested = !open;      // the window's close button
    } else if (cfg.show_fps) {
        DrawFps();
    }
    DrawBanner();

    ImGui::EndFrame();
    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();
    if (dd->CmdListsCount > 0 && SUCCEEDED(dev->BeginScene())) {
        ImGui_ImplDX9_RenderDrawData(dd); // backend saves/restores device state itself
        dev->EndScene();
    }
    if (closeRequested) SetVisible(false);
}

void PreReset()  { if (s_init) ImGui_ImplDX9_InvalidateDeviceObjects(); }
void PostReset() { if (s_init) ImGui_ImplDX9_CreateDeviceObjects(); }

} // namespace overlay
