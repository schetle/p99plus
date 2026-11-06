// Dragging a sizable window by its body. The game moves a window clicked in its client area only
// when it isn't sizable (a sizable one only moves by its title bar), and a bordered, sizable window
// of ours has no title bar. For windows with `dragByBody` a client-area click takes the
// not-sizable path; the border still resizes them, and every other window is left alone.
#include "core/log.h"
#include "game/eqgame.h"
#include "game/ui/detail.h"
#include "util/hook.h"
#include "util/mem.h"
#include <cstring>

// The detour at addr::kDragSizableTest. It runs in the game's mouse code with its registers live:
// eax = hit code (2 = the client area), edi = the window, and it must leave edx = the window's
// style as the replaced instruction did.
extern "C" {
uintptr_t p99_dragMove = 0;
uintptr_t p99_dragResize = 0;
bool __cdecl p99_DragByBody(uintptr_t wnd);
void p99_DragStub();
}
asm(R"(
    .text
    .p2align 4
    .globl _p99_DragStub
_p99_DragStub:
    movl  0x64(%edi), %edx
    cmpl  $2, %eax
    jne   1f
    pushal
    pushl %edi
    call  _p99_DragByBody
    addl  $4, %esp
    testb %al, %al
    popal
    jnz   2f
1:  testb $2, %dh
    jz    2f
    jmp   *_p99_dragResize
2:  jmp   *_p99_dragMove
)");

extern "C" bool __cdecl p99_DragByBody(uintptr_t wnd)
{
    hook::Busy busy;
    for (const game::wnd::CustomWindow* w : game::wnd::detail::Registry())
        if (w->Def().dragByBody && game::wnd::detail::Access::Handle(*w).ptr() == wnd) return true;
    return false;
}

namespace game::wnd::detail {

bool InstallBodyDrag()
{
    using namespace game::addr;
    void* at = (void*)kDragSizableTest;
    if (!mem::Matches(at, kDragSizableTestBytes, sizeof(kDragSizableTestBytes))) {
        Log("body drag: unexpected code at %p; not patched", at);
        return false;
    }
    p99_dragMove = kDragMove;
    p99_dragResize = kDragResize;
    unsigned char jmp[sizeof(kDragSizableTestBytes)];
    memset(jmp, 0x90, sizeof(jmp));
    jmp[0] = 0xe9;
    const int rel = (int)((uintptr_t)&p99_DragStub - (kDragSizableTest + 5));
    memcpy(jmp + 1, &rel, 4);
    if (!mem::PatchCode(at, jmp, sizeof(jmp))) {
        Log("body drag: patch failed at %p", at);
        return false;
    }
    Log("body drag: patched at %p", at);
    return true;
}

} // namespace game::wnd::detail
