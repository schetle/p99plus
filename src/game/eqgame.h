#pragma once
// Everything we know about eqgame.exe's memory (Titanium, build string "Oct 31 2005",
// image base 0x400000). Nothing here is touched unless Check() confirmed that build.
#include "util/math.h"
#include <cstdint>

namespace game {

namespace addr {
inline constexpr uintptr_t kImageBase    = 0x400000;
inline constexpr uintptr_t kMouselook    = 0x4929dd;   // mouselook function (reads the per-frame deltas)
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
// The UI manager: its first list (count +0x4, CXWnd** +0x8) holds every live window, children included.
inline constexpr uintptr_t kWndManager    = 0x99d3ec;
// Building a window of our own (the same steps as the map and inventory constructors).
inline constexpr uintptr_t kNew           = 0x60d071;  // void* __cdecl operator new(size_t)
inline constexpr uintptr_t kSidlCtor      = 0x5a3850;  // wnd* __thiscall(wnd, parent, const CXStr& screen, flags, int, const char* ini), ret 0x14
inline constexpr uintptr_t kCreateChildren = 0x5a25b0; // void __thiscall(wnd): builds the screen's pieces
inline constexpr uintptr_t kButtonSetCheck = 0x5bb140; // void __thiscall(button, bool), ret 4 (writes kChecked)
inline constexpr uintptr_t kGetChildItem  = 0x5a1e10;  // CXWnd* __thiscall(wnd, const CXStr& screenId), ret 4
inline constexpr uintptr_t kXStrCtor      = 0x571e00;  // void __thiscall(CXStr* slot, const char*), ret 4
inline constexpr uintptr_t kXStrFree      = 0x571d00;  // void __thiscall(CXStr* slot, rep), ret 4
// UI (re)load. The XML is parsed by the loader, which frees every earlier definition first, so a
// screen exists only if it came in with the EQUI.xml load. kBuildGameUi then creates the in-world
// windows; kResetGameUi deletes them all (log line "Resetting game UI.").
inline constexpr uintptr_t kUiLoader      = 0x5b60b0;  // int __thiscall(sidlMgr, const CXStr& dir, const CXStr&, const CXStr& file), ret 0xc
inline constexpr uintptr_t kBuildGameUi   = 0x43efdd;  // __thiscall(this), no arguments
inline constexpr uintptr_t kResetGameUi   = 0x43af46;  // __thiscall(this), no arguments
// The inventory window, one of the in-world windows: set by kBuildGameUi, back to 0 in kResetGameUi.
inline constexpr uintptr_t kInventoryWnd  = 0x850c34;
// Window manager, left button down, after the hit test (eax = hit code, edi = window):
//   kDragSizableTest: mov edx,[edi+0x64]; test dh,2; jne kDragResize   (8 bytes)
// A sizable window goes to the resize code, which only moves by the title bar; any other window
// moves when clicked anywhere (kDragMove).
inline constexpr uintptr_t kDragSizableTest = 0x5b0d79;
inline constexpr uintptr_t kDragMove        = 0x5b0d81;
inline constexpr uintptr_t kDragResize      = 0x5b0dda;
} // namespace addr

// Field offsets inside the spawn struct.
namespace spawn {
inline constexpr int kX = 0x30, kY = 0x34, kZ = 0x2C;          // found in-game by matching the first-person eye
inline constexpr int kCamZ = 0x38;                             // the z the camera controllers build on (eye height goes on top)
inline constexpr int kHeading = 0x4c, kTurnSpeed = 0x58, kPitch = 0x5c;
} // namespace spawn

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
inline constexpr int kSidlSize = 0x15c;     // sizeof(CSidlScreenWnd)
inline constexpr int kMgrCount = 0x4;       // in the UI manager: number of live windows
inline constexpr int kMgrItems = 0x8;       // in the UI manager: CXWnd** to them
// Flags and mode the inventory window is built with: position, fade and lock go to the ini.
inline constexpr unsigned kIniFlags = 0xfffffeff;
inline constexpr int kIniMode = 2;
} // namespace wnd

// The SIDL manager (the object the UI loader is called on).
namespace sidlmgr {
inline constexpr int kLoaderFailed = 0x260;   // byte the loader sets when it fails ("Error reading XML.")
} // namespace sidlmgr

// A CXStr is a pointer to { ..., int length at +0x8, chars at +0x14 }.
namespace xstr {
inline constexpr int kLength = 0x8, kChars = 0x14;
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

// push -1; push <seh handler>; mov eax, fs:[0]
inline constexpr unsigned char kUiLoaderPrologue[] = { 0x6a, 0xff, 0x68, 0x5d, 0x3f, 0x63, 0x00, 0x64, 0xa1 };
// mov eax, <seh handler>   (then call __SEH_prolog)
inline constexpr unsigned char kBuildGameUiPrologue[] = { 0xb8, 0x72, 0x63, 0x62, 0x00 };
// push esi; push edi; push <"Resetting game UI.">
inline constexpr unsigned char kResetGameUiPrologue[] = { 0x56, 0x57, 0x68, 0xf8, 0x25, 0x64, 0x00 };

inline constexpr unsigned char kDragSizableTestBytes[] = { 0x8b, 0x57, 0x64, 0xf6, 0xc6, 0x02, 0x75, 0x59 };

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

} // namespace game
