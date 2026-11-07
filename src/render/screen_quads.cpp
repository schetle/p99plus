#include "render/screen_quads.h"
#include "render/dispatch.h"
#include <algorithm>
#include <d3d9.h>
#include <iterator>

namespace render::quads {

namespace {

IDirect3DStateBlock9* s_state = nullptr;

// Half the size in each direction: each pixel the alpha-weighted average of a 2 x 2 block.
std::vector<uint32_t> HalfSize(const std::vector<uint32_t>& px, int w, int h)
{
    const int hw = std::max(w / 2, 1), hh = std::max(h / 2, 1);
    std::vector<uint32_t> out((size_t)hw * hh);
    for (int y = 0; y < hh; ++y)
        for (int x = 0; x < hw; ++x) {
            unsigned a = 0, r = 0, g = 0, b = 0;
            for (int k = 0; k < 4; ++k) {
                const uint32_t p = px[(size_t)std::min(y * 2 + k / 2, h - 1) * w + std::min(x * 2 + k % 2, w - 1)];
                const unsigned pa = p >> 24;
                a += pa;
                r += (p >> 16 & 0xff) * pa;
                g += (p >> 8 & 0xff) * pa;
                b += (p & 0xff) * pa;
            }
            out[(size_t)y * hw + x] = a ? (a / 4) << 24 | (r / a) << 16 | (g / a) << 8 | (b / a) : 0;
        }
    return out;
}

bool Upload(IDirect3DTexture9* tex, UINT level, const uint32_t* px, int w, int h)
{
    D3DLOCKED_RECT lr;
    if (FAILED(tex->LockRect(level, &lr, nullptr, 0))) return false;
    for (int y = 0; y < h; ++y)
        std::copy_n(px + (size_t)y * w, w, (uint32_t*)((unsigned char*)lr.pBits + (size_t)y * lr.Pitch));
    tex->UnlockRect(level);
    return true;
}

} // namespace

IDirect3DTexture9* MakeTexture(IDirect3DDevice9* dev, const uint32_t* px, int w, int h, bool mips)
{
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(dev->CreateTexture(w, h, mips ? 0 : 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr))) return nullptr;
    bool ok = Upload(tex, 0, px, w, h);
    std::vector<uint32_t> level(px, px + (size_t)w * h);
    for (UINT i = 1; ok && i < tex->GetLevelCount(); ++i) {
        level = HalfSize(level, w, h);
        w = std::max(w / 2, 1);
        h = std::max(h / 2, 1);
        ok = Upload(tex, i, level.data(), w, h);
    }
    if (!ok) { tex->Release(); return nullptr; }
    return tex;
}

void Batch::Add(IDirect3DTexture9* tex, float l, float t, float r, float b,
                float u0, float v0, float u1, float v1, float depth, uint32_t argb)
{
    if (!tex || !(depth > 0.0f && depth < 1.0f) || r <= l || b <= t) return;   // behind the camera, or empty
    l -= 0.5f; t -= 0.5f; r -= 0.5f; b -= 0.5f;   // pixel centres (D3D9 rasterisation rules)
    const Vertex q[6] = { { l, t, depth, 1.0f, argb, u0, v0 }, { r, t, depth, 1.0f, argb, u1, v0 },
                          { l, b, depth, 1.0f, argb, u0, v1 }, { r, t, depth, 1.0f, argb, u1, v0 },
                          { r, b, depth, 1.0f, argb, u1, v1 }, { l, b, depth, 1.0f, argb, u0, v1 } };
    auto g = std::find_if(m_groups.begin(), m_groups.end(), [&](const Group& x) { return x.tex == tex; });
    if (g == m_groups.end()) g = m_groups.insert(m_groups.end(), Group{ tex, {} });
    g->verts.insert(g->verts.end(), std::begin(q), std::end(q));
}

Batch::~Batch()
{
    IDirect3DDevice9* dev = Device();
    if (!dev || m_groups.empty()) return;

    OwnCalls own;   // these calls aren't the game's: keep them away from the camera fallback
    if (!s_state && FAILED(dev->CreateStateBlock(D3DSBT_ALL, &s_state))) { s_state = nullptr; return; }
    s_state->Capture();

    UINT width = 0, height = 0;
    if (IDirect3DSurface9* rt = nullptr; SUCCEEDED(dev->GetRenderTarget(0, &rt)) && rt) {
        D3DSURFACE_DESC d;
        if (SUCCEEDED(rt->GetDesc(&d))) { width = d.Width; height = d.Height; }
        rt->Release();
    }
    const D3DVIEWPORT9 vp = { 0, 0, width, height, 0.0f, 1.0f };
    dev->SetViewport(&vp);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xf);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_LIGHTING, FALSE);
    dev->SetRenderState(D3DRS_CLIPPING, TRUE);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    dev->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    dev->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
    dev->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    dev->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

    for (const Group& g : m_groups) {
        dev->SetTexture(0, g.tex);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)g.verts.size() / 3, g.verts.data(), sizeof(Vertex));
    }
    s_state->Apply();
}

void ReleaseDeviceState()
{
    if (s_state) { s_state->Release(); s_state = nullptr; }
}

void Release() { ReleaseDeviceState(); }

} // namespace render::quads
