#pragma once
// Everything we know about eqgame.exe's memory (Titanium, build string "Oct 31 2005",
// image base 0x400000). Nothing here is touched unless Check() confirmed that build.
#include "util/math.h"
#include <cstdint>
#include <string>

namespace game {

namespace addr {
inline constexpr uintptr_t kImageBase    = 0x400000;
inline constexpr uintptr_t kMouselook    = 0x4929dd;   // mouselook function (reads the per-frame deltas)
// float 511.5: the heading-between-two-points function (0x46e42f, 16 callers, among them the
// camera controllers) turns degrees into heading as degrees / 360 * this, where a circle is 512,
// so every heading it returns is 0.1 % short and jumps by half a unit at north.
inline constexpr uintptr_t kHeadingScale = 0x64604c;
inline constexpr float     kHeadingScaleGame = 511.5f;
inline constexpr uintptr_t kDeadzoneYaw  = 0x492a68;   // jp 0x492a78: skips zeroing the heading
inline constexpr uintptr_t kDeadzonePit  = 0x492c3c;   // jp 0x492c4c: skips zeroing the pitch
inline constexpr uintptr_t kPlayerPtr    = 0x905cf8;   // pointer to the controlled spawn
inline constexpr uintptr_t kCameraMode   = 0x850ccc;   // 0 = first person
inline constexpr uintptr_t kMouseDX      = 0x9237d4;   // this frame's mouse deltas
inline constexpr uintptr_t kMouseDY      = 0x9237d8;
// Mouselook scaling inputs:
inline constexpr uintptr_t kScreenLeft   = 0x923778, kScreenTop = 0x92377c;
inline constexpr uintptr_t kScreenRight  = 0x923780, kScreenBottom = 0x923784;
inline constexpr uintptr_t kMouseSens    = 0x9237dc;   // eqclient.ini MouseSensitivity
inline constexpr uintptr_t kInvertMouse  = 0x923840;   // byte
// Camera placement. Each frame kCameraUpdate (thiscall, this = the spawn the camera follows)
// runs the controller for the current camera mode, which builds a desired eye around the spawn
// and sweeps it with kCollideLine, then hands the controller's eye and angles to the engine
// camera (SetOrientation, SetPosition).
inline constexpr uintptr_t kCameraUpdate = 0x56ffa8;
// bool __thiscall CollideLine(display, from x/y/z, to x/y/z, float* outX, float* outY, float* outZ, char flags):
// sweeps from -> to through the world and writes where it stops (`to` when nothing is in the way).
inline constexpr uintptr_t kCollideLine  = 0x43a15c;
inline constexpr uintptr_t kCameraCtrls  = 0x9254fc;   // camera controller per camera mode (9 pointers)
inline constexpr uintptr_t kPitchDrop    = 0x65fb2c;   // float the controllers may subtract from a fixed pitch
// UI windows. Every window built from an XML screen is a CSidlScreenWnd, and every derived
// destructor ends in kSidlDtor.
inline constexpr uintptr_t kSidlDtor      = 0x5a3100;  // void __thiscall(wnd): base-class destructor
// Where windows are, the mouse and the keys.
inline constexpr uintptr_t kWindowAt      = 0x5ae730;  // CXWnd* __thiscall(manager, const POINT* screen, int* hitCode), ret 8: the
                                                       // window under a point, 0 for none (the game world); keeps a one-point cache
inline constexpr uintptr_t kScreenRect    = 0x59d960;  // int* __thiscall(wnd, int rect[4]), ret 4: left, top, right, bottom on screen
                                                       // (anchored children too, from the parent's client rect)
inline constexpr uintptr_t kMoveWnd       = 0x59fb00;  // void __thiscall(wnd, const int rect[4]), ret 4: CXWnd::Move to left, top,
                                                       // right, bottom (refused below the minimum size); lays out the children
inline constexpr uintptr_t kScreenWidth   = 0x92379c;  // int: the UI's screen size (0x5595db reads both; the UI ini's XPos<w>x<h>)
inline constexpr uintptr_t kScreenHeight  = 0x9237a0;
inline constexpr uintptr_t kWndCursor     = 0x59d150;  // void* __thiscall(wnd): the cursor to show over it (vtable +0x30, every
                                                       // window class we build). Sizable: drag on the title bar, resize on the
                                                       // edges, default inside; not sizable: drag over the whole window
inline constexpr uintptr_t kWndMgrRef     = 0x99d334;  // CXWndManager**: how kWndCursor reaches the UI manager
inline constexpr uintptr_t kKeyboardMsg   = 0x5ae430;  // int __thiscall(manager, int scancode, int down), ret 8: the UI's turn at a key;
                                                       // 0 = the UI took it (a key down then skips the game's own handling, 0x4b23e9)
// The UI manager: its first list (count +0x4, CXWnd** +0x8) holds every live window, children included.
inline constexpr uintptr_t kWndManager    = 0x99d3ec;
// Building a window of our own (the same steps as the map and inventory constructors).
inline constexpr uintptr_t kNew           = 0x60d071;  // void* __cdecl operator new(size_t)
inline constexpr uintptr_t kSidlCtor      = 0x5a3850;  // wnd* __thiscall(wnd, parent, const CXStr& screen, flags, int, const char* ini), ret 0x14
inline constexpr uintptr_t kCreateChildren = 0x5a25b0; // void __thiscall(wnd): builds the screen's pieces
inline constexpr uintptr_t kButtonSetCheck = 0x5bb140; // void __thiscall(button, bool), ret 4 (writes kChecked)
inline constexpr uintptr_t kGetChildItem  = 0x5a1e10;  // CXWnd* __thiscall(wnd, const CXStr& screenId), ret 4
inline constexpr uintptr_t kXStrCtor      = 0x571e00;  // void __thiscall(CXStr* slot, const char*), ret 4
inline constexpr uintptr_t kXStrCtorWide  = 0x571f00;  // void __thiscall(CXStr* slot, const wchar_t*), ret 4: a UTF-16 string
inline constexpr uintptr_t kXStrFree      = 0x571d00;  // void __thiscall(CXStr* slot, rep), ret 4
// Controls (notes/internals/ui-windows.md, "Notepad controls").
inline constexpr uintptr_t kListAddString = 0x59b9d0;  // int __thiscall(list, const CXStr* text, ARGB color, int data, void* anim), ret 0x10: the new row
inline constexpr uintptr_t kListGetCurSel = 0x597be0;  // int __thiscall(list): the selected row, -1 for none
inline constexpr uintptr_t kListSetCurSel = 0x597c20;  // void __thiscall(list, int row), ret 4
inline constexpr uintptr_t kSetFocusWnd   = 0x5ae0f0;  // CXWnd* __thiscall(manager, CXWnd* wnd or 0), ret 4: tells the old focus window
                                                       // (+0xa8), stores manager +0x44, tells the new one (+0xa4)
inline constexpr uintptr_t kEditInsert    = 0x5b90e0;  // void __thiscall(edit, CXStr text, int), ret 8: replaces the selection with
                                                       // `text` (at the cursor if none); the callee frees the CXStr (paste passes 0)
inline constexpr uintptr_t kStmlSetText   = 0x5a8b00;  // void __thiscall(stml, CXStr text, int, int), ret 0xc; the callee frees the CXStr (stock calls pass 1, 0)
inline constexpr uintptr_t kStmlActivateLink = 0x5ac550;  // void __thiscall(stml, StmlLink link by value), ret 0x10: a clicked
                                                       // <a> (from LButtonUp 0x5acc90); switch on the type, frees link.text
// UI (re)load. The XML is parsed by the loader, which frees every earlier definition first, so a
// screen exists only if it came in with the EQUI.xml load. kBuildGameUi then creates the in-world
// windows; kResetGameUi deletes them all (log line "Resetting game UI.").
inline constexpr uintptr_t kUiLoader      = 0x5b60b0;  // int __thiscall(sidlMgr, const CXStr& dir, const CXStr&, const CXStr& file), ret 0xc
inline constexpr uintptr_t kBuildGameUi   = 0x43efdd;  // __thiscall(this), no arguments
inline constexpr uintptr_t kResetGameUi   = 0x43af46;  // __thiscall(this), no arguments
// The inventory window, one of the in-world windows: set by kBuildGameUi, back to 0 in kResetGameUi.
inline constexpr uintptr_t kInventoryWnd  = 0x850c34;
// Zone names: [player + kZoneId] is the zone id; the table holds a pointer per id.
inline constexpr uintptr_t kZoneTable     = 0x905cc0;  // pointer to the zone list object
// Chat: every line printed to the chat windows goes through kChatPrint
// (void __thiscall(CEverQuest* [kEverQuest], const char* text, int color, bool), ret 0xc).
inline constexpr uintptr_t kChatPrint     = 0x4982c6;
// Window manager, left button down, after the hit test (eax = hit code, edi = window):
//   kDragSizableTest: mov edx,[edi+0x64]; test dh,2; jne kDragResize   (8 bytes)
// A sizable window goes to the resize code, which only moves by the title bar; any other window
// moves when clicked anywhere (kDragMove).
inline constexpr uintptr_t kDragSizableTest = 0x5b0d79;
inline constexpr uintptr_t kDragMove        = 0x5b0d81;
inline constexpr uintptr_t kDragResize      = 0x5b0dda;
inline constexpr uintptr_t kEverQuest     = 0x988df0;  // CEverQuest*
// eqstr_*.txt texts: const char* __thiscall(table [kStringTable], int id, int), ret 8.
inline constexpr uintptr_t kStringTable   = 0x905cb0;
inline constexpr uintptr_t kGetString     = 0x5dfa00;
// Combat: every hit the client turns into a combat line (melee, spells, local damage) goes through
// kHitReport (__thiscall(CEverQuest* [kEverQuest], const hit*, bool, int), ret 0xc). Callers: the
// damage packet handler (0x452d37), the spell action code (0x4a8f75), local damage (0x41d39f).
inline constexpr uintptr_t kHitReport     = 0x4a56e3;
inline constexpr uintptr_t kLocalPlayer   = 0x905ce4;  // the player's own spawn (the one "YOU" lines compare with)
inline constexpr uintptr_t kTarget        = 0x905d04;  // the targeted spawn, or 0 (what ability use 0x41f4a0 is given)
inline constexpr uintptr_t kSpawnTable    = 0x900f50;  // spawn* per spawn id, 0 < id < kMaxSpawnId (what 0x401b28 reads)
inline constexpr int       kMaxSpawnId    = 3000;
// Spell actions: the OP_Action packet (and the client's own hit animations) go to kSpellAction
// (__thiscall(target spawn, const action*), ret 4); see game/actions.h for the layout.
inline constexpr uintptr_t kSpellAction   = 0x47f614;
// The character's own HP, as the game works out its HP percent for the UI (0x559904): int
// __thiscall(charInfo = [kCharData] + kCharInfo, int spawnType), ret 4, for both.
inline constexpr uintptr_t kCharData      = 0x905d00;
inline constexpr int       kCharInfo      = 0xc280;
inline constexpr uintptr_t kCurHp         = 0x417e07;
inline constexpr uintptr_t kMaxHp         = 0x416963;
inline constexpr uintptr_t kEyeHeight     = 0x489bf5;  // float __thiscall(spawn): eye height above spawn::kCamZ
// Text in the game's own fonts. The fonts are listed in the window manager (wnd::kMgrFonts);
// each is a CTextureFont.
inline constexpr uintptr_t kFontDraw      = 0x5a3c80;  // int __thiscall(font, const CXStr&, x, y, width, const CXRect& clip, argb, flags, startX), ret 0x20
inline constexpr uintptr_t kFontWidth     = 0x5a3a00;  // int __thiscall(font, const CXStr&), ret 4: width in pixels
inline constexpr uintptr_t kFontHeight    = 0x5a3a20;  // int __thiscall(font): line height in pixels
// The main loop, right after the game draws its own net-status text (0x4456ca, only when /netstat
// is on): `mov ecx,[0x985c88]` (6 bytes), reached every pass. Our text is drawn from there.
inline constexpr uintptr_t kHudPoint      = 0x4456d2;
// Player models: the global model loader (0x43df32, "Initializing global data.", run on entering
// the world) asks kUseLuclinModel per race and gender which ones get their global<tag>_chr.s3d.
// int __thiscall(this, race, gender 0 male / 1 female), ret 8. It says no unless the server's
// expansion mask (kExpansionMask, bit 2 Luclin, bit 3 PoP) allows it.
inline constexpr uintptr_t kUseLuclinModel    = 0x43ac64;
inline constexpr uintptr_t kExpansionMask     = 0x9247b8;
// In the loader, once a Luclin model is loaded: `jne` past the Velious armor textures unless
// eqclient.ini LoadVeliousArmorsWithLuclin is TRUE.
inline constexpr uintptr_t kVeliousLuclinGate = 0x43ea9d;
} // namespace addr

// The zone list object: record pointers at +0x1c + id*4; each record has the long name at
// +0x91 (ids 1..1000, with the instance in the high 16 bits).
namespace zonelist {
inline constexpr int kRecords = 0x1c, kLongName = 0x91;
inline constexpr int kMaxId = 1000;
} // namespace zonelist

// Field offsets inside the spawn struct.
namespace spawn {
inline constexpr int kX = 0x30, kY = 0x34, kZ = 0x2C;          // found in-game by matching the first-person eye
inline constexpr int kCamZ = 0x38;                             // the z the camera controllers build on (eye height goes on top)
inline constexpr int kHeading = 0x4c, kTurnSpeed = 0x58, kPitch = 0x5c;
inline constexpr int kZoneId = 0x204;                          // the zone the character is in
inline constexpr int kName = 0x120;                            // char[0x40]
inline constexpr int kType = 0x244;                            // byte: 0 player, 1 NPC, 2 player corpse, 3 NPC corpse
inline constexpr int kLevel = 0x24c;                           // byte (the label's "%s [%d %s]" reads it)
inline constexpr int kHasMaster = 0x1b3;                       // byte: set on pets (tested before kMasterId)
inline constexpr int kId = 0x258;                              // spawn id
inline constexpr int kMasterId = 0x25c;                        // the owner's spawn id (pets)
inline constexpr int kHp = 0x274;                              // current HP: exact on the player's own spawn, in percent on
                                                               // every other (the HP update 0x457e57 stores cur * 100 / max)
inline constexpr int kActor = 0x1a8;                           // actor holder: [holder] is the engine actor
} // namespace spawn

// The engine actor (EQGraphicsDX9.dll) behind a spawn: [[spawn + spawn::kActor]].
namespace actor {
inline constexpr int kNameTag = 0x1a0;      // the name label object (set through the actor's vtable +0x140), or 0
inline constexpr int kTagRect = 0x10;       // in the label: left, top, right, bottom in screen pixels, written
                                            // when the engine draws the label (stale while it isn't drawn)
inline constexpr int kTagDepth = 0x3c;      // in the label: the post-projection z its text is drawn and
                                            // depth-tested at (the rect setter's last argument, sprite +0x38)
inline constexpr int kTagColor = 0x20;      // in the label: the ARGB its text is drawn in (sprite +0x1c); the
                                            // engine lowers the alpha to fade far names out
inline constexpr int kTagTextHeight = 0x48; // in the label: the text's height as measured in the label font (an int,
                                            // stored from eax at 0x1004eb19; the drawn box keeps
                                            // the measured width : height)
inline constexpr int kTagLineHeight = 41;   // what kTagTextHeight reads for one line (seen live on every label)
} // namespace actor

// Fields of a UI window (CXWnd / CSidlScreenWnd) and its vtable.
namespace wnd {
inline constexpr int kVisible = 0x4;        // byte: shown (what IsVisible 0x59dd30 returns)
inline constexpr int kCloseOnEsc = 0x18;    // byte: Esc may close it (CLOSE_TOP_WINDOW, 0x5ae6b0: the topmost
                                            // top-level window with this set is hidden); the CXWnd ctor sets 1
inline constexpr int kStyle = 0x64;         // style bits (title bar, border, ...)
inline constexpr int kTemplate = 0x124;     // the screen definition found by name; 0 = not loaded
inline constexpr int kTemplateStyle = 0x60; // the same style bits, inside the definition
inline constexpr int kDelete = 0x4;         // vtable offset: deleting destructor, (wnd, 1)
inline constexpr int kActivate = 0x84;      // vtable offset: Show(true, true)
inline constexpr int kShow = 0xc0;          // vtable offset: Show(bool show, bool bringToTop), ret 8
inline constexpr int kChecked = 0x128;      // in a button: checked (pressed in); a Style_Checkbox button flips it on click
inline constexpr int kSetText = 0xfc;       // vtable offset: SetWindowText(const CXStr&), ret 4
inline constexpr int kStoreIni = 0x11c;     // vtable offset: StoreIniInfo(), writes position/size to the UI ini
inline constexpr int kListDeleteAll = 0x130;// vtable offset in a list: DeleteAll(), no arguments
inline constexpr int kRect = 0x3c;         // ints: the location rect on screen, left, top, right, bottom
inline constexpr int kWindowText = 0x74;    // CXStr: the window's text (what an Editbox holds while it is typed in)
inline constexpr int kEditMaxChars = 0x128; // in an Editbox: int, the longest text it takes (-1 = no limit)
inline constexpr int kEditSelStart = 0x120, kEditSelEnd = 0x124;   // in an Editbox: ints, the selection (equal: the cursor), in characters
inline constexpr int kMgrDefaultCursor = 0x134;   // in the UI manager: the cursor kWndCursor returns for "nothing special"
inline constexpr int kMgrMouse = 0x74;      // in the UI manager: POINT, the cursor on screen (what a click is tested at)
inline constexpr int kMgrFocus = 0x44;      // in the UI manager: the window with the keyboard focus (0x5ae150 returns it)
inline constexpr unsigned kEditReturnNewline = 0x40000;   // style bit of an Editbox: Enter types a line break (0x0a); without it
                                                           // Enter tells the parent (notification 6). Style_Multiline doesn't set it
                                                           // (key handler 0x5b9730, Enter case 0x5b99b8)
inline constexpr int kSidlSize = 0x15c;     // sizeof(CSidlScreenWnd)
inline constexpr int kMgrCount = 0x4;       // in the UI manager: number of live windows
inline constexpr int kMgrItems = 0x8;       // in the UI manager: CXWnd** to them
inline constexpr int kMgrFontCount = 0xf4;  // in the UI manager: number of fonts
inline constexpr int kMgrFonts = 0xf8;      // in the UI manager: CTextureFont** (the net-status text uses [2])
// Flags and mode the inventory window is built with: position, fade and lock go to the ini.
inline constexpr unsigned kIniFlags = 0xfffffeff;
inline constexpr int kIniMode = 2;
} // namespace wnd

// The SIDL manager (the object the UI loader is called on).
namespace sidlmgr {
inline constexpr int kLoaderFailed = 0x260;   // byte the loader sets when it fails ("Error reading XML.")
inline constexpr int kLoaderError = 0x264;    // CXStr: the parser's message for it (0x442d80 logs it as "UI ERROR: %s")
} // namespace sidlmgr

// A CXStr is a pointer to { ..., int length at +0x8, chars at +0x14 }.
namespace xstr {
inline constexpr int kLength = 0x8, kChars = 0x14;   // length in characters
inline constexpr int kEncoding = 0xc;   // 0: 8-bit characters, 1: UTF-16 (what an Editbox holds while typed in; verified live)
} // namespace xstr

// Fields of a camera controller (see kCameraCtrls).
namespace camctl {
inline constexpr int kAngles = 0x10;      // heading, pitch, roll handed to SetOrientation
inline constexpr int kFixedPitch = 0x30;  // the mode's own camera pitch
} // namespace camctl

// push ebp; mov ebp,esp; sub esp,0xC
inline constexpr unsigned char kMouselookPrologue[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c };
// push ebp; mov ebp,esp; sub esp,0x10; push ebx; push esi; mov esi,ecx
inline constexpr unsigned char kCameraUpdatePrologue[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x53, 0x56, 0x8b, 0xf1 };
// push ebp; mov ebp,esp; sub esp,0x34; push ebx; xor ebx,ebx
inline constexpr unsigned char kCollideLinePrologue[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x34, 0x53, 0x33, 0xdb };

// push -1; push <seh handler>; mov eax, fs:[0]
inline constexpr unsigned char kSidlDtorPrologue[] = { 0x6a, 0xff, 0x68, 0x42, 0x2f, 0x63, 0x00, 0x64, 0xa1 };
// sub esp,0x10; push ebx; push ebp; push esi; mov esi,ecx
inline constexpr unsigned char kKeyboardMsgPrologue[] = { 0x83, 0xec, 0x10, 0x53, 0x55, 0x56, 0x8b, 0xf1 };

// push -1; push <seh handler>; mov eax, fs:[0]
inline constexpr unsigned char kUiLoaderPrologue[] = { 0x6a, 0xff, 0x68, 0x5d, 0x3f, 0x63, 0x00, 0x64, 0xa1 };
// mov eax, <seh handler>   (then call __SEH_prolog)
inline constexpr unsigned char kBuildGameUiPrologue[] = { 0xb8, 0x72, 0x63, 0x62, 0x00 };
// push esi; push edi; push <"Resetting game UI.">
inline constexpr unsigned char kResetGameUiPrologue[] = { 0x56, 0x57, 0x68, 0xf8, 0x25, 0x64, 0x00 };

// push -1; push <seh handler>; mov eax, fs:[0]
inline constexpr unsigned char kStmlActivateLinkPrologue[] = { 0x6a, 0xff, 0x68, 0x18, 0x36, 0x63, 0x00, 0x64, 0xa1 };
// mov eax,[0x99d334]; sub esp,0xc; push esi
inline constexpr unsigned char kWndCursorPrologue[] = { 0xa1, 0x34, 0xd3, 0x99, 0x00, 0x83, 0xec, 0x0c, 0x56 };
inline constexpr unsigned char kDragSizableTestBytes[] = { 0x8b, 0x57, 0x64, 0xf6, 0xc6, 0x02, 0x75, 0x59 };

// push ebp; mov eax,0x13c4; lea ebp,[esp-0x134c]
inline constexpr unsigned char kHitReportPrologue[] = { 0x55, 0xb8, 0xc4, 0x13, 0x00, 0x00, 0x8d, 0xac, 0x24, 0xb4, 0xec, 0xff, 0xff };
// push ebx; push esi; mov esi,ecx; mov eax,[esi+4]; mov eax,[eax+4]   (both); cur HP then mov ebx,[esi+0x50]
inline constexpr unsigned char kCurHpPrologue[] = { 0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x8b, 0x40, 0x04, 0x8b, 0x5e, 0x50 };
inline constexpr unsigned char kMaxHpPrologue[] = { 0x53, 0x56, 0x8b, 0xf1, 0x8b, 0x46, 0x04, 0x8b, 0x40, 0x04 };
// push esi; mov esi,ecx; push edi; lea edi,[esi+0x1a8]
inline constexpr unsigned char kEyeHeightPrologue[] = { 0x56, 0x8b, 0xf1, 0x57, 0x8d, 0xbe, 0xa8, 0x01, 0x00, 0x00 };
// sub esp,0x10; mov edx,[esp+0x1c]
inline constexpr unsigned char kFontDrawPrologue[] = { 0x83, 0xec, 0x10, 0x8b, 0x54, 0x24, 0x1c };
// mov eax,[esp+4]; mov ecx,[ecx+4]; push eax; push ecx
inline constexpr unsigned char kFontWidthPrologue[] = { 0x8b, 0x44, 0x24, 0x04, 0x8b, 0x49, 0x04, 0x50, 0x51 };
// mov eax,[ecx+4]; push eax
inline constexpr unsigned char kFontHeightPrologue[] = { 0x8b, 0x41, 0x04, 0x50 };
// mov ecx,[0x985c88]
inline constexpr unsigned char kHudPointBytes[] = { 0x8b, 0x0d, 0x88, 0x5c, 0x98, 0x00 };

// push ebx; push ebp; push esi; push edi; mov edi,ecx; xor ebx,ebx
inline constexpr unsigned char kSpellActionPrologue[] = { 0x53, 0x55, 0x56, 0x57, 0x8b, 0xf9, 0x33, 0xdb };

// push ebp; mov ebp,esp; mov eax,0x82
inline constexpr unsigned char kUseLuclinModelPrologue[] = { 0x55, 0x8b, 0xec, 0xb8, 0x82, 0x00, 0x00, 0x00 };
// jne 0x43ebbe
inline constexpr unsigned char kVeliousLuclinGateBytes[] = { 0x0f, 0x85, 0x1b, 0x01, 0x00, 0x00 };

// mov eax, <seh handler>   (then call __SEH_prolog)
inline constexpr unsigned char kChatPrintPrologue[] = { 0xb8, 0x0a, 0x70, 0x62, 0x00 };

// One 60 FPS frame of the game's turn integration: heading += speed * 0.02 * ms.
inline constexpr float kTurnPerSpeed = 0.02f * (1000.0f / 60.0f);

void Check();            // once: is this the expected eqgame.exe? (safe to call every frame)
bool Available();        // yes

int  CameraMode();       // 0 = first person, >0 other modes, -1 unknown
bool IsFirstPerson();

uintptr_t PlayerBase();                       // controlled spawn struct, or 0
bool      PlayerPos(math::Vec3& pos);         // false when there is no player
float     PlayerHeading();                    // EQ angle units, 0 when there is no player
// Where a spawn stands, as the game's cameras see it (x, y and kCamZ). False for a bad spawn.
bool      SpawnCameraPos(uintptr_t spawnBase, math::Vec3& pos);

// How far right-click mouselook turns per mouse count, in radians (yaw at the 60 FPS rate,
// which is what frame-rate independent mouselook gives), and whether the game inverts Y.
bool MouselookRates(float& yawRad, float& pitchRad, bool& invert);

// The player's own spawn ([kLocalPlayer]), or 0.
uintptr_t LocalPlayer();
// The spawn with this id, or 0 (read from the game's table; valid until the spawn leaves).
uintptr_t SpawnById(int id);
int       SpawnId(uintptr_t spawnBase);          // 0 for a bad spawn
bool      IsPetOf(uintptr_t spawnBase, int ownerId);
bool      IsPet(uintptr_t spawnBase);                // anyone's pet
bool      IsPlayer(uintptr_t spawnBase);             // a player character (not an NPC or corpse)
bool      IsPlayersPet(uintptr_t spawnBase);         // a pet whose owner is a player character
int       SpawnHp(uintptr_t spawnBase);          // current HP (exact only for the player's own spawn, else percent)
uintptr_t Target();                              // the targeted spawn, or 0
int       OwnHpPercent();                        // the character's HP in percent (as the game's UI has it), or -1
int       SpawnType(uintptr_t spawnBase);        // spawn::kType, -1 for a bad spawn
int       SpawnLevel(uintptr_t spawnBase);       // 0 for a bad spawn
std::string SpawnName(uintptr_t spawnBase);      // as the client keeps it ("a_gnoll01"); empty for a bad spawn
// The engine actor behind the spawn ([[spawn + kActor]], what the label hooks are given), or 0.
uintptr_t SpawnActor(uintptr_t spawnBase);
// Where the spawn's eyes are, in world coordinates (camera z + the game's eye height, which also
// goes to *eyeHeight). False while the spawn has no model yet (zoning, just arrived).
bool      SpawnEyePos(uintptr_t spawnBase, math::Vec3& pos, float* eyeHeight = nullptr);

// The spawn's name label as the engine last drew it.
struct Label {
    int   left, top, right, bottom;   // screen pixels
    float depth;                      // the z its text is depth-tested at (0 near .. 1 far)
    int   lines;                      // lines of text (from its measured height)
    float alpha;                      // 0..1: how far the engine has faded it (1 = not at all)
};
// False when the spawn has no label, or (once game/labels.h is installed) when the engine didn't
// draw it in the last frame or two, so what is read is current.
bool      SpawnLabel(uintptr_t spawnBase, Label& label);

// The long name of the zone the character is in ("Butcherblock Mountains"). False when there is
// no character or the zone is unknown.
bool ZoneName(std::string& name);
// The zone id the character is in (instance bits removed), or 0.
int ZoneId();
// Where the character stands, as /loc prints it: "-975.37, -1.51, -0.49" (the game's own order
// and two decimals; the command reads the spawn at x, y and kCamZ). False when there is no character.
bool PlayerLocText(std::string& text);
// The character's name. False when there is no character.
bool PlayerName(std::string& name);
// The in-world UI is built (between kBuildGameUi and kResetGameUi): the character is in a zone
// that has finished loading. False at character select and while zoning.
bool InWorld();

} // namespace game
