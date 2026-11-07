#include "render/icons.h"
#include "core/app.h"
#include "core/log.h"
#include "render/dispatch.h"
#include "render/screen_quads.h"
#include <algorithm>
#include <d3d9.h>
#include <fstream>
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
        s_sheets[i] = quads::MakeTexture(dev, px.data(), w, h);
        ok += s_sheets[i] != nullptr;
    }
    s_loaded = true;
    Log("icons: %d of %d spell icon sheets loaded", ok, kSheets);
    return ok > 0;
}

void Draw(const Icon* icons, int count)
{
    if (!s_loaded || count <= 0) return;
    quads::Batch batch;
    for (int i = 0; i < count; ++i) {
        const Icon& c = icons[i];
        const int sheet = c.cell / kPerSheet;
        if (c.cell < 0 || sheet >= kSheets || !s_sheets[sheet] || c.size <= 0.0f) continue;
        const int at = c.cell % kPerSheet;
        const float u0 = (float)(at % kPerRow * kCell) / kSheetSize, v0 = (float)(at / kPerRow * kCell) / kSheetSize;
        const float u1 = u0 + (float)kCell / kSheetSize, v1 = v0 + (float)kCell / kSheetSize;
        batch.Add(s_sheets[sheet], c.x, c.y, c.x + c.size, c.y + c.size, u0, v0, u1, v1, c.depth, c.argb);
    }
}

void Release()
{
    for (IDirect3DTexture9*& t : s_sheets)
        if (t) { t->Release(); t = nullptr; }
    s_loaded = false;
}

} // namespace render::icons
