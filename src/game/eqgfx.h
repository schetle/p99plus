#pragma once
// Everything we know about EQGraphicsDX9.dll (the build that ships with Titanium, image base
// 0x10000000): the engine camera class and the functions we hook.
#include <cstdint>

namespace gfx {

inline constexpr uintptr_t kImageBase = 0x10000000;
inline constexpr uintptr_t kBuilder   = 0x10004140;   // projection + view builder (thiscall)
inline constexpr uintptr_t kSetPos    = 0x10004760;   // camera SetPosition(const float pos[3], bool force), vtable +0x34
inline constexpr uintptr_t kSetAng    = 0x100047B0;   // camera SetOrientation(const float ang[3]), vtable +0x38

// push ebp; mov ebp,esp; and esp,-16; sub esp,0x248
inline constexpr unsigned char kBuilderPrologue[] = { 0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x81, 0xEC, 0x48, 0x02, 0x00, 0x00 };
// mov al,[esp+8]; test al,al; push esi; mov esi,[esp+8]; push edi; mov edi,ecx
inline constexpr unsigned char kSetPosPrologue[] = { 0x8A, 0x44, 0x24, 0x08, 0x84, 0xC0, 0x56, 0x8B, 0x74, 0x24, 0x08, 0x57, 0x8B, 0xF9 };
// mov eax,[esp+4]; mov edx,[eax]; mov [ecx+0xB0],edx
inline constexpr unsigned char kSetAngPrologue[] = { 0x8B, 0x44, 0x24, 0x04, 0x8B, 0x10, 0x89, 0x91, 0xB0, 0x00, 0x00, 0x00 };

// Fields of the camera object (the `this` the functions above receive).
inline float& HalfFov(void* cam) { return *(float*)((char*)cam + 0x04); }   // EQ angle units (512 per circle); 45 = 31.6 deg
inline float& Aspect(void* cam)  { return *(float*)((char*)cam + 0x08); }   // from the game's resolution
inline float* Angles(void* cam)  { return (float*)((char*)cam + 0xB0); }    // heading, pitch, roll
inline float* Eye(void* cam)     { return (float*)((char*)cam + 0xBC); }    // x, y, z

} // namespace gfx
