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
} // namespace addr

// Field offsets inside the spawn struct.
namespace spawn {
inline constexpr int kX = 0x30, kY = 0x34, kZ = 0x2C;          // found in-game by matching the first-person eye
inline constexpr int kHeading = 0x4c, kTurnSpeed = 0x58, kPitch = 0x5c;
} // namespace spawn

// push ebp; mov ebp,esp; sub esp,0xC
inline constexpr unsigned char kMouselookPrologue[] = { 0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c };

// One 60 FPS frame of the game's turn integration: heading += speed * 0.02 * ms.
inline constexpr float kTurnPerSpeed = 0.02f * (1000.0f / 60.0f);

void Check();            // once: is this the expected eqgame.exe? (safe to call every frame)
bool Available();        // yes

int  CameraMode();       // 0 = first person, >0 other modes, -1 unknown
bool IsFirstPerson();

uintptr_t PlayerBase();                       // controlled spawn struct, or 0
bool      PlayerPos(math::Vec3& pos);         // false when there is no player
float     PlayerHeading();                    // EQ angle units, 0 when there is no player

// How far right-click mouselook turns per mouse count, in radians (yaw at the 60 FPS rate,
// which is what frame-rate independent mouselook gives), and whether the game inverts Y.
bool MouselookRates(float& yawRad, float& pitchRad, bool& invert);

} // namespace game
