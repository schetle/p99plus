// ImGui overlay lifecycle: init on the first Present, window subclassing, the menu toggle and
// the hidden game cursor. The menu contents are in menu.cpp, its input in input_feed.cpp.
#include "overlay/overlay.h"
#include "core/app.h"
#include "core/host.h"
#include "core/log.h"
#include "features/mouse/mouse.h"
#include "overlay/input_feed.h"
#include "overlay/menu.h"
#include "util/hook.h"
#include <d3d9.h>
#include <imgui.h>
#include <imgui_impl_dx9.h>
#include <imgui_impl_win32.h>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace overlay {

namespace {

struct Ring { ImVec2 a, b; ImU32 color; };
std::vector<Ring> s_rings;   // Highlight() since the last draw
constexpr size_t kMaxRings = 16;

IDirect3DDevice9* s_dev = nullptr;
HWND    s_hwnd = nullptr;
bool    s_init = false;
bool    s_initTried = false;
bool    s_visible = false;
std::string s_imguiIni;
constexpr const char* kVisibleKey = "overlay.visible";   // stash: the menu was open at a reload

// ---- game cursor --------------------------------------------------------------
// EQ uses a normal Win32 cursor (SetCursor). While the menu is open we force it to
// NULL so only ImGui's cursor is visible, and restore the game's choice on close.
using SetCursor_t = HCURSOR (WINAPI*)(HCURSOR);
SetCursor_t oSetCursor = nullptr;
HCURSOR s_gameCursor = nullptr;

HCURSOR WINAPI Hk_SetCursor(HCURSOR h)
{
    hook::Busy busy;
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

void Init(IDirect3DDevice9* dev)
{
    if (s_initTried) return;
    s_initTried = true;
    s_hwnd = g_host->hwnd;   // the host found and subclassed it (host/window.cpp)
    if (!s_hwnd) { Log("overlay: no game window; menu unavailable"); return; }

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    s_imguiIni = AppFile("_imgui.ini");
    io.IniFilename = s_imguiIni.c_str();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(s_hwnd);
    ImGui_ImplDX9_Init(dev);

    HookSetCursor();

    s_dev = dev;
    s_init = true;
    Log("overlay: init hwnd=%p dev=%p", s_hwnd, dev);

    // After a reload: reopen the menu if it was open.
    bool wasVisible = false;
    if (stash::GetValue(kVisibleKey, wasVisible) && wasVisible) SetVisible(true);
    stash::Clear(kVisibleKey);
}

// Highlight()'s rings, then forgotten: a feature asks again each frame.
void DrawRings()
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    for (const Ring& r : s_rings) {
        dl->AddRect(ImVec2(r.a.x - 1, r.a.y - 1), ImVec2(r.b.x + 1, r.b.y + 1), r.color & 0x60ffffff, 3.0f, 0, 4.0f);   // soft glow
        dl->AddRect(r.a, r.b, r.color, 2.0f, 0, 1.5f);
    }
    s_rings.clear();
}

void PollToggle()
{
    static keybind::Press press;
    if (press.Poll(cfg.menu_key) && GetForegroundWindow() == s_hwnd) SetVisible(!s_visible);
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
    if (s_visible) keybind::PollCapture(s_hwnd);
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
    DrawRings();

    ImGui::EndFrame();
    ImGui::Render();
    ImDrawData* dd = ImGui::GetDrawData();
    if (dd->CmdListsCount > 0 && SUCCEEDED(dev->BeginScene())) {
        ImGui_ImplDX9_RenderDrawData(dd); // backend saves/restores device state itself
        dev->EndScene();
    }
    if (closeRequested) SetVisible(false);
}

void Highlight(float left, float top, float right, float bottom, unsigned int rgba)
{
    if (s_rings.size() < kMaxRings) s_rings.push_back({ ImVec2(left, top), ImVec2(right, bottom), (ImU32)rgba });
}

void PreReset()  { if (s_init) ImGui_ImplDX9_InvalidateDeviceObjects(); }
void PostReset() { if (s_init) ImGui_ImplDX9_CreateDeviceObjects(); }

bool WndProc(HWND h, UINT msg, WPARAM w, LPARAM l, LRESULT* result)
{
    if (!s_visible || !s_init) return false;

    const bool mouseMsg = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_MOUSELEAVE;
    const bool keyMsg   = (msg >= WM_KEYFIRST && msg <= WM_KEYLAST);

    if (mouseMsg) {
        *result = 0;   // menu mouse comes from DirectInput; keep window mouse away from ImGui and the game
        return true;
    }
    if (msg == WM_SETCURSOR) {
        if (oSetCursor) oSetCursor(nullptr);
        *result = TRUE;   // don't let the game put its cursor back
        return true;
    }
    if (keyMsg) {
        // Menu keyboard is polled (feed::Keyboard); don't feed ImGui twice.
        if (ImGui::GetIO().WantCaptureKeyboard) { *result = 0; return true; }
    } else {
        ImGui_ImplWin32_WndProcHandler(h, msg, w, l);
    }
    return false;
}

void Shutdown()
{
    stash::Clear(kVisibleKey);
    if (!s_init) return;
    const bool visible = s_visible;
    SetVisible(false);   // gives the game its cursor back while the SetCursor hook is still in
    keybind::CancelCapture();
    stash::PutValue(kVisibleKey, visible);

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    s_init = false;
}

} // namespace overlay
