#include "game/ui/custom_window.h"
#include "core/host.h"
#include "core/log.h"
#include "game/eqgame.h"
#include "game/ui/detail.h"
#include "game/ui/driver.h"
#include "util/hook.h"
#include "util/mem.h"
#include <algorithm>
#include <utility>
#include <cstring>

namespace game::wnd {

namespace detail {
std::vector<CustomWindow*>& Registry()
{
    static std::vector<CustomWindow*> all;
    return all;
}
} // namespace detail

using detail::Access;
using detail::Registry;

// ---------------------------------------------------------------------------------------------
// The feature-facing side

CustomWindow::CustomWindow(CustomWindowDef def) : def_(std::move(def))
{
    Registry().push_back(this);
}

Status CustomWindow::GetStatus() const
{
    if (!Ready()) return Status::Unavailable;
    if (detail::g_inject.failed) return Status::Rejected;
    if (failed_) return Status::NotLoaded;
    if (stale_) return Status::Stale;
    return wnd_ ? Status::Shown : Status::NotInWorld;
}

void CustomWindow::Attach(Wnd w)
{
    wnd_ = w;
    controls_.clear();
}

CustomWindow::Control* CustomWindow::Find(const char* id)
{
    if (!wnd_) return nullptr;
    for (Control& c : controls_)
        if (c.id == id) return &c;
    controls_.push_back({ id, wnd_.Child(id) });
    return &controls_.back();
}

void CustomWindow::SetText(const char* id, const std::string& text)
{
    Control* c = Find(id);
    if (!c || !c->wnd || c->text == text) return;
    c->wnd.SetText(text.c_str());
    c->text = text;
}

void CustomWindow::ShowChild(const char* id, bool show)
{
    Control* c = Find(id);
    if (!c || !c->wnd || c->shown == (int)show) return;
    c->wnd.Show(show);
    c->shown = show;
}

bool CustomWindow::BindCheck(const char* id, bool& value)
{
    Control* c = Find(id);
    if (!c || !c->wnd) return false;
    if (c->checked != (int)value) {   // new window, or the code changed the value
        c->wnd.SetChecked(value);
        c->checked = value;
        return false;
    }
    const bool pressed = c->wnd.Checked();
    if (pressed == value) return false;
    value = pressed;
    c->checked = pressed;
    return true;
}

// ---------------------------------------------------------------------------------------------
// Building and deleting the windows

namespace {

using Build_t = int  (__thiscall*)(void* self);
using Reset_t = void (__thiscall*)(void* self);
using Dtor_t  = void (__thiscall*)(void* self);

Build_t oBuild = nullptr;
Reset_t oReset = nullptr;
Dtor_t  oDtor = nullptr;

bool s_tried = false;
bool s_ok = false;

// The in-world windows exist (between kBuildGameUi and kResetGameUi), read from the game itself.
bool UiBuilt() { return mem::Read<uintptr_t>(addr::kInventoryWnd) != 0; }

// Our windows are game objects (the game's vtable and controls, nothing of ours), so they stay up
// through a core reload and the next core takes them over by screen name. Deleting and rebuilding
// them in the reload's frame left the new window invisible until it was rebuilt again.
struct Kept { uintptr_t wnd; char screen[detail::kScreenLen]; };
struct Stash {
    detail::InjectState inject;
    int                 numKept;
    Kept                kept[detail::kMaxWindows];
};
constexpr const char* kStashKey = "wnd.custom";
Kept s_kept[detail::kMaxWindows];
int  s_numKept = 0;   // from the previous core, not yet taken over (see Adopt)

// `store`: write the window's position and size to the character's UI ini first, as the game does
// for its own windows when it saves the UI, so the next one built is where this one was.
void Destroy(CustomWindow& cw, bool store)
{
    const Wnd w = Access::Handle(cw);
    if (!w) return;
    if (store) w.StoreIni();
    w.Delete();
    Access::Attach(cw, Wnd());
}

// Per-window choices the game keeps in the window itself.
void ApplyFlags(CustomWindow& cw)
{
    Access::Handle(cw).SetCloseOnEsc(cw.Def().closeOnEsc);
}

void Create(CustomWindow& cw)
{
    const Wnd w = Wnd::CreateSidl(cw.Def().screen.c_str());
    if (!w) {
        // The name isn't in the loaded skin (the XML wasn't added).
        Access::Failed(cw) = true;
        Log("custom window %s: its screen isn't loaded; reload the UI (/loadskin) to try again", cw.Def().screen.c_str());
        return;
    }
    Access::Attach(cw, w);
    ApplyFlags(cw);
    w.Activate();
    Log("custom window %s: created", cw.Def().screen.c_str());
}

// Every CSidlScreenWnd's base destructor: notices when the game deletes one of ours itself.
void __fastcall Hk_Dtor(void* self, void* /*edx*/)
{
    hook::Busy busy;
    for (CustomWindow* w : Registry())
        if (Access::Handle(*w) == Wnd((uintptr_t)self)) Access::Attach(*w, Wnd());
    oDtor(self);
}

// Takes over the previous core's windows. A window no definition claims any more is deleted.
void Adopt()
{
    for (int i = 0; i < s_numKept; ++i) {
        const Kept& k = s_kept[i];
        const Wnd w(k.wnd);
        if (!w.Alive()) continue;
        CustomWindow* owner = nullptr;
        for (CustomWindow* cw : Registry())
            if (!Access::Handle(*cw) && cw->Def().screen == k.screen) owner = cw;
        if (!owner) {
            w.StoreIni();
            w.Delete();
            continue;
        }
        Access::Attach(*owner, w);
        ApplyFlags(*owner);
        // Hidden by the game (Esc, before closeOnEsc existed): it is wanted, so show it again.
        if (Access::Wanted(*owner) && !w.Visible()) w.Activate();
        Log("custom window %s: kept from the previous core", k.screen);
    }
    s_numKept = 0;
}

int __fastcall Hk_Build(void* self, void* /*edx*/)
{
    hook::Busy busy;
    const int r = oBuild(self);   // the game's own windows first
    for (CustomWindow* w : Registry()) {
        Access::Attach(*w, Wnd());
        Access::Failed(*w) = false;
        if (Access::Wanted(*w) && !detail::g_inject.failed) Create(*w);
    }
    return r;
}

void __fastcall Hk_Reset(void* self, void* /*edx*/)
{
    hook::Busy busy;
    if (s_numKept) Adopt();   // so they are deleted with the rest below
    for (CustomWindow* w : Registry()) {
        Destroy(*w, false);   // before the game drops the window list ours is in (it saved the UI already)
        Access::Failed(*w) = false;
    }
    oReset(self);
}

// A screen name the stash can't carry, or a registry it has no room for, would lose the window on
// the next reload: say so now rather than truncate.
void DropUnstashable()
{
    auto& all = Registry();
    for (size_t i = 0; i < all.size();) {
        if (all[i]->Def().screen.size() >= (size_t)detail::kScreenLen || i >= (size_t)detail::kMaxWindows) {
            Log("custom window %s: too long a name or too many windows; not added", all[i]->Def().screen.c_str());
            all.erase(all.begin() + i);
        } else {
            ++i;
        }
    }
}

} // namespace

bool Ready() { return s_ok; }

bool Init()
{
    if (s_tried) return s_ok;
    if (!game::Available()) return false;   // Check() may not have run yet
    s_tried = true;

    using namespace game::addr;
    DropUnstashable();
    s_ok = detail::InstallInject() &&
           hook::Install("UI build", (void*)kBuildGameUi, (void*)&Hk_Build, (void**)&oBuild, kBuildGameUiPrologue) == hook::Result::Ok &&
           hook::Install("UI reset", (void*)kResetGameUi, (void*)&Hk_Reset, (void**)&oReset, kResetGameUiPrologue) == hook::Result::Ok &&
           hook::Install("window destroy", (void*)kSidlDtor, (void*)&Hk_Dtor, (void**)&oDtor, kSidlDtorPrologue) == hook::Result::Ok;
    if (!s_ok) {
        Log("custom windows: unavailable");
        return false;
    }
    detail::InstallBodyDrag();   // without it, sizable windows of ours move only by a title bar

    Stash st;
    if (stash::GetValue(kStashKey, st)) {
        detail::g_inject = st.inject;
        s_numKept = std::min(std::max(st.numKept, 0), detail::kMaxWindows);
        std::copy(st.kept, st.kept + s_numKept, s_kept);
        detail::CheckStale();
    }
    stash::Clear(kStashKey);
    return true;
}

void Shutdown()
{
    stash::Clear(kStashKey);
    if (!s_ok) return;
    // Our windows stay up for the next core; the screens stay loaded in the game's UI.
    Stash st = {};
    st.inject = detail::g_inject;
    for (const CustomWindow* w : Registry()) {
        const Wnd h = Access::Handle(*w);
        if (!h || st.numKept == detail::kMaxWindows) continue;
        Kept& k = st.kept[st.numKept++];
        k.wnd = h.ptr();
        strncpy(k.screen, w->Def().screen.c_str(), sizeof(k.screen) - 1);
    }
    stash::PutValue(kStashKey, st);
}

void Tick()
{
    if (!s_ok || !UiBuilt()) return;
    if (s_numKept) Adopt();
    for (CustomWindow* w : Registry()) {
        if (detail::g_inject.failed) continue;
        const bool exists = (bool)Access::Handle(*w);
        if (Access::Wanted(*w) && !exists && !Access::Failed(*w)) Create(*w);
        else if (!Access::Wanted(*w) && exists) Destroy(*w, true);
    }
}

} // namespace game::wnd
