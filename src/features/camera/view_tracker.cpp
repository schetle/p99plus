// Remembers the view and projection matrices the game last set through Direct3D. The engine
// hooks use them to learn which way the camera faces; the fallback offsets them directly, and
// ProjectToScreen uses the world camera's to place things on screen.
#include "features/camera/internal.h"
#include "game/eqgame.h"
#include "util/mem.h"

namespace camera::view {

namespace {

D3DMATRIX s_view = {}, s_proj = {};
bool  s_haveView = false, s_haveProj = false;
float s_projSign = 1.0f;       // +1 left-handed (_34 = 1), -1 right-handed
bool  s_engine = false;

// The world camera's matrices: set by the engine builder for the main camera. Without a projection
// from the builder, the last perspective projection set at all.
D3DMATRIX s_worldView = {}, s_worldProj = {}, s_lastPersp = {};
bool s_haveWorldView = false, s_haveWorldProj = false, s_haveLastPersp = false;
bool s_inBuilder = false;     // inside the builder for the main camera

bool IsPerspective(const D3DMATRIX& m)
{
    return m._44 == 0.0f && m._34 != 0.0f && m._11 != 0.0f && m._22 != 0.0f;
}

} // namespace

void NoteView(const D3DMATRIX& m)
{
    s_view = m;
    s_haveView = true;
    if (s_inBuilder) { s_worldView = m; s_haveWorldView = true; }
}

void NoteProjection(const D3DMATRIX& m)
{
    s_proj = m;
    s_haveProj = true;
    if (IsPerspective(m)) {
        s_projSign = m._34 < 0 ? -1.0f : 1.0f;
        s_lastPersp = m;
        s_haveLastPersp = true;
    }
    if (s_inBuilder) { s_worldProj = m; s_haveWorldProj = true; }
}

void BeginBuilder(bool main) { s_inBuilder = main; }
void EndBuilder() { s_inBuilder = false; }

const D3DMATRIX* View() { return s_haveView ? &s_view : nullptr; }
const D3DMATRIX* Projection() { return s_haveProj ? &s_proj : nullptr; }

bool Axes(math::Vec3& right, math::Vec3& forward)
{
    if (!s_haveView) return false;
    // Camera axes in world space are the columns of the view's 3x3 rotation part.
    right   = { s_view._11, s_view._21, s_view._31 };
    forward = { s_view._13 * s_projSign, s_view._23 * s_projSign, s_view._33 * s_projSign };
    return true;
}

void SetEngineActive() { s_engine = true; }
bool EngineActive() { return s_engine; }

} // namespace camera::view


namespace camera {

bool ProjectToScreen(math::Vec3 p, float& x, float& y)
{
    using namespace view;
    const D3DMATRIX* V = s_haveWorldView ? &s_worldView : (s_haveView ? &s_view : nullptr);
    const D3DMATRIX* P = s_haveWorldProj ? &s_worldProj : (s_haveLastPersp ? &s_lastPersp : nullptr);
    if (!V || !P) return false;
    // Row vector times V, then P (Direct3D convention).
    float v[4];
    for (int c = 0; c < 4; ++c)
        v[c] = p.x * V->m[0][c] + p.y * V->m[1][c] + p.z * V->m[2][c] + V->m[3][c];
    float q[4];
    for (int c = 0; c < 4; ++c)
        q[c] = v[0] * P->m[0][c] + v[1] * P->m[1][c] + v[2] * P->m[2][c] + v[3] * P->m[3][c];
    if (q[3] < 0.01f) return false;
    const float nx = q[0] / q[3], ny = q[1] / q[3];
    const float left = (float)mem::Read<int>(game::addr::kScreenLeft), top = (float)mem::Read<int>(game::addr::kScreenTop);
    const float right = (float)mem::Read<int>(game::addr::kScreenRight), bottom = (float)mem::Read<int>(game::addr::kScreenBottom);
    if (right <= left || bottom <= top) return false;
    x = left + (nx + 1.0f) * 0.5f * (right - left);
    y = top + (1.0f - ny) * 0.5f * (bottom - top);
    return true;
}

} // namespace camera
