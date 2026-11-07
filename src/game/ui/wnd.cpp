#include "game/ui/wnd.h"
#include "game/eqgame.h"
#include "game/ui/xstr.h"
#include "util/mem.h"
#include <algorithm>

namespace game::wnd {

namespace {
using New_t      = void* (__cdecl*)(size_t size);
using SidlCtor_t = void* (__thiscall*)(void* self, void* parent, const void* screen, unsigned flags, int mode, const char* ini);
using Void_t     = void  (__thiscall*)(void* self);
using GetChild_t = uintptr_t (__thiscall*)(void* self, const void* screenId);
using SetText_t  = void  (__thiscall*)(void* self, const void* text);
using ShowWnd_t  = void  (__thiscall*)(void* self, bool show, bool bringToTop);
using Activate_t = int   (__thiscall*)(void* self);
using SetCheck_t = void  (__thiscall*)(void* self, bool checked);
using Delete_t   = void  (__thiscall*)(void* self, int free);
using AddStr_t   = int   (__thiscall*)(void* self, const void* text, unsigned color, int data, void* anim);
using GetSel_t   = int   (__thiscall*)(void* self);
using SetSel_t   = void  (__thiscall*)(void* self, int row);
using SetStml_t  = void  (__thiscall*)(void* self, uintptr_t text, int a, int b);
using EditInsert_t = void (__thiscall*)(void* self, uintptr_t text, int a);
using SetFocus_t = uintptr_t (__thiscall*)(void* mgr, uintptr_t wnd);
using ScreenRect_t = int* (__thiscall*)(void* self, int* rect);
using Move_t     = void  (__thiscall*)(void* self, const int* rect);
using WindowAt_t = uintptr_t (__thiscall*)(void* mgr, const void* point, int* hitCode);

// The UI manager, or 0 while there is none.
uintptr_t Manager()
{
    const uintptr_t mgr = mem::Read<uintptr_t>(addr::kWndManager);
    return mgr < 0x10000 ? 0 : mgr;
}
} // namespace

template <class Fn> Fn Wnd::Virtual(int offset) const
{
    return (Fn)mem::Read<uintptr_t>(mem::Read<uintptr_t>(p_) + offset);
}

Wnd Wnd::CreateSidl(const char* screen)
{
    void* memory = ((New_t)addr::kNew)(kSidlSize);
    if (!memory) return {};
    const XStr name(screen);   // alive until we're done, as the game's own callers keep theirs
    ((SidlCtor_t)addr::kSidlCtor)(memory, nullptr, name.get(), kIniFlags, kIniMode, nullptr);
    const Wnd w((uintptr_t)memory);
    const uintptr_t tmpl = mem::Read<uintptr_t>(w.p_ + kTemplate);
    if (!tmpl) {
        w.Delete();
        return {};
    }
    mem::Write<unsigned>(w.p_ + kStyle, mem::Read<unsigned>(w.p_ + kStyle) | mem::Read<unsigned>(tmpl + kTemplateStyle));
    ((Void_t)addr::kCreateChildren)(memory);
    return w;
}

bool Wnd::Alive() const
{
    const uintptr_t mgr = Manager();
    if (!mgr) return false;
    const int n = mem::Read<int>(mgr + kMgrCount);
    const uintptr_t* items = mem::Read<const uintptr_t*>(mgr + kMgrItems);
    if (!items || n <= 0) return false;
    for (int i = 0; i < n; ++i)
        if (items[i] == p_) return true;
    return false;
}

Wnd Wnd::Child(const char* screenId) const
{
    if (!p_) return {};
    const XStr id(screenId);
    return Wnd(((GetChild_t)addr::kGetChildItem)((void*)p_, id.get()));
}

void Wnd::SetText(const char* text) const
{
    if (!p_) return;
    const XStr s(text);
    Virtual<SetText_t>(kSetText)((void*)p_, s.get());
}

void Wnd::Show(bool show) const
{
    if (p_) Virtual<ShowWnd_t>(kShow)((void*)p_, show, false);
}

void Wnd::Activate() const
{
    if (p_) Virtual<Activate_t>(kActivate)((void*)p_);
}

bool Wnd::Visible() const { return p_ && mem::Read<unsigned char>(p_ + kVisible) != 0; }

bool Wnd::Checked() const { return p_ && mem::Read<unsigned char>(p_ + kChecked) != 0; }

void Wnd::SetChecked(bool checked) const
{
    if (p_) ((SetCheck_t)addr::kButtonSetCheck)((void*)p_, checked);
}

void Wnd::SetCloseOnEsc(bool on) const
{
    if (p_) mem::Write<unsigned char>(p_ + kCloseOnEsc, on ? 1 : 0);
}

void Wnd::StoreIni() const
{
    if (p_) Virtual<Void_t>(kStoreIni)((void*)p_);
}

void Wnd::Delete() const
{
    if (p_) Virtual<Delete_t>(kDelete)((void*)p_, 1);
}

std::string Wnd::Text(int maxLength) const
{
    return p_ ? XStrToString(p_ + kWindowText, maxLength) : std::string();
}

void Wnd::Rect(int& left, int& top, int& right, int& bottom) const
{
    left = top = right = bottom = 0;
    if (!p_) return;
    left = mem::Read<int>(p_ + kRect);
    top = mem::Read<int>(p_ + kRect + 4);
    right = mem::Read<int>(p_ + kRect + 8);
    bottom = mem::Read<int>(p_ + kRect + 12);
}

void Wnd::Move(int left, int top, int right, int bottom) const
{
    const int r[4] = { left, top, right, bottom };
    if (p_) ((Move_t)addr::kMoveWnd)((void*)p_, r);
}

void Wnd::ScreenSize(int& width, int& height)
{
    width = mem::Read<int>(addr::kScreenWidth);
    height = mem::Read<int>(addr::kScreenHeight);
}

void Wnd::SetMaxChars(int n) const
{
    if (p_) mem::Write<int>(p_ + kEditMaxChars, n);
}

void Wnd::AddStyle(unsigned bits) const
{
    if (p_) mem::Write<unsigned>(p_ + kStyle, mem::Read<unsigned>(p_ + kStyle) | bits);
}

bool Wnd::HasFocus() const
{
    const uintptr_t mgr = Manager();
    return p_ && mgr && mem::Read<uintptr_t>(mgr + kMgrFocus) == p_;
}

void Wnd::TakeFocus() const
{
    if (p_ && Manager() && !HasFocus()) ((SetFocus_t)addr::kSetFocusWnd)((void*)Manager(), p_);
}

void Wnd::DropFocus() const
{
    if (HasFocus()) ((SetFocus_t)addr::kSetFocusWnd)((void*)Manager(), 0);
}

bool Wnd::ScreenRect(int& left, int& top, int& right, int& bottom) const
{
    if (!p_) return false;
    int r[4] = {};
    ((ScreenRect_t)addr::kScreenRect)((void*)p_, r);
    left = r[0], top = r[1], right = r[2], bottom = r[3];
    return right > left && bottom > top;
}

Wnd Wnd::UnderCursor()
{
    const uintptr_t mgr = Manager();
    if (!mgr) return Wnd();
    int hit = 0;
    return Wnd(((WindowAt_t)addr::kWindowAt)((void*)mgr, (const void*)(mgr + kMgrMouse), &hit));
}

bool Wnd::CursorPos(int& x, int& y)
{
    const uintptr_t mgr = Manager();
    if (!mgr) return false;
    x = mem::Read<int>(mgr + kMgrMouse);
    y = mem::Read<int>(mgr + kMgrMouse + 4);
    return true;
}

void Wnd::EditSelection(int& start, int& end) const
{
    start = end = 0;
    if (!p_) return;
    const int a = mem::Read<int>(p_ + kEditSelStart), b = mem::Read<int>(p_ + kEditSelEnd);
    start = std::max(0, std::min(a, b));
    end = std::max(0, std::max(a, b));
}

void Wnd::EditSelect(int start, int end) const
{
    if (!p_) return;
    mem::Write<int>(p_ + kEditSelStart, start);
    mem::Write<int>(p_ + kEditSelEnd, end);
}

void Wnd::EditInsert(const char* text) const
{
    if (!p_) return;
    XStr s(text);   // the edit takes the string by value and frees it
    ((EditInsert_t)addr::kEditInsert)((void*)p_, s.Release(), 0);
}

void Wnd::ListClear() const
{
    if (p_) Virtual<Void_t>(kListDeleteAll)((void*)p_);
}

int Wnd::ListAdd(const char* text, uint32_t argb) const
{
    if (!p_) return -1;
    const XStr s(text);   // AddString copies it; ours is freed here
    return ((AddStr_t)addr::kListAddString)((void*)p_, s.get(), argb, 0, nullptr);
}

int Wnd::ListSelection() const
{
    return p_ ? ((GetSel_t)addr::kListGetCurSel)((void*)p_) : -1;
}

void Wnd::ListSelect(int row) const
{
    if (p_) ((SetSel_t)addr::kListSetCurSel)((void*)p_, row);
}

void Wnd::SetStml(const char* stml) const
{
    if (!p_) return;
    XStr s(stml);   // SetSTMLText takes the string by value and frees it
    ((SetStml_t)addr::kStmlSetText)((void*)p_, s.Release(), 1, 0);
}

} // namespace game::wnd
