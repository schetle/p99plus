#include "features/camera/d3d_fallback.h"
#include "features/camera/internal.h"
#include "game/eqgame.h"
#include <cstring>

namespace camera::fallback {

namespace {

using math::Vec3;

// Last view matrix we produced, so a game GetTransform -> SetTransform round trip
// doesn't get adjusted twice.
D3DMATRIX s_viewOut = {};
bool      s_viewOutValid = false;

// Current view offset, for shader constants that hold the game's View*Proj: VP' = T(-d) * VP.
Vec3 s_constOffset;

// View*Proj rewriting in shader constants. The game uploads transpose(View*Proj) as 4
// consecutive vertex shader registers (c8 for the main object shader, c0 for others). We find
// those blocks by exact match against the game's current View*Proj, remember (shader, reg) so
// later uploads still get fixed if the game uploads before SetTransform, and rewrite them.
struct Slot { void* vs; UINT reg; };
Slot      s_slots[32];
int       s_numSlots = 0;
void*     s_vs = nullptr;           // current vertex shader (learned slots are per shader)
D3DMATRIX s_vpCur = {};
bool      s_vpDirty = true;
float     s_constBuf[256 * 4];

D3DMATRIX Mul(const D3DMATRIX& a, const D3DMATRIX& b)
{
    D3DMATRIX r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
    return r;
}

bool Near(float a, float b)
{
    const float m = std::fmax(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= 1e-3f * (m > 1.0f ? m : 1.0f);
}

// 0 = no match, 1 = matches M as stored row-major, 2 = matches transpose(M)
int MatchBlock(const float* f, const D3DMATRIX& M)
{
    bool plain = true, trans = true;
    for (int r = 0; r < 4 && (plain || trans); ++r)
        for (int c = 0; c < 4; ++c) {
            if (plain && !Near(f[r * 4 + c], M.m[r][c])) plain = false;
            if (trans && !Near(f[r * 4 + c], M.m[c][r])) trans = false;
        }
    return plain ? 1 : trans ? 2 : 0;
}

bool LearnedSlot(void* vs, UINT reg)
{
    for (int i = 0; i < s_numSlots; ++i)
        if (s_slots[i].vs == vs && s_slots[i].reg == reg) return true;
    return false;
}

void LearnSlot(void* vs, UINT reg)
{
    if (LearnedSlot(vs, reg) || s_numSlots >= 32) return;
    s_slots[s_numSlots++] = { vs, reg };
}

// transpose(VP): its last register row is VP column 3 = view-forward (x, y, z, w) for a
// standard perspective; the xyz part is a unit vector. Cheap sanity check for learned slots.
bool LooksLikeVPT(const float* f)
{
    const float l = f[12] * f[12] + f[13] * f[13] + f[14] * f[14];
    return l > 0.98f && l < 1.02f;
}

// f holds transpose(M) in 4 registers. Apply M' = T(-d) * M.
void CorrectVPT(float* f, Vec3 d)
{
    D3DMATRIX M;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) M.m[r][c] = f[c * 4 + r];
    for (int c = 0; c < 4; ++c) M.m[3][c] -= d.x * M.m[0][c] + d.y * M.m[1][c] + d.z * M.m[2][c];
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) f[c * 4 + r] = M.m[r][c];
}

// Offsets the view matrix `in` (the one the game set), or returns it unchanged.
const D3DMATRIX* AdjustView(const D3DMATRIX* in, bool fromGame)
{
    if (fromGame) {
        if (s_viewOutValid && memcmp(in, &s_viewOut, sizeof(D3DMATRIX)) == 0) return in;   // already ours
        view::NoteView(*in);
    }
    s_constOffset = {};
    // The engine camera's eye already carries the offset (see EyeOffset); nothing to do here.
    if (view::EngineActive()) return in;

    Vec3 d;
    if (!ComputeEyeOffset(game::IsFirstPerson(), 1.0f, d)) return in;
    s_constOffset = d;

    // V = [R 0; t 1], t = -eye*R. Moving the eye by d: t' = t - d*R.
    const D3DMATRIX& m = *in;
    s_viewOut = m;
    for (int j = 0; j < 3; ++j)
        s_viewOut.m[3][j] -= d.x * m.m[0][j] + d.y * m.m[1][j] + d.z * m.m[2][j];
    s_viewOutValid = true;
    return &s_viewOut;
}

} // namespace

const D3DMATRIX* OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    if (!m) return m;
    if (state == D3DTS_VIEW) {
        s_vpDirty = true;
        return AdjustView(m, true);
    }
    if (state == D3DTS_PROJECTION) {
        s_vpDirty = true;
        view::NoteProjection(*m);
    }
    return m;
}

const D3DMATRIX* ViewForBeginScene()
{
    const D3DMATRIX* v = view::View();
    return v ? AdjustView(v, false) : nullptr;
}

void OnSetVertexShader(void* vs) { s_vs = vs; }

const float* OnSetVertexShaderConstants(UINT reg, const float* data, UINT count)
{
    if (!data || count < 4 || count > 256) return data;

    const D3DMATRIX* V = view::View();
    const D3DMATRIX* P = view::Projection();
    if (V && P && s_vpDirty) { s_vpCur = Mul(*V, *P); s_vpDirty = false; }

    const bool active = s_constOffset.x != 0 || s_constOffset.y != 0 || s_constOffset.z != 0;
    const float* out = data;

    for (UINT k = 0; k + 4 <= count; ++k) {
        const float* f = data + k * 4;
        const bool matched = V && P && MatchBlock(f, s_vpCur) == 2;
        if (matched) LearnSlot(s_vs, reg + k);
        const bool learned = !matched && LearnedSlot(s_vs, reg + k) && LooksLikeVPT(f);
        if (!active || (!matched && !learned)) continue;

        if (out == data) { memcpy(s_constBuf, data, count * 4 * sizeof(float)); out = s_constBuf; }
        CorrectVPT(s_constBuf + k * 4, s_constOffset);
        k += 3;   // skip the rest of this block
    }
    return out;
}

} // namespace camera::fallback
