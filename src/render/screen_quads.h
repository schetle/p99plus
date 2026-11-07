#pragma once
// Textured screen-space quads drawn straight through Direct3D at a given depth, z-tested against
// the world but not writing depth, so they hide behind the scene the way the engine's name labels
// do. Used for what we draw next to the labels (game/labels.h AfterLabels): spell icons, the
// health status text.
#include <cstdint>
#include <vector>

struct IDirect3DDevice9;
struct IDirect3DTexture9;

namespace render::quads {

// A managed A8R8G8B8 texture from top-down, straight-alpha BGRA pixels (w * h), or null. With
// `mips`, the smaller levels are built too (for images drawn well below their size).
IDirect3DTexture9* MakeTexture(IDirect3DDevice9* dev, const uint32_t* px, int w, int h, bool mips = false);

// Collects quads; the destructor draws them all (grouped by texture) with the device state set up
// for it, and puts the device state back. Call where the world's depth buffer is bound.
class Batch {
public:
    Batch() = default;
    ~Batch();
    Batch(const Batch&) = delete;
    Batch& operator=(const Batch&) = delete;

    // Screen pixels (left, top, right, bottom), texture coordinates, post-projection depth
    // (0 near .. 1 far; anything else is skipped) and a tint multiplied into the texture.
    void Add(IDirect3DTexture9* tex, float l, float t, float r, float b,
             float u0, float v0, float u1, float v1, float depth, uint32_t argb);

private:
    struct Vertex { float x, y, z, rhw; uint32_t color; float u, v; };
    struct Group { IDirect3DTexture9* tex; std::vector<Vertex> verts; };
    std::vector<Group> m_groups;
};

void ReleaseDeviceState();   // before a device reset
void Release();              // at unload

} // namespace render::quads
