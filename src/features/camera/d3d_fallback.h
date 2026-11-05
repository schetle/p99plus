#pragma once
// Direct3D-level camera offset, used only when the engine camera hooks aren't active.
//
// The game hands its view matrix to IDirect3DDevice9::SetTransform and uploads View*Proj blocks
// as vertex shader constants; render/d3d9.cpp routes those calls here so the offset can be
// applied to both. (World geometry takes its camera from the shader constants.)
#include <d3d9.h>

namespace camera::fallback {

// SetTransform: returns the matrix to pass on (`m` itself or an offset copy).
const D3DMATRIX* OnSetTransform(D3DTRANSFORMSTATETYPE state, const D3DMATRIX* m);

// BeginScene: the game may set the view only when it changes, so this is the last view with
// the current offset applied, to push again. nullptr if the game hasn't set a view yet.
const D3DMATRIX* ViewForBeginScene();

void OnSetVertexShader(void* vs);

// SetVertexShaderConstantF: returns the data to pass on (`data` itself or a rewritten copy).
const float* OnSetVertexShaderConstants(UINT reg, const float* data, UINT count);

} // namespace camera::fallback
