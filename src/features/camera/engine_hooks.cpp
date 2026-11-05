// Hooks into the engine camera class in EQGraphicsDX9.dll (see game/eqgfx.h):
//   - the projection/view builder: runs every frame; we refresh the FOV, the offset eye and the
//     orbit angles stored in the camera object just before it builds the matrices
//   - SetPosition: the game sets the eye; we store the offset eye instead
//   - SetOrientation: the game sets the angles; we store them with the orbit added
// The camera class stores its eye at +0xBC. GetPosition (+0x3C) reads it back, and the
// mouse-pick ray (+0x64) and eqgame.exe's name tag / effect placement read it too, which is why
// the offset is written into the object itself. Installed once the DLL is loaded, and only if
// the image base and function prologues match the expected build.
#include "features/camera/internal.h"
#include "core/log.h"
#include "game/eqgfx.h"
#include "util/hook.h"
#include <cstdio>
#include <cstring>

namespace camera {

namespace {

using Builder_t = void (__fastcall*)(void* self, void* edx);
using SetPos_t  = int  (__fastcall*)(void* self, void* edx, const float* pos, int force);
using SetAng_t  = void (__fastcall*)(void* self, void* edx, const float* ang);

Builder_t oBuilder = nullptr;
SetPos_t  oSetPos = nullptr;
SetAng_t  oSetAng = nullptr;

bool s_tried = false;
char s_problem[160] = "";
unsigned s_frame = 0;

void __fastcall Hk_Builder(void* self, void* edx)
{
    ApplyFov(self);

    // Keep the stored eye at real eye + orbit + offset, and the angles at real angles + orbit.
    float* eye = gfx::Eye(self);
    float native[3], out[3];
    EyeOffset(self, eye, native, out);
    memcpy(eye, out, sizeof(out));
    EyeCommit(self, native, out);

    float* ang = gfx::Angles(self);
    float angNative[3], angOut[3];
    orbit::AngOffset(self, ang, angNative, angOut);
    if (memcmp(ang, angOut, sizeof(angOut)) != 0) {
        if (oSetAng) oSetAng(self, nullptr, angOut);     // also rebuilds the pick-ray basis
        else memcpy(ang, angOut, sizeof(angOut));
    }
    orbit::AngCommit(self, angNative, angOut);

    oBuilder(self, edx);
}

// The game sets the camera angles: store real angles + orbit instead.
void __fastcall Hk_SetAng(void* self, void* edx, const float* ang)
{
    if (!ang) { oSetAng(self, edx, ang); return; }
    float native[3], out[3];
    orbit::AngOffset(self, ang, native, out);
    oSetAng(self, edx, out);
    orbit::AngCommit(self, native, out);
}

// The game sets the camera position: store real eye + offset instead.
int __fastcall Hk_SetPos(void* self, void* edx, const float* pos, int force)
{
    if (!pos) return oSetPos(self, edx, pos, force);
    float native[3], out[3];
    EyeOffset(self, pos, native, out);
    int r = oSetPos(self, edx, out, force);
    if (r != 0 && memcmp(out, native, sizeof(out)) != 0) {
        memcpy(out, native, sizeof(out));                // offset spot rejected: use the real eye
        r = oSetPos(self, edx, out, force);
    }
    if (r == 0) EyeCommit(self, native, out);
    return r;
}

void Install()
{
    HMODULE m = GetModuleHandleA("EQGraphicsDX9.dll");
    if (!m) return;                                      // not loaded yet; try again later
    s_tried = true;                                      // from here on: success or a final failure

    auto base = (const unsigned char*)m;
    auto nt = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
    auto at = [&](uintptr_t va) { return (void*)(base + (va - gfx::kImageBase)); };

    if (nt->OptionalHeader.ImageBase != gfx::kImageBase) {
        Log("camera: EQGraphicsDX9.dll is not the expected build; FOV control unavailable");
        snprintf(s_problem, sizeof(s_problem), "FOV unavailable (unexpected EQGraphicsDX9.dll)");
        return;
    }
    switch (hook::Install("camera builder", at(gfx::kBuilder), (void*)&Hk_Builder, (void**)&oBuilder, gfx::kBuilderPrologue)) {
    case hook::Result::Ok: break;
    case hook::Result::Mismatch:
        snprintf(s_problem, sizeof(s_problem), "FOV unavailable (unexpected EQGraphicsDX9.dll)");
        return;
    case hook::Result::Failed:
        snprintf(s_problem, sizeof(s_problem), "FOV unavailable (hook failed)");
        return;
    }

    if (hook::Install("camera SetPosition", at(gfx::kSetPos), (void*)&Hk_SetPos, (void**)&oSetPos, gfx::kSetPosPrologue) != hook::Result::Ok)
        Log("camera: offset is only refreshed per frame");

    if (hook::Install("camera SetOrientation", at(gfx::kSetAng), (void*)&Hk_SetAng, (void**)&oSetAng, gfx::kSetAngPrologue) != hook::Result::Ok) {
        oSetAng = nullptr;
        Log("camera: orbit is only refreshed per frame");
    }
}

} // namespace

const char* FovProblem() { return s_problem[0] ? s_problem : nullptr; }

void EngineHooksOnFrame()
{
    ++s_frame;
    if (!s_tried && s_frame % 30 == 1) Install();
}

} // namespace camera
