// The notepad: two game windows (see game/ui/custom_window.h) and the notes behind them.
//
// The icon is a small window holding a copy of the skin's spellbook button (the book on the spell
// gem window: its art, hover and open look) that opens and closes the notepad, and shows the book
// open while the notepad is. The notepad has a list of notes (newest first), and for the selected
// one a header (when and where it was written) and its text, shown formatted (View, Markdown turned
// into the game's colours) or as typed (Edit, a game::wnd::TextEdit). The text is written to the
// note's file a moment after the last change (notebook.h).
//
// The header is only read from the game's memory (the zone, the position /loc would print, the
// character's name); we never use /loc or any command.
#include "features/notepad/notepad.h"
#include "core/host.h"
#include "core/log.h"
#include "features/notepad/markdown.h"
#include "features/notepad/notebook.h"
#include "game/eqgame.h"
#include "game/ui/custom_window.h"
#include "game/ui/sidl_xml.h"
#include "game/ui/stml_link.h"
#include "game/ui/text_edit.h"
#include "overlay/overlay.h"
#include "overlay/widgets.h"
#include <imgui.h>
#include <windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>

namespace notepad {

Settings cfg;

namespace {

namespace sidl = game::wnd::sidl;

constexpr char kNotepadScreen[] = "P99PlusNotepadWnd";   // also the window's section in the character's UI ini
constexpr char kIconScreen[]    = "P99PlusNotepadIconWnd";
constexpr char kIconLook[]      = "CSPW_SpellBook";    // the button copied: the spell gem window's book
constexpr char kIconButton[]    = "IconBtn";
constexpr int  kIconWndSize = 32;                     // the icon window; the button sits in its centre
constexpr size_t kRowChars = 48;                      // a list row: the note's first line, cut to this
constexpr ULONGLONG kDeleteConfirmMs = 3000;          // Delete asks "Sure?" for this long
constexpr unsigned kFocusRing = 0xff4ad2ff;   // ImGui ABGR: the gold of the Markdown headings (#ffd24a)

// ---- the windows ---------------------------------------------------------------------------

// The notepad's layout, in the window's client area. Left: the list (fixed width) with New and
// Delete under it. Right: a header (created, last updated, place), then the text, which gets all the rest.
std::string NotepadXml()
{
    // { top, bottom, left, right, leftFromLeft, topFromTop, bottomFromTop, rightFromLeft }
    const sidl::Anchors list   = { 4, 34, 4, 180, true, true, false, true };
    const sidl::Anchors newBtn = { 30, 6, 4, 90, true, false, false, true };
    const sidl::Anchors delBtn = { 30, 6, 94, 180, true, false, false, true };
    const sidl::Anchors date   = { 4, 22, 188, 82, true };
    const sidl::Anchors mode   = { 3, 25, 78, 4, false };
    const sidl::Anchors update = { 26, 44, 188, 6, true };
    const sidl::Anchors where  = { 48, 66, 188, 6, true };
    const sidl::Anchors body   = { 72, 6, 188, 6, true, true, false, false };
    const sidl::Anchors hint   = { 82, 100, 192, 10, true };

    sidl::Screen screen = { kNotepadScreen, 200, 120, 560, 380, {}, true, true, "Notepad" };
    screen.pieces = { "P99PlusNotepadList", "P99PlusNotepadNew", "P99PlusNotepadDelete", "P99PlusNotepadDate",
                      "P99PlusNotepadMode", "P99PlusNotepadUpdated", "P99PlusNotepadWhere", "P99PlusNotepadEditor", "P99PlusNotepadStml",
                      "P99PlusNotepadHint" };
    return sidl::Document({
        sidl::Listbox("P99PlusNotepadList", "NoteList", list, "Notes"),
        sidl::PushButton("P99PlusNotepadNew", "NewBtn", newBtn, "New"),
        sidl::PushButton("P99PlusNotepadDelete", "DelBtn", delBtn, "Delete"),
        sidl::Label("P99PlusNotepadDate", "DateLine", date, "-", false),
        sidl::PushButton("P99PlusNotepadMode", "ModeBtn", mode, "Edit"),
        sidl::Label("P99PlusNotepadUpdated", "UpdatedLine", update, "-", false),
        sidl::Label("P99PlusNotepadWhere", "WhereLine", where, "-", false),
        sidl::Editbox("P99PlusNotepadEditor", "Editor", body),
        sidl::StmlBox("P99PlusNotepadStml", "Stml", body),
        sidl::Label("P99PlusNotepadHint", "HintLine", hint, "No notes yet. Press New to write one.", true),
        sidl::BorderedScreen(screen),
    });
}

game::wnd::CustomWindowDef NotepadDef()
{
    game::wnd::CustomWindowDef d;
    d.screen = kNotepadScreen;
    d.xml = NotepadXml();
    d.startHidden = true;   // the icon, the key or the menu opens it
    return d;
}

game::wnd::CustomWindowDef IconDef()
{
    sidl::Screen screen = { kIconScreen, 1130, 10, kIconWndSize, kIconWndSize, { "P99PlusNotepadIconBtn" }, false, false, "" };
    game::wnd::CustomWindowDef d;
    d.screen = kIconScreen;
    d.xml = sidl::Document({
        sidl::BorrowedButton("P99PlusNotepadIconBtn", kIconButton, 0, 0, kIconWndSize, kIconWndSize, kIconLook),
        sidl::BorderedScreen(screen),
    });
    d.dragByBody = true;   // the margin around the button moves the icon
    return d;
}

game::wnd::CustomWindow s_window(NotepadDef());
game::wnd::CustomWindow s_icon(IconDef());
game::wnd::TextEdit     s_edit(s_window, "Editor");

// ---- state ---------------------------------------------------------------------------------

enum Mode { kView = 0, kEdit = 1 };

// What a core reload keeps (the notes themselves are on disk).
struct Stash { char id[96]; int mode; };
constexpr const char* kStashKey = "notepad.state";

int         s_selected = -1;       // index in book::All(), -1 for none
std::string s_selectedId;          // the same note by name: survives a reload of the folder
int         s_mode = kView;
std::string s_editorId;            // the note whose text the text box holds
bool        s_listDirty = true;    // the rows have to be built again
std::vector<std::string> s_rows;
bool        s_listPush = true;     // the list control has to be given the rows / the selection
bool        s_stmlDirty = true;    // the formatted text has to be set again
bool        s_wasVisible = false;
bool        s_restored = false;
ULONGLONG   s_deleteArmedUntil = 0;
bool        s_focusEditor = false;   // give the text box the keys (New, Edit)
keybind::Press s_openKey;

std::vector<Note>& Notes() { return book::All(); }
bool Valid(int i) { return i >= 0 && i < (int)Notes().size(); }

std::string RowText(const Note& n) { return FirstLine(n, kRowChars); }

// The text box holds the selected note, for editing.
bool EditingSelected() { return s_mode == kEdit && Valid(s_selected) && Notes()[s_selected].id == s_editorId; }

// The text box's text -> the selected note.
void NoteEdited()
{
    Note& n = Notes()[s_selected];
    const std::string row = RowText(n);
    n.text = s_edit.Text();
    std::string who;
    game::PlayerName(who);
    book::Changed(s_selected, who);
    s_stmlDirty = true;
    if (RowText(n) != row) s_listDirty = true;
}

void SyncFromEditor()
{
    if (EditingSelected() && s_edit.Sync()) NoteEdited();
}

void LoadEditor()
{
    if (!Valid(s_selected)) {
        s_edit.Load("", true);
        s_editorId.clear();
        return;
    }
    const Note& n = Notes()[s_selected];
    s_edit.Load(n.text, n.id != s_editorId);
    s_editorId = n.id;
}

// Writes the selected note now (`all`: every changed note). A new note gets its file, named after
// its first line, and with it a new id: the selection and the text box follow it.
void WriteNow(bool all)
{
    const bool held = Valid(s_selected) && Notes()[s_selected].id == s_editorId;
    if (all) book::Flush();
    else book::Settle(s_selected);
    if (!Valid(s_selected)) return;
    s_selectedId = Notes()[s_selected].id;
    if (held) s_editorId = s_selectedId;
}

// A new note's title is settled once the cursor is past the end of its first line.
void SettleTitle()
{
    if (!EditingSelected() || Notes()[s_selected].saved) return;
    const std::string& text = Notes()[s_selected].text;
    const size_t lineEnd = text.find('\n');
    if (lineEnd == std::string::npos) return;
    int start, end;
    s_edit.Box().EditSelection(start, end);
    if ((size_t)start > lineEnd) WriteNow(false);
}

void SelectionChanged()
{
    s_selectedId = Valid(s_selected) ? Notes()[s_selected].id : std::string();
    s_deleteArmedUntil = 0;
    LoadEditor();
    s_stmlDirty = true;
    s_listPush = true;
}

void SelectNote(int index)
{
    SyncFromEditor();
    WriteNow(false);   // the note left: a new one gets its file
    s_selected = index;
    s_mode = kView;
    SelectionChanged();
}

void NewNote()
{
    SyncFromEditor();
    WriteNow(false);
    std::string zone, loc, who;
    game::ZoneName(zone);
    game::PlayerLocText(loc);
    game::PlayerName(who);
    s_selected = book::Add(zone, loc, who);
    s_mode = kEdit;   // you are here to write
    s_listDirty = true;
    SelectionChanged();
    s_focusEditor = true;
}

void DeleteClicked()
{
    if (!Valid(s_selected)) return;
    const ULONGLONG now = GetTickCount64();
    if (now >= s_deleteArmedUntil) {   // the first click only asks
        s_deleteArmedUntil = now + kDeleteConfirmMs;
        return;
    }
    book::Remove(s_selected);
    s_editorId.clear();
    s_selected = std::min(s_selected, (int)Notes().size() - 1);
    s_mode = kView;
    s_listDirty = true;
    SelectionChanged();
}

void ToggleMode()
{
    if (!Valid(s_selected)) return;
    if (s_mode == kEdit) {
        SyncFromEditor();
        WriteNow(false);
        s_mode = kView;
        s_stmlDirty = true;
    } else {
        s_mode = kEdit;
        LoadEditor();
        s_focusEditor = true;
    }
}

void OnOpened()
{
    book::Load();   // files added or edited outside the game show up
    s_selected = -1;
    for (int i = 0; i < (int)Notes().size(); ++i)
        if (Notes()[i].id == s_selectedId) s_selected = i;
    if (s_selected < 0) {
        s_selected = Notes().empty() ? -1 : 0;
        s_mode = kView;
    }
    s_listDirty = true;
    s_editorId.clear();
    SelectionChanged();
}

void OnClosed()
{
    s_edit.Box().DropFocus();   // the keys go back to the game
    SyncFromEditor();
    WriteNow(true);
}

std::string HeaderDate(const Note& n)
{
    std::string s = "Created " + n.created;
    if (!n.character.empty()) s += "  by " + n.character;
    return s;
}

std::string HeaderUpdated(const Note& n)
{
    std::string s = "Updated " + n.updated;
    if (!n.updatedBy.empty()) s += "  by " + n.updatedBy;
    return s;
}

std::string HeaderWhere(const Note& n)
{
    std::string s;
    if (!n.zone.empty()) s = "Zone: " + n.zone;
    if (!n.loc.empty()) s += std::string(s.empty() ? "" : "    ") + "Loc: " + n.loc;
    return s.empty() ? "(place unknown)" : s;
}

// A link's target ("other.md", "./notes/My%20note.md#part") -> the note it names, -1 for none.
int FindLinked(const std::string& target)
{
    std::string name;
    for (size_t i = 0; i < target.size(); ++i) {   // %20 and the like
        if (target[i] == '%' && i + 2 < target.size() && std::isxdigit((unsigned char)target[i + 1]) &&
            std::isxdigit((unsigned char)target[i + 2])) {
            name += (char)std::stoi(target.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else {
            name += target[i] == '/' ? '\\' : target[i];
        }
    }
    name = name.substr(0, name.find('#'));
    const size_t slash = name.rfind('\\');   // only the file name: every note is in the one folder
    if (slash != std::string::npos) name = name.substr(slash + 1);
    if (name.size() > 3 && _stricmp(name.c_str() + name.size() - 3, ".md") == 0) name.resize(name.size() - 3);
    for (int i = 0; i < (int)Notes().size(); ++i)
        if (_stricmp(Notes()[i].id.c_str(), name.c_str()) == 0) return i;
    return -1;
}

void FollowLink(const std::string& target)
{
    const int i = FindLinked(target);
    if (i < 0) {
        Log("notepad: no note for the link \"%s\" in %s", target.c_str(), book::Folder().c_str());
        return;
    }
    SelectNote(i);
}

void Update()
{
    if (s_window.Clicked("NewBtn")) NewNote();
    if (s_window.Clicked("DelBtn")) DeleteClicked();
    if (s_window.Clicked("ModeBtn")) ToggleMode();
    if (EditingSelected() && s_edit.Update(!overlay::Visible())) NoteEdited();
    SettleTitle();

    // The list: rebuilt when a note was added, removed or its first line changed; the player's
    // click on a row selects that note.
    if (s_listDirty) {
        s_rows.clear();
        for (const Note& n : Notes()) s_rows.push_back(RowText(n));
        s_listDirty = false;
        s_listPush = true;
    }
    if (s_listPush) {
        s_window.SetList("NoteList", s_rows, s_selected);
        s_listPush = false;
    } else {
        const int clicked = s_window.ListSelection("NoteList");
        if (clicked != s_selected && clicked >= 0 && clicked < (int)Notes().size()) SelectNote(clicked);
    }

    const bool has = Valid(s_selected);
    s_window.ShowChild("Editor", has && s_mode == kEdit);
    // The text box has the keys while it is shown (the game's UI then keeps them from the game, as
    // for the chat line); hidden, it gives them back.
    if (has && s_mode == kEdit) {
        if (s_focusEditor) s_edit.Box().TakeFocus();
    } else {
        s_edit.Box().DropFocus();
    }
    s_focusEditor = false;
    s_window.ShowChild("Stml", has && s_mode == kView);
    s_window.ShowChild("ModeBtn", has);
    s_window.ShowChild("DateLine", has);
    s_window.ShowChild("UpdatedLine", has && !Notes()[s_selected].updated.empty());
    s_window.ShowChild("WhereLine", has);
    s_window.ShowChild("HintLine", !has);
    s_window.SetText("ModeBtn", s_mode == kEdit ? "View" : "Edit");
    s_window.SetText("DelBtn", GetTickCount64() < s_deleteArmedUntil ? "Sure?" : "Delete");
    if (!has) return;

    const Note& n = Notes()[s_selected];
    s_window.SetText("DateLine", HeaderDate(n));
    if (!n.updated.empty()) s_window.SetText("UpdatedLine", HeaderUpdated(n));
    s_window.SetText("WhereLine", HeaderWhere(n));
    if (s_mode == kView && s_stmlDirty) {
        s_window.SetStml("Stml", ToStml(n.text));
        s_stmlDirty = false;
    }
    std::string target;
    if (s_mode == kView && game::wnd::TakeLink(s_window.Child("Stml"), target)) FollowLink(target);
}

void Toggle() { s_window.SetShown(!s_window.Visible()); }

void PollHotkey()
{
    if (s_openKey.Poll(cfg.open_key) && !overlay::Visible() && GetForegroundWindow() == g_host->hwnd) Toggle();
}

void OnFrame()
{
    if (!s_restored) {
        s_restored = true;
        Stash st = {};
        if (stash::GetValue(kStashKey, st)) {
            s_selectedId = st.id;
            s_mode = st.mode == kEdit ? kEdit : kView;
        }
        stash::Clear(kStashKey);
    }
    book::Tick();
    s_window.SetWanted(cfg.enabled);
    s_icon.SetWanted(cfg.enabled && cfg.show_icon);
    PollHotkey();
    // The book opens and closes the notepad, and shows open while it is (as the spellbook button does).
    if (s_icon.Clicked(kIconButton)) Toggle();
    s_icon.SetCheck(kIconButton, s_window.Visible());

    // Typing in the note: no key press reaches the game (Ctrl+A would turn the character).
    const bool typing = s_window.Visible() && s_mode == kEdit && Valid(s_selected);
    const game::wnd::Wnd editor = typing ? s_edit.Box() : game::wnd::Wnd();
    game::wnd::GuardKeys(editor);
    int l, t, r, b;
    if (editor.HasFocus() && editor.ScreenRect(l, t, r, b))   // the keys go to the note: say so
        overlay::Highlight((float)l - 2, (float)t - 2, (float)r + 2, (float)b + 2, kFocusRing);

    if (!s_window.Exists()) {
        s_wasVisible = false;
        return;
    }
    const bool visible = s_window.Visible();
    if (visible && !s_wasVisible) OnOpened();
    if (!visible && s_wasVisible) OnClosed();
    s_wasVisible = visible;
    if (visible) Update();
}

void OnShutdown()
{
    game::wnd::GuardKeys(game::wnd::Wnd());
    s_edit.Box().DropFocus();
    SyncFromEditor();
    WriteNow(true);   // before the id goes into the stash: a new note's changes with its file
    Stash st = {};
    snprintf(st.id, sizeof(st.id), "%s", s_selectedId.c_str());
    st.mode = s_mode;
    stash::PutValue(kStashKey, st);
}

const Setting kSettings[] = {
    BoolSetting("notepad", "enabled", &cfg.enabled, "Show the notepad icon and window."),
    BoolSetting("notepad", "show_icon", &cfg.show_icon, "Show the small book icon that opens the notepad."),
    KeySetting ("notepad", "open_key", &cfg.open_key, "Key that opens and closes the notepad: optional ctrl+/shift+/alt+ prefixes, then a virtual-key code (0x4E = N)."),
};

void DrawMenu()
{
    ui::WindowStatusHint(s_window);
    ImGui::Checkbox("Show the icon", &cfg.show_icon);

    ui::KeyBindingRow("Open key", cfg.open_key, Settings{}.open_key);
    ui::Hint("Notes are saved as Markdown files in %s", book::Folder().c_str());
}

} // namespace

const Feature feature = {
    "Notepad",
    "Notes that record when and where you wrote them, kept as Markdown files.",
    "Enable the notepad", &cfg.enabled,
    kSettings, std::size(kSettings), OnFrame, DrawMenu, OnShutdown,
};

} // namespace notepad
