#pragma once
// What the host's Direct3D hooks do with each call (the hooks themselves are in host/d3d9.cpp).
#include <d3d9.h>

namespace render {

void OnPresent(IDirect3DDevice9* dev);   // features' frame work, the overlay, the frame cap
void PreReset();
void PostReset();
void Shutdown();                         // the core is unloading: release our device objects

IDirect3DDevice9* Device();              // the game's device (from the last Present), or nullptr

// While one exists, the device calls we make ourselves (the overlay, the spell icons) go through
// the host's hooks untouched instead of being treated as the game's.
class OwnCalls {
public:
    OwnCalls();
    ~OwnCalls();
    OwnCalls(const OwnCalls&) = delete;
    OwnCalls& operator=(const OwnCalls&) = delete;
};
const D3DMATRIX* OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m);
const D3DMATRIX* ViewForBeginScene();
void OnSetVertexShader(IDirect3DVertexShader9* vs);
const float* OnSetVSConstants(UINT reg, const float* data, UINT count);

} // namespace render
