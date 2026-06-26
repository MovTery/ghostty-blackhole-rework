// screen_capture.h ? DXGI Desktop Duplication wrapper
#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>

struct ScreenCapture {
    ID3D11Device*        d3dDev = nullptr;
    ID3D11DeviceContext* d3dCtx = nullptr;
    IDXGIOutputDuplication* dupl = nullptr;
    int width  = 0;
    int height = 0;
};

// Initialize D3D11 device + DXGI Desktop Duplication for primary monitor
bool scInit(ScreenCapture& sc);

// Acquire latest desktop frame as D3D11 texture (GPU memory, unmapped).
// Returns true and sets outTex on success (caller must Release()).
// Returns false on timeout or error ? caller reuses previous texture.
bool scAcquireFrame(ScreenCapture& sc, ID3D11Texture2D*& outTex);

// Release all DXGI resources
void scShutdown(ScreenCapture& sc);
