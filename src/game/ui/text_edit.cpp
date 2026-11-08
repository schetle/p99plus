#include "game/ui/text_edit.h"
#include "core/host.h"
#include "game/eqgame.h"
#include "input/input.h"
#include "util/clipboard.h"
#include "util/hook.h"
#include "util/mem.h"
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <iterator>

namespace game::wnd {

namespace {

constexpr int       kMinChars = 16000;       // the box's limit is at least this, and kSpareChars over its text
constexpr int       kSpareChars = 4000;
constexpr size_t    kMaxSteps = 200;         // undo (and redo) steps kept
constexpr ULONGLONG kTypingPauseMs = 1000;   // a pause this long ends a burst of typing (one undo step)
constexpr ULONGLONG kPollMs = 250;           // how often Update reads the box back
constexpr ULONGLONG kRepeatDelayMs = 400;    // a held arrow repeats after this long ...
constexpr ULONGLONG kRepeatMs = 35;          // ... this often

constexpr int kNavKeys[] = { VK_LEFT, VK_RIGHT, VK_UP, VK_DOWN, VK_HOME, VK_END };

bool IsWordChar(char c) { return (unsigned char)c >= 0x80 || std::isalnum((unsigned char)c) || c == '_'; }

size_t LineStart(const std::string& s, size_t at)
{
    while (at > 0 && s[at - 1] != '\n') --at;
    return at;
}

size_t LineEnd(const std::string& s, size_t at)
{
    while (at < s.size() && s[at] != '\n') ++at;
    return at;
}

// Ctrl+Left: to the start of this word, or of the one before.
size_t WordLeft(const std::string& s, size_t at)
{
    while (at > 0 && !IsWordChar(s[at - 1])) --at;
    while (at > 0 && IsWordChar(s[at - 1])) --at;
    return at;
}

// Ctrl+Right: past this word and the gap after it.
size_t WordRight(const std::string& s, size_t at)
{
    while (at < s.size() && IsWordChar(s[at])) ++at;
    while (at < s.size() && !IsWordChar(s[at])) ++at;
    return at;
}

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
    Navigate(keys && !(keybind::HeldMods() & keybind::kAlt));
    const ULONGLONG now = GetTickCount64();
    if (now >= nextPoll_) {
        nextPoll_ = now + kPollMs;
        changed = Sync() || changed;
    }
    return changed;
}

void TextEdit::Navigate(bool keys)
{
    int held = 0;
    if (keys && !keybind::Capturing())
        for (int vk : kNavKeys)
            if (keybind::KeyDown(vk)) held = vk;   // two at once: the last in the list wins
    const ULONGLONG now = GetTickCount64();
    bool act = false;
    if (held != navKey_) {
        navKey_ = held;
        navNext_ = now + kRepeatDelayMs;
        act = held != 0;
    } else if (held && now >= navNext_) {
        navNext_ = now + kRepeatMs;
        act = true;
    }
    if (act) {
        const int mods = keybind::HeldMods();
        Move(held, (mods & keybind::kShift) != 0, (mods & keybind::kCtrl) != 0);
    }
}

void TextEdit::Move(int vk, bool shift, bool ctrl)
{
    const Wnd box = Box();
    const std::string shown = box.Text(limit_);   // positions count the box's own characters
    const int size = (int)shown.size();
    int start, end;
    box.EditSelection(start, end);
    start = std::min(start, size);
    end = std::min(end, size);
    // The box gives the selection without its direction: ours, unless the player changed it since
    // (a click, typing), then the cursor is at its end.
    if (std::min(anchor_, caret_) != start || std::max(anchor_, caret_) != end) {
        anchor_ = start;
        caret_ = end;
        goalColumn_ = -1;
    }
    const bool selection = start != end;
    size_t at = (size_t)caret_;
    if (vk != VK_UP && vk != VK_DOWN) goalColumn_ = -1;
    switch (vk) {
    case VK_LEFT:
        if (selection && !shift) at = (size_t)start;   // the selection goes; the cursor to its start
        else if (ctrl) at = WordLeft(shown, at);
        else if (at > 0) --at;
        break;
    case VK_RIGHT:
        if (selection && !shift) at = (size_t)end;
        else if (ctrl) at = WordRight(shown, at);
        else if (at < shown.size()) ++at;
        break;
    case VK_HOME:
        at = ctrl ? 0 : LineStart(shown, at);
        break;
    case VK_END:
        at = ctrl ? shown.size() : LineEnd(shown, at);
        break;
    case VK_UP:
    case VK_DOWN: {
        const size_t lineStart = LineStart(shown, at);
        if (goalColumn_ < 0) goalColumn_ = (int)(at - lineStart);
        size_t target;
        if (vk == VK_UP) {
            if (lineStart == 0) { at = 0; break; }   // the first line: to the start
            target = LineStart(shown, lineStart - 1);
        } else {
            const size_t lineEnd = LineEnd(shown, at);
            if (lineEnd >= shown.size()) { at = shown.size(); break; }   // the last line: to the end
            target = lineEnd + 1;
        }
        at = std::min(target + (size_t)goalColumn_, LineEnd(shown, target));
        break;
    }
    default:
        return;
    }
    caret_ = (int)at;
    if (!shift) anchor_ = caret_;
    box.EditSelect(std::min(anchor_, caret_), std::max(anchor_, caret_));
    lastCursor_ = caret_;
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
volatile bool s_escWanted = false;   // TakeEscape(true): a lone Escape is taken
volatile bool s_escTaken = false;    // and one was, since the last TakeEscape
constexpr int kDikEscape = 0x01;
// The arrows, Home and End (TextEdit's kNavKeys as DirectInput codes).
constexpr int kDikNavKeys[] = { 0xc8, 0xd0, 0xcb, 0xcd, 0xc7, 0xcf };

bool IsNavKey(int key) { return std::find(std::begin(kDikNavKeys), std::end(kDikNavKeys), key) != std::end(kDikNavKeys); }

int __fastcall Hk_KeyboardMsg(void* mgr, void* /*edx*/, int key, int down)
{
    hook::Busy busy;
    const uintptr_t guarded = s_guarded;
    const bool held = guarded && down && mem::Read<uintptr_t>((uintptr_t)mgr + kMgrFocus) == guarded;
    // TextEdit moves the cursor itself. With Shift held the box's own Left/Right also step it and
    // collapse the selection, so Shift+arrow selected one character and skipped one; the box
    // doesn't get these keys at all.
    if (held && IsNavKey(key)) return 0;
    // An Escape no control has: for TakeEscape (one press does one thing, so the game doesn't
    // also clear the target).
    if (!held && down && key == kDikEscape && s_escWanted) {
        s_escTaken = true;
        return 0;
    }
    const int r = oKeyboardMsg(mgr, key, down);
    if (!held) return r;
    if (key == kDikEscape) Wnd(guarded).DropFocus();
    return 0;   // taken by the UI
}

// Hooked the first time it is needed.
void EnsureHook()
{
    static bool tried = false;
    if (tried || !game::Available()) return;
    tried = true;
    hook::Install("key guard", (void*)addr::kKeyboardMsg, (void*)&Hk_KeyboardMsg, (void**)&oKeyboardMsg,
                  kKeyboardMsgPrologue);
}

} // namespace

void GuardKeys(Wnd control)
{
    if (control) EnsureHook();
    s_guarded = oKeyboardMsg ? control.ptr() : 0;

    // The game only moves the focus for a click on a window; a click in the world leaves it.
    static bool wasDown[3] = {};
    for (int i = 0; i < 3; ++i) {
        const bool down = input::Button(i);
        if (down && !wasDown[i] && control && control.HasFocus() && !Wnd::UnderCursor()) control.DropFocus();
        wasDown[i] = down;
    }
}

bool TakeEscape(bool want)
{
    if (want) EnsureHook();
    s_escWanted = want && oKeyboardMsg;
    const bool taken = s_escTaken;
    s_escTaken = false;
    return want && taken;
}

} // namespace game::wnd
