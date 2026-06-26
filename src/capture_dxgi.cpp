// capture_dxgi.cpp  DXGI Desktop Duplication — stable, no DWM composition race
#include "capture_dxgi.h"
#include <cstdio>
#include <windows.h>

bool DXGI_Init(DXGICapture& dxgi) {
    dxgi.active = false;
    dxgi.width  = GetSystemMetrics(SM_CXSCREEN);
    dxgi.height = GetSystemMetrics(SM_CYSCREEN);

    // Create D3D11 device
    D3D_FEATURE_LEVEL featLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &dxgi.d3dDev, &featLevel, &dxgi.d3dCtx);
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] D3D11CreateDevice failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    // Get DXGI output for primary monitor
    IDXGIDevice* dxgiDev = nullptr;
    hr = dxgi.d3dDev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDev->GetAdapter(&adapter);
    dxgiDev->Release();
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    adapter->Release();
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) { DXGI_Release(dxgi); return false; }

    hr = output1->DuplicateOutput(dxgi.d3dDev, &dxgi.dupl);
    output1->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] DuplicateOutput failed: 0x%08X\n", (unsigned)hr);
        DXGI_Release(dxgi);
        return false;
    }

    // Create staging texture
    D3D11_TEXTURE2D_DESC stagingDesc = {};
    stagingDesc.Width          = dxgi.width;
    stagingDesc.Height         = dxgi.height;
    stagingDesc.MipLevels      = 1;
    stagingDesc.ArraySize      = 1;
    stagingDesc.Format         = DXGI_FORMAT_B8G8R8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage          = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags      = 0;
    hr = dxgi.d3dDev->CreateTexture2D(&stagingDesc, nullptr, &dxgi.stagingTex);
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] Create staging tex failed: 0x%08X\n", (unsigned)hr);
        DXGI_Release(dxgi);
        return false;
    }

    dxgi.active = true;
    fprintf(stderr, "[DXGI] Capture ready: %dx%d\n", dxgi.width, dxgi.height);
    return true;
}

ID3D11Texture2D* DXGI_GetFrame(DXGICapture& dxgi) {
    if (!dxgi.active || !dxgi.dupl) return nullptr;

    IDXGIResource* frameRes = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};
    HRESULT hr = dxgi.dupl->AcquireNextFrame(16, &frameInfo, &frameRes);

    if (FAILED(hr)) return nullptr;

    ID3D11Texture2D* tex = nullptr;
    hr = frameRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex);
    frameRes->Release();
    if (FAILED(hr)) {
        dxgi.dupl->ReleaseFrame();
        return nullptr;
    }
    return tex;
}

void DXGI_ReleaseFrame(DXGICapture& dxgi) {
    if (dxgi.dupl) dxgi.dupl->ReleaseFrame();
}

bool DXGI_CopyToStaging(DXGICapture& dxgi, ID3D11Texture2D* srcTex,
                        D3D11_MAPPED_SUBRESOURCE& mapped) {
    if (!dxgi.active || !srcTex || !dxgi.stagingTex) return false;
    dxgi.d3dCtx->CopyResource(dxgi.stagingTex, srcTex);
    HRESULT hr = dxgi.d3dCtx->Map(dxgi.stagingTex, 0, D3D11_MAP_READ, 0, &mapped);
    return SUCCEEDED(hr);
}

void DXGI_UnmapStaging(DXGICapture& dxgi) {
    if (dxgi.stagingTex) dxgi.d3dCtx->Unmap(dxgi.stagingTex, 0);
}

void DXGI_Release(DXGICapture& dxgi) {
    if (dxgi.stagingTex) { dxgi.stagingTex->Release(); dxgi.stagingTex = nullptr; }
    if (dxgi.dupl)      { dxgi.dupl->Release();      dxgi.dupl      = nullptr; }
    if (dxgi.d3dCtx)    { dxgi.d3dCtx->Release();    dxgi.d3dCtx    = nullptr; }
    if (dxgi.d3dDev)    { dxgi.d3dDev->Release();    dxgi.d3dDev    = nullptr; }
    dxgi.active = false;
}