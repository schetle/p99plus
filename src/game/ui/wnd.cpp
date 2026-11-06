#include "game/ui/wnd.h"
#include "game/eqgame.h"
#include "game/ui/xstr.h"
#include "util/mem.h"

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
    const uintptr_t mgr = mem::Read<uintptr_t>(addr::kWndManager);
    if (mgr < 0x10000) return false;
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

} // namespace game::wnd
