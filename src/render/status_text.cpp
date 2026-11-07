#include "render/status_text.h"
#include "core/log.h"
#include "render/dispatch.h"
#include "render/screen_quads.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <d3d9.h>
#include <vector>

namespace render::status_text {

namespace {

constexpr int kTexW = 512, kTexH = 512;
constexpr int kRowH = 64;           // one line per row
constexpr int kFontPx = 40;         // GDI cell height (the engine's label font is Arial 40)
constexpr int kTextTop = (kRowH - kFontPx) / 2;
constexpr int kPad = 6;             // room for the shadow left and right
constexpr int kShadow = 3;          // drop shadow offset, right and down, in texture pixels (just under
                                    // a screen pixel at the sizes the text is drawn at)
constexpr float kShadowDark = 0.9f; // its strength where the text is solid
static_assert(kMaxLines * kRowH <= kTexH, "rows must fit the texture");

struct Row { float u1; float aspect; };   // right edge of the text in the row, row width / height

IDirect3DTexture9* s_tex = nullptr;
Row  s_rows[kMaxLines] = {};
int  s_count = 0;
bool s_tried = false;

// Draws the lines white on black with GDI; returns each pixel's coverage (0..255), or false.
bool RenderCoverage(const char* const* lines, int count, std::vector<unsigned char>& cov)
{
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) return false;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biWidth = kTexW;
    bi.bmiHeader.biHeight = -kTexH;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HFONT font = CreateFontA(kFontPx, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
    bool ok = bmp && bits && font;
    if (ok) {
        HGDIOBJ oldBmp = SelectObject(dc, bmp);
        HGDIOBJ oldFont = SelectObject(dc, font);
        std::fill_n((uint32_t*)bits, (size_t)kTexW * kTexH, 0u);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        for (int i = 0; i < count; ++i) {
            const int len = (int)strlen(lines[i]);
            SIZE sz = {};
            GetTextExtentPoint32A(dc, lines[i], len, &sz);
            const int w = std::min((int)sz.cx, kTexW - 2 * kPad);
            s_rows[i] = { (float)(w + 2 * kPad) / kTexW, (float)(w + 2 * kPad) / kRowH };
            TextOutA(dc, kPad, i * kRowH + kTextTop, lines[i], len);
        }
        GdiFlush();
        cov.resize((size_t)kTexW * kTexH);
        const uint32_t* px = (const uint32_t*)bits;
        for (size_t i = 0; i < cov.size(); ++i)   // grey antialiasing: any channel will do, take the brightest
            cov[i] = (unsigned char)std::max({ px[i] & 0xff, px[i] >> 8 & 0xff, px[i] >> 16 & 0xff });
        SelectObject(dc, oldFont);
        SelectObject(dc, oldBmp);
    }
    if (font) DeleteObject(font);
    if (bmp) DeleteObject(bmp);
    DeleteDC(dc);
    return ok;
}

// White text over a hard dark drop shadow (the coverage moved by kShadow, not blurred: the
// antialiased glyph edges and the downscaling soften it enough), as straight (not premultiplied)
// BGRA. Only the first `rows` rows hold text.
std::vector<uint32_t> Shadowed(const std::vector<unsigned char>& cov, int rows)
{
    std::vector<uint32_t> out((size_t)kTexW * kTexH);
    for (int y = 0; y < rows * kRowH; ++y)
        for (int x = 0; x < kTexW; ++x) {
            const int sx = x - kShadow, sy = y - kShadow;
            const int under = sx >= 0 && sy >= 0 ? cov[(size_t)sy * kTexW + sx] : 0;
            const float c = cov[(size_t)y * kTexW + x] / 255.0f;
            const float sh = under / 255.0f * kShadowDark;
            const float a = c + sh * (1.0f - c);                    // text over shadow
            const unsigned v = a > 0.0f ? (unsigned)std::lround(255.0f * c / a) : 0;
            out[(size_t)y * kTexW + x] = (unsigned)std::lround(255.0f * a) << 24 | v << 16 | v << 8 | v;
        }
    return out;
}

} // namespace

bool Load(const char* const* lines, int count)
{
    if (s_tried) return s_tex != nullptr;
    IDirect3DDevice9* dev = Device();
    if (!dev) return false;
    s_tried = true;
    count = std::min(count, kMaxLines);
    std::vector<unsigned char> cov;
    if (!RenderCoverage(lines, count, cov)) {
        Log("status text: GDI couldn't render the text");
        return false;
    }
    const std::vector<uint32_t> px = Shadowed(cov, count);
    s_tex = quads::MakeTexture(dev, px.data(), kTexW, kTexH, true);
    if (!s_tex) { Log("status text: can't create the texture"); return false; }
    s_count = count;
    return true;
}

void Draw(const Item* items, int count)
{
    if (!s_tex || count <= 0) return;
    quads::Batch batch;
    for (int i = 0; i < count; ++i) {
        const Item& it = items[i];
        if (it.line < 0 || it.line >= s_count || it.height <= 0.0f) continue;
        const Row& row = s_rows[it.line];
        const float scale = it.height / kFontPx;                // screen pixels per texture pixel
        const float h = kRowH * scale, w = h * row.aspect;
        const float l = std::round(it.centerX - w / 2), t = std::round(it.top - kTextTop * scale);
        const float v0 = (float)(it.line * kRowH) / kTexH, v1 = (float)((it.line + 1) * kRowH) / kTexH;
        batch.Add(s_tex, l, t, l + w, t + h, 0.0f, v0, row.u1, v1, it.depth, it.argb);
    }
}

void Release()
{
    if (s_tex) { s_tex->Release(); s_tex = nullptr; }
    s_tried = false;
    s_count = 0;
}

} // namespace render::status_text
