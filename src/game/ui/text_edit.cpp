#include "game/ui/text_edit.h"
#include "core/host.h"
#include "game/eqgame.h"
#include "input/input.h"
#include "util/clipboard.h"
#include "util/hook.h"
#include "util/mem.h"
#include <windows.h>
#include <algorithm>

namespace game::wnd {

namespace {

constexpr int       kMinChars = 16000;       // the box's limit is at least this, and kSpareChars over its text
constexpr int       kSpareChars = 4000;
constexpr size_t    kMaxSteps = 200;         // undo (and redo) steps kept
constexpr ULONGLONG kTypingPauseMs = 1000;   // a pause this long ends a burst of typing (one undo step)
constexpr ULONGLONG kPollMs = 250;           // how often Update reads the box back

int Room(size_t chars) { return std::max(kMinChars, (int)chars + kSpareChars); }

} // namespace

void TextEdit::Load(const std::string& text, bool newDocument)
{
    if (newDocument) ClearHistory();
    window_.AddStyle(id_, kEditReturnNewline);   // Enter starts a new line
    Put(text);
    loaded_ = Box();
}

bool TextEdit::Sync()
{
    const Wnd box = Box();
    if (!box || box != loaded_) return false;
    const std::string now = box.Text(limit_);
    int cursor, selEnd;
    box.EditSelection(cursor, selEnd);
    if (now == text_) {
        lastCursor_ = cursor;
        return false;
    }
    const ULONGLONG t = GetTickCount64();
    if (newStep_ || t - lastChange_ >= kTypingPauseMs) {
        Push(undo_, { text_, lastCursor_ });
        redo_.clear();
        newStep_ = false;
    }
    lastChange_ = t;
    lastCursor_ = cursor;
    text_ = now;
    return true;
}

bool TextEdit::Update(bool keysAllowed)
{
    static const struct { KeyBinding key; Action action; } kShortcuts[kNumShortcuts] = {
        { { 'C', keybind::kCtrl }, kCopy },
        { { 'X', keybind::kCtrl }, kCut },
        { { 'V', keybind::kCtrl }, kPaste },
        { { 'A', keybind::kCtrl }, kSelectAll },
        { { 'Z', keybind::kCtrl }, kUndo },
        { { 'Y', keybind::kCtrl }, kRedo },
        { { 'Z', keybind::kCtrl | keybind::kShift }, kRedo },
    };
    const Wnd box = Box();
    if (!box || box != loaded_) return false;
    bool changed = false;
    const bool keys = keysAllowed && box.HasFocus() && GetForegroundWindow() == g_host->hwnd;
    for (int i = 0; i < kNumShortcuts; ++i)
        if (keys_[i].Poll(kShortcuts[i].key, keys)) changed = Run(kShortcuts[i].action) || changed;
    const ULONGLONG now = GetTickCount64();
    if (now >= nextPoll_) {
        nextPoll_ = now + kPollMs;
        changed = Sync() || changed;
    }
    return changed;
}

void TextEdit::Put(const std::string& text)
{
    limit_ = Room(text.size());
    window_.SetMaxChars(id_, limit_);
    window_.SetText(id_, text, true);
    text_ = text;
}

void TextEdit::Grow(size_t chars)
{
    if (Room(chars) <= limit_) return;
    limit_ = Room(chars);
    window_.SetMaxChars(id_, limit_);
}

void TextEdit::Push(std::vector<Step>& steps, Step step)
{
    steps.push_back(std::move(step));
    if (steps.size() > kMaxSteps) steps.erase(steps.begin());
}

bool TextEdit::Insert(const std::string& text, size_t shownChars)
{
    bool changed = Sync();   // typing before it is a step of its own
    newStep_ = true;
    Grow(shownChars + text.size());
    Box().EditInsert(text.c_str());
    changed = Sync() || changed;
    newStep_ = true;
    return changed;
}

bool TextEdit::Apply(std::vector<Step>& from, std::vector<Step>& to)
{
    const bool typed = Sync();   // typing not yet seen becomes a step of its own first
    if (from.empty()) return typed;
    int cursor, selEnd;
    Box().EditSelection(cursor, selEnd);
    Push(to, { text_, cursor });
    const Step step = std::move(from.back());
    from.pop_back();
    Put(step.text);
    const int at = std::min(step.cursor, (int)text_.size());
    Box().EditSelect(at, at);
    lastCursor_ = at;
    newStep_ = true;
    return true;
}

bool TextEdit::Run(Action a)
{
    const Wnd box = Box();
    const std::string shown = box.Text(limit_);   // the box's own characters: the selection counts these
    int start, end;
    box.EditSelection(start, end);
    start = std::min(start, (int)shown.size());
    end = std::min(end, (int)shown.size());
    const std::string selected = shown.substr(start, end - start);
    switch (a) {
    case kCopy:
        if (!selected.empty()) clipboard::SetText(g_host->hwnd, selected);
        return false;
    case kCut:
        return !selected.empty() && clipboard::SetText(g_host->hwnd, selected) && Insert("", shown.size());
    case kPaste: {
        std::string paste;
        return clipboard::GetText(g_host->hwnd, paste) && !paste.empty() && Insert(paste, shown.size());
    }
    case kSelectAll:
        box.EditSelect(0, (int)shown.size());
        return false;
    case kUndo:
        return Apply(undo_, redo_);
    case kRedo:
        return Apply(redo_, undo_);
    default:
        return false;
    }
}

void TextEdit::ClearHistory()
{
    undo_.clear();
    redo_.clear();
    newStep_ = true;
    lastCursor_ = 0;
}

// ---- the key guard ---------------------------------------------------------------------------

namespace {

using KeyboardMsg_t = int (__thiscall*)(void* mgr, int key, int down);
KeyboardMsg_t oKeyboardMsg = nullptr;
volatile uintptr_t s_guarded = 0;   // GuardKeys' control
constexpr int kDikEscape = 0x01;

int __fastcall Hk_KeyboardMsg(void* mgr, void* /*edx*/, int key, int down)
{
    hook::Busy busy;
    const uintptr_t guarded = s_guarded;
    const bool held = guarded && down && mem::Read<uintptr_t>((uintptr_t)mgr + kMgrFocus) == guarded;
    const int r = oKeyboardMsg(mgr, key, down);
    if (!held) return r;
    if (key == kDikEscape) Wnd(guarded).DropFocus();
    return 0;   // taken by the UI
}

} // namespace

void GuardKeys(Wnd control)
{
    static bool tried = false;
    if (control && !tried && game::Available()) {   // hooked the first time it is needed
        tried = true;
        hook::Install("key guard", (void*)addr::kKeyboardMsg, (void*)&Hk_KeyboardMsg, (void**)&oKeyboardMsg,
                      kKeyboardMsgPrologue);
    }
    s_guarded = oKeyboardMsg ? control.ptr() : 0;

    // The game only moves the focus for a click on a window; a click in the world leaves it.
    static bool wasDown[3] = {};
    for (int i = 0; i < 3; ++i) {
        const bool down = input::Button(i);
        if (down && !wasDown[i] && control && control.HasFocus() && !Wnd::UnderCursor()) control.DropFocus();
        wasDown[i] = down;
    }
}

} // namespace game::wnd
