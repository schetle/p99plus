#include "game/hud.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "game/ui/xstr.h"
#include "util/hook.h"
#include "util/mem.h"
#include <algorithm>
#include <cstring>
#include <vector>

// The detour at addr::kHudPoint: runs the replaced `mov ecx,[0x985c88]` after our drawing and
// goes back. Registers and flags are kept; the stack is 16-byte aligned for the call.
extern "C" {
uintptr_t p99_hudReturn = 0;
void __cdecl p99_HudDraw();
void p99_HudStub();
}
asm(R"(
    .text
    .p2align 4
    .globl _p99_HudStub
_p99_HudStub:
    pushfl
    pushal
    movl  %esp, %ebp
    andl  $-16, %esp
    call  _p99_HudDraw
    movl  %ebp, %esp
    popal
    popfl
    movl  0x985c88, %ecx
    jmp   *_p99_hudReturn
)");

namespace game::hud {

namespace {

using FontDraw_t   = int (__thiscall*)(void* font, const void* text, int x, int y, int width,
                                       const void* clip, uint32_t argb, int flags, int startX);
using FontWidth_t  = int (__thiscall*)(void* font, const void* text);
using FontHeight_t = int (__thiscall*)(void* font);

struct Rect { int left, top, right, bottom; };

std::vector<DrawFn> s_draw;
bool s_tried = false;
bool s_ok = false;
bool s_presented = false;    // a Present since we last drew: draw once per rendered frame

void* Font(int i)
{
    const uintptr_t mgr = mem::Read<uintptr_t>(addr::kWndManager);
    if (!mgr) return nullptr;
    const int n = mem::Read<int>(mgr + wnd::kMgrFontCount);
    const uintptr_t list = mem::Read<uintptr_t>(mgr + wnd::kMgrFonts);
    if (n <= 0 || n > 64 || !list) return nullptr;
    i = std::clamp(i, 0, n - 1);
    return (void*)mem::Read<uintptr_t>(list + i * 4);
}

} // namespace

} // namespace game::hud

extern "C" void __cdecl p99_HudDraw()
{
    hook::Busy busy;
    using namespace game::hud;
    if (!s_presented) return;
    s_presented = false;
    for (DrawFn fn : s_draw) fn();
}

namespace game::hud {

bool Init()
{
    if (s_tried) return s_ok;
    if (!game::Available()) return false;
    s_tried = true;
    using namespace game::addr;
    void* at = (void*)kHudPoint;
    if (!mem::Matches((void*)kFontDraw, kFontDrawPrologue, sizeof(kFontDrawPrologue)) ||
        !mem::Matches((void*)kFontWidth, kFontWidthPrologue, sizeof(kFontWidthPrologue)) ||
        !mem::Matches((void*)kFontHeight, kFontHeightPrologue, sizeof(kFontHeightPrologue))) {
        Log("hud: unexpected font code; game-font text unavailable");
        return false;
    }
    if (!mem::Matches(at, kHudPointBytes, sizeof(kHudPointBytes))) {
        Log("hud: unexpected code at %p; game-font text unavailable", at);
        return false;
    }
    p99_hudReturn = kHudPoint + sizeof(kHudPointBytes);
    unsigned char jmp[sizeof(kHudPointBytes)];
    memset(jmp, 0x90, sizeof(jmp));
    jmp[0] = 0xe9;
    const int rel = (int)((uintptr_t)&p99_HudStub - (kHudPoint + 5));
    memcpy(jmp + 1, &rel, 4);
    if (!mem::PatchCode(at, jmp, sizeof(jmp))) {
        Log("hud: patch failed at %p", at);
        return false;
    }
    Log("hud: patched at %p", at);
    s_ok = true;
    return true;
}

bool Ready() { return s_ok; }

void OnDraw(DrawFn fn)
{
    if (std::find(s_draw.begin(), s_draw.end(), fn) == s_draw.end()) s_draw.push_back(fn);
}

void Tick() { s_presented = true; }

int FontCount()
{
    if (!s_ok) return 0;
    const uintptr_t mgr = mem::Read<uintptr_t>(addr::kWndManager);
    if (!mgr) return 0;
    const int n = mem::Read<int>(mgr + wnd::kMgrFontCount);
    return (n > 0 && n <= 64 && mem::Read<uintptr_t>(mgr + wnd::kMgrFonts)) ? n : 0;
}

bool Measure(int font, const char* text, int& width, int& height)
{
    void* f = s_ok ? Font(font) : nullptr;
    if (!f) return false;
    game::XStr s(text);
    width = ((FontWidth_t)addr::kFontWidth)(f, s.get());
    height = ((FontHeight_t)addr::kFontHeight)(f);
    return true;
}

void Text(int x, int y, uint32_t argb, int font, const char* text)
{
    void* f = s_ok ? Font(font) : nullptr;
    if (!f) return;
    const int sw = mem::Read<int>(addr::kScreenRight), sh = mem::Read<int>(addr::kScreenBottom);
    const Rect clip = { 0, 0, std::max(sw, 1), std::max(sh, 1) };
    const int width = std::max(clip.right - x, 1);
    game::XStr s(text);
    auto draw = (FontDraw_t)addr::kFontDraw;
    draw(f, s.get(), x + 1, y + 1, width, &clip, argb & 0xff000000u, 1, 0);   // shadow
    draw(f, s.get(), x, y, width, &clip, argb, 1, 0);
}

} // namespace game::hud
