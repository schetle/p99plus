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
} // namespace addr

// Field offsets inside the spawn struct.
namespace spawn {
inline constexpr int kX = 0x30, kY = 0x34, kZ = 0x2C;          // found in-game by matching the first-person eye
inline constexpr int kCamZ = 0x38;                             // the z the camera controllers build on (eye height goes on top)
inline constexpr int kHeading = 0x4c, kTurnSpeed = 0x58, kPitch = 0x5c;
} // namespace spawn

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
