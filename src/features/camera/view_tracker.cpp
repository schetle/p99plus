// Remembers the view and projection matrices the game last set through Direct3D. The engine
// hooks use them to learn which way the camera faces; the fallback offsets them directly.
#include "features/camera/internal.h"

namespace camera::view {

namespace {

D3DMATRIX s_view = {}, s_proj = {};
bool  s_haveView = false, s_haveProj = false;
float s_projSign = 1.0f;       // +1 left-handed (_34 = 1), -1 right-handed
bool  s_engine = false;

bool IsPerspective(const D3DMATRIX& m)
{
    return m._44 == 0.0f && m._34 != 0.0f && m._11 != 0.0f && m._22 != 0.0f;
}

} // namespace

void NoteView(const D3DMATRIX& m) { s_view = m; s_haveView = true; }

void NoteProjection(const D3DMATRIX& m)
{
    s_proj = m;
    s_haveProj = true;
    if (IsPerspective(m)) s_projSign = m._34 < 0 ? -1.0f : 1.0f;
}

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
