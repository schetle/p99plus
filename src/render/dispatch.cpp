// Direct3D calls handed over by the host's device hooks: Present runs the features' frame work,
// draws the overlay and applies the frame cap; the rest go to the camera fallback.
#include "render/dispatch.h"
#include "core/feature.h"
#include "features/camera/d3d_fallback.h"
#include "features/limiter/limiter.h"
#include "overlay/overlay.h"

namespace render {

namespace {
bool s_overlay = false;   // true while our overlay renders: its own calls pass through untouched
} // namespace

void OnPresent(IDirect3DDevice9* dev)
{
    feature::OnFrame();
    s_overlay = true;
    overlay::OnPresent(dev);
    s_overlay = false;
    limiter::Wait();
}

void PreReset()  { overlay::PreReset(); }
void PostReset() { overlay::PostReset(); }

const D3DMATRIX* OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    return s_overlay ? m : camera::fallback::OnSetTransform(state, m);
}

const D3DMATRIX* ViewForBeginScene()
{
    return s_overlay ? nullptr : camera::fallback::ViewForBeginScene();
}

void OnSetVertexShader(IDirect3DVertexShader9* vs)
{
    if (!s_overlay) camera::fallback::OnSetVertexShader(vs);
}

const float* OnSetVSConstants(UINT reg, const float* data, UINT count)
{
    return s_overlay ? data : camera::fallback::OnSetVertexShaderConstants(reg, data, count);
}

} // namespace render
