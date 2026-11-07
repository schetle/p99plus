#include "render/icons.h"
#include "core/app.h"
#include "core/log.h"
#include "render/dispatch.h"
#include <algorithm>
#include <d3d9.h>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace render::icons {

namespace {

constexpr int kSheets = 7;
constexpr int kSheetSize = 256;
constexpr int kCell = 40;
constexpr int kPerRow = kSheetSize / kCell;          // 6
constexpr int kPerSheet = kPerRow * kPerRow;         // 36

IDirect3DTexture9* s_sheets[kSheets] = {};
IDirect3DStateBlock9* s_state = nullptr;
bool s_loaded = false;

// An uncompressed or RLE true-colour TGA (types 2 and 10, 24 or 32 bits) as top-down BGRA.
bool ReadTga(const std::string& path, std::vector<uint32_t>& out, int& w, int& h)
{
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    const std::vector<unsigned char> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 18) return false;
    const int type = d[2], bpp = d[16], desc = d[17];
    w = d[12] | d[13] << 8;
    h = d[14] | d[15] << 8;
    if ((type != 2 && type != 10) || (bpp != 24 && bpp != 32) || d[1] != 0 || w <= 0 || h <= 0) return false;
    const size_t bytes = bpp / 8, count = (size_t)w * h;
    size_t pos = 18 + d[0];
    std::vector<uint32_t> px;
    px.reserve(count);
    auto pixel = [&](size_t at) {
        const uint32_t a = bytes == 4 ? d[at + 3] : 0xffu;
        return a << 24 | (uint32_t)d[at + 2] << 16 | (uint32_t)d[at + 1] << 8 | d[at];
    };
    while (px.size() < count) {
        if (type == 2) {
            if (pos + bytes > d.size()) return false;
            px.push_back(pixel(pos));
            pos += bytes;
            continue;
        }
        if (pos >= d.size()) return false;
        const int head = d[pos++];
        const size_t run = (size_t)(head & 0x7f) + 1;
        if (head & 0x80) {
            if (pos + bytes > d.size()) return false;
            const uint32_t p = pixel(pos);
            pos += bytes;
            for (size_t i = 0; i < run && px.size() < count; ++i) px.push_back(p);
        } else {
            for (size_t i = 0; i < run && px.size() < count; ++i, pos += bytes) {
                if (pos + bytes > d.size()) return false;
                px.push_back(pixel(pos));
            }
        }
    }
    out.resize(count);
    const bool topDown = desc & 0x20;
    for (int y = 0; y < h; ++y)
        std::copy_n(&px[(size_t)(topDown ? y : h - 1 - y) * w], w, &out[(size_t)y * w]);
    return true;
}

IDirect3DTexture9* MakeTexture(IDirect3DDevice9* dev, const std::vector<uint32_t>& px, int w, int h)
{
    IDirect3DTexture9* tex = nullptr;
    if (FAILED(dev->CreateTexture(w, h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, nullptr))) return nullptr;
    D3DLOCKED_RECT lr;
    if (FAILED(tex->LockRect(0, &lr, nullptr, 0))) { tex->Release(); return nullptr; }
    for (int y = 0; y < h; ++y)
        std::copy_n(&px[(size_t)y * w], w, (uint32_t*)((unsigned char*)lr.pBits + (size_t)y * lr.Pitch));
    tex->UnlockRect(0);
    return tex;
}

} // namespace

bool Load(const std::string& skinDir)
{
    if (s_loaded) return true;
    IDirect3DDevice9* dev = Device();
    if (!dev) return false;
    int ok = 0;
    for (int i = 0; i < kSheets; ++i) {
        const std::string name = "spells0" + std::to_string(i + 1) + ".tga";
        std::vector<uint32_t> px;
        int w = 0, h = 0;
        if ((skinDir.empty() || !ReadTga(skinDir + name, px, w, h)) &&
            !ReadTga(g_gameDir + "UIFiles\\Default\\" + name, px, w, h))
            continue;
        if (w != kSheetSize || h != kSheetSize) { Log("icons: %s is %dx%d, not %dx%d; skipped", name.c_str(), w, h, kSheetSize, kSheetSize); continue; }
        s_sheets[i] = MakeTexture(dev, px, w, h);
        ok += s_sheets[i] != nullptr;
    }
    s_loaded = true;
    Log("icons: %d of %d spell icon sheets loaded", ok, kSheets);
    return ok > 0;
}

void Draw(const Icon* icons, int count)
{
    IDirect3DDevice9* dev = Device();
    if (!dev || !s_loaded || count <= 0) return;

    struct Vertex { float x, y, z, rhw; uint32_t color; float u, v; };
    constexpr DWORD kFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
    std::vector<Vertex> verts[kSheets];
    for (int i = 0; i < count; ++i) {
        const Icon& c = icons[i];
        const int sheet = c.cell / kPerSheet;
        if (c.cell < 0 || sheet >= kSheets || !s_sheets[sheet] || c.size <= 0.0f) continue;
        if (!(c.depth > 0.0f && c.depth < 1.0f)) continue;   // behind the camera
        const int at = c.cell % kPerSheet;
        const float u0 = (float)(at % kPerRow * kCell) / kSheetSize, v0 = (float)(at / kPerRow * kCell) / kSheetSize;
        const float u1 = u0 + (float)kCell / kSheetSize, v1 = v0 + (float)kCell / kSheetSize;
        const float l = c.x - 0.5f, t = c.y - 0.5f, r = l + c.size, b = t + c.size;
        const Vertex q[6] = { { l, t, c.depth, 1.0f, c.argb, u0, v0 }, { r, t, c.depth, 1.0f, c.argb, u1, v0 },
                              { l, b, c.depth, 1.0f, c.argb, u0, v1 }, { r, t, c.depth, 1.0f, c.argb, u1, v0 },
                              { r, b, c.depth, 1.0f, c.argb, u1, v1 }, { l, b, c.depth, 1.0f, c.argb, u0, v1 } };
        verts[sheet].insert(verts[sheet].end(), std::begin(q), std::end(q));
    }

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
    dev->SetFVF(kFvf);
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
    dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, FALSE);

    for (int s = 0; s < kSheets; ++s) {
        if (verts[s].empty()) continue;
        dev->SetTexture(0, s_sheets[s]);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, (UINT)verts[s].size() / 3, verts[s].data(), sizeof(Vertex));
    }
    s_state->Apply();
}

void ReleaseDeviceState()
{
    if (s_state) { s_state->Release(); s_state = nullptr; }
}

void Release()
{
    for (IDirect3DTexture9*& t : s_sheets)
        if (t) { t->Release(); t = nullptr; }
    if (s_state) { s_state->Release(); s_state = nullptr; }
    s_loaded = false;
}

} // namespace render::icons
