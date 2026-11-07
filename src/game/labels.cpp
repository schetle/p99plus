#include "game/labels.h"
#include "core/log.h"
#include "game/eqgfx.h"
#include "util/hook.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <iterator>
#include <unordered_map>
#include <vector>

namespace game::labels {

namespace {

using LabelDraw_t = int (__fastcall*)(void* label, void* edx, void* actor);
using SetRect_t   = int (__fastcall*)(void* sprite, void* edx, int font, const int* rect, uint32_t argb, float f);
using Flush_t     = void (__fastcall*)(void* queue, void* edx);

LabelDraw_t oLabelDraw = nullptr;
SetRect_t   oSetRect = nullptr;
Flush_t     oFlush = nullptr;

Policy   s_policy = nullptr;
float*   s_scale = nullptr;       // the engine's kLabelScale, made writable; null = sizes can't be changed
float    s_default = 0.0f;        // its value as the engine has it
bool     s_tried = false;
bool     s_ok = false;
bool     s_tracked = false;       // kLabelSetRect is hooked: we know which labels were drawn
unsigned s_frame = 1;
unsigned s_polls = 0;
std::unordered_map<uintptr_t, unsigned> s_drawn;   // label -> the frame it was last drawn in
bool     s_queued = false;        // labels were queued since the last flush
bool     s_flushHooked = false;
std::vector<AfterFn> s_after;
uintptr_t s_cameraHolder = 0;     // gfx::kCameraHolder in the loaded module

constexpr unsigned kPruneEvery = 600;   // frames; labels not drawn for this long are forgotten

int __fastcall Hk_LabelDraw(void* label, void* edx, void* actor)
{
    hook::Busy busy;
    const float k = s_policy ? s_policy((uintptr_t)actor) : 1.0f;
    if (k <= 0.0f) return 0;                         // hidden (the caller ignores the result)
    if (k == 1.0f || !s_scale) return oLabelDraw(label, edx, actor);
    *s_scale = s_default * k;                        // only this function reads it
    const int r = oLabelDraw(label, edx, actor);
    *s_scale = s_default;
    return r;
}

// Called only from kLabelDraw, when it has placed the label on screen.
int __fastcall Hk_SetRect(void* sprite, void* edx, int font, const int* rect, uint32_t argb, float f)
{
    hook::Busy busy;
    s_drawn[(uintptr_t)sprite - gfx::kLabelSprite] = s_frame;
    s_queued = true;
    return oSetRect(sprite, edx, font, rect, argb, f);
}

// The flush that drew labels is followed by what wants to be drawn with them.
void __fastcall Hk_Flush(void* queue, void* edx)
{
    hook::Busy busy;
    oFlush(queue, edx);
    if (!s_queued) return;
    s_queued = false;
    for (AfterFn fn : s_after) fn();
}

void Install()
{
    HMODULE m = GetModuleHandleA("EQGraphicsDX9.dll");
    if (!m) return;                                  // not loaded yet; try again later
    s_tried = true;

    auto base = (const unsigned char*)m;
    auto nt = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
    auto at = [&](uintptr_t va) { return (void*)(base + (va - gfx::kImageBase)); };
    if (nt->OptionalHeader.ImageBase != gfx::kImageBase) {
        Log("labels: EQGraphicsDX9.dll is not the expected build; name labels unchanged");
        return;
    }

    if (hook::Install("label draw", at(gfx::kLabelDraw), (void*)&Hk_LabelDraw, (void**)&oLabelDraw,
                      gfx::kLabelDrawPrologue) != hook::Result::Ok)
        return;
    s_ok = true;

    // The size factor sits in read-only data; it stays writable from here on. Every write is put
    // back right after the label is drawn, so the engine never keeps one of ours.
    float* scale = (float*)at(gfx::kLabelScale);
    DWORD old;
    if (std::fabs(*scale - gfx::kLabelScaleDefault) > 1e-7f)
        Log("labels: unexpected label size factor %g; label sizes unavailable", *scale);
    else if (!VirtualProtect(scale, sizeof(float), PAGE_EXECUTE_READWRITE, &old))
        Log("labels: can't make the label size factor writable; label sizes unavailable");
    else {
        s_default = *scale;
        s_scale = scale;
    }

    s_tracked = hook::Install("label rect", at(gfx::kLabelSetRect), (void*)&Hk_SetRect, (void**)&oSetRect,
                              gfx::kLabelSetRectPrologue) == hook::Result::Ok;
    if (!s_tracked) Log("labels: can't tell which labels are drawn; their rects may be stale");

    s_cameraHolder = (uintptr_t)at(gfx::kCameraHolder);
    s_flushHooked = s_tracked && hook::Install("label flush", at(gfx::kQueueFlush), (void*)&Hk_Flush, (void**)&oFlush,
                                               gfx::kQueueFlushPrologue) == hook::Result::Ok;
}

} // namespace

void Init()
{
    if (s_tried || s_polls++ % 30 != 0) return;
    Install();
}

bool Ready() { return s_ok; }

void Tick()
{
    ++s_frame;
    if (s_frame % kPruneEvery == 0)
        for (auto it = s_drawn.begin(); it != s_drawn.end();)
            it = (s_frame - it->second > kPruneEvery) ? s_drawn.erase(it) : std::next(it);
}

void SetPolicy(Policy fn) { s_policy = fn; }

void AfterLabels(AfterFn fn)
{
    if (std::find(s_after.begin(), s_after.end(), fn) == s_after.end()) s_after.push_back(fn);
}

bool AfterLabelsReady() { return s_flushHooked; }

float Nearer(float depth, float units)
{
    const uintptr_t holder = s_cameraHolder ? *(const uintptr_t*)s_cameraHolder : 0;
    const uintptr_t cam = holder > 0x10000 ? *(const uintptr_t*)(holder + gfx::kCamera) : 0;
    if (cam <= 0x10000) return depth;
    const float zn = *(const float*)(cam + gfx::kCameraNear);
    const float q = *(const float*)(cam + gfx::kCameraDepthQ);
    if (!(zn > 0.0f && q > 1.0f && depth > 0.0f && depth < 1.0f)) return depth;
    // depth = q - q * zn / w  <=>  w = q * zn / (q - depth)
    const float w = std::max(q * zn / (q - depth) - units, zn);
    return q - q * zn / w;
}

bool DrawnRecently(uintptr_t label)
{
    if (!s_tracked) return true;
    auto it = s_drawn.find(label);
    return it != s_drawn.end() && s_frame - it->second <= 1;
}

} // namespace game::labels
