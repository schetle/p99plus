// Direct3D calls handed over by the host's device hooks: Present runs the features' frame work,
// draws the overlay and applies the frame cap; the rest go to the camera fallback.
#include "render/dispatch.h"
#include "core/feature.h"
#include "features/camera/d3d_fallback.h"
#include "features/limiter/limiter.h"
#include "overlay/overlay.h"
#include "render/icons.h"

namespace render {

namespace {
int s_own = 0;                       // OwnCalls scopes open: our own calls pass through untouched
IDirect3DDevice9* s_dev = nullptr;
} // namespace

OwnCalls::OwnCalls() { ++s_own; }
OwnCalls::~OwnCalls() { --s_own; }

IDirect3DDevice9* Device() { return s_dev; }

void OnPresent(IDirect3DDevice9* dev)
{
    s_dev = dev;
    feature::OnFrame();
    {
        OwnCalls own;
        overlay::OnPresent(dev);
    }
    limiter::Wait();
}

void PreReset()  { overlay::PreReset(); icons::ReleaseDeviceState(); }
void PostReset() { overlay::PostReset(); }
void Shutdown()  { icons::Release(); }

const D3DMATRIX* OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m)
{
    return s_own ? m : camera::fallback::OnSetTransform(state, m);
}

const D3DMATRIX* ViewForBeginScene()
{
    return s_own ? nullptr : camera::fallback::ViewForBeginScene();
}

void OnSetVertexShader(IDirect3DVertexShader9* vs)
{
    if (!s_own) camera::fallback::OnSetVertexShader(vs);
}

const float* OnSetVSConstants(UINT reg, const float* data, UINT count)
{
    return s_own ? data : camera::fallback::OnSetVertexShaderConstants(reg, data, count);
}

} // namespace render
