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

// Name labels. Each frame an actor's render calls kLabelDraw (thiscall, this = the label object
// at [actor + 0x1a0], arg = the actor, ret 4). It skips labels more than 150 units away, sizes
// the on-screen box from the distance times kLabelScale (a float only kLabelDraw reads), clamps it
// to about a seventh of the screen and hands it to kLabelSetRect (thiscall on the label's text
// sprite at label + 4: font id (always 10, a 40 px Arial texture), const int rect[4], ARGB,
// float; ret 0x10), the one caller of that function, which stores the rect at sprite + 0xc.
inline constexpr uintptr_t kLabelDraw    = 0x1004eb40;
inline constexpr uintptr_t kLabelSetRect = 0x100035e0;
inline constexpr uintptr_t kLabelScale   = 0x101caec4;
inline constexpr float     kLabelScaleDefault = 0.0081249997f;
inline constexpr int       kLabelSprite  = 0x4;        // the text sprite inside the label object
// sub esp,0x40; mov eax,[abs]   (the address after a1 is relocated, so only this much is checked)
inline constexpr unsigned char kLabelDrawPrologue[] = { 0x83, 0xEC, 0x40, 0xA1 };
// mov eax,[ecx+8]; mov edx,[esp+8]; push esi; mov esi,[esp+8]
inline constexpr unsigned char kLabelSetRectPrologue[] = { 0x8B, 0x41, 0x08, 0x8B, 0x54, 0x24, 0x08, 0x56, 0x8B, 0x74, 0x24, 0x08 };

// The engine's 2D queue (text sprites such as the labels, set up through kLabelSetRect) is drawn by
// kQueueFlush (thiscall on the queue, no arguments), called a couple of times per frame. It turns
// fog, lighting and z-writes off and alpha blending on, but leaves the z-test as the world render
// left it: that is why labels hide behind walls. Their depth is the post-projection z
// Q - Q * zn / w, from the camera [[kCameraHolder] + kCamera]: zn at +0xc, Q (= zf / (zf - zn)) at +0x10.
inline constexpr uintptr_t kQueueFlush   = 0x10001120;
inline constexpr uintptr_t kCameraHolder = 0x102326e8;
inline constexpr int       kCamera       = 0x30;
inline constexpr int       kCameraNear   = 0xc;
inline constexpr int       kCameraDepthQ = 0x10;
inline constexpr unsigned char kQueueFlushPrologue[] = { 0x83, 0xEC, 0x1C, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x08, 0x57, 0x33, 0xFF };

// Which way the camera angles turn the view, fixed for this build: +heading turns the forward
// vector counter-clockwise about +Z, +pitch raises it (measured from the view matrix).
inline constexpr int kHeadingSign = 1;
inline constexpr int kPitchSign   = 1;

} // namespace gfx
