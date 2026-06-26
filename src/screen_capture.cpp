// screen_capture.cpp ? DXGI Desktop Duplication implementation
#include "screen_capture.h"
#include <cstdio>
#include <windows.h>

bool scInit(ScreenCapture& sc) {
    sc.width  = GetSystemMetrics(SM_CXSCREEN);
    sc.height = GetSystemMetrics(SM_CYSCREEN);

    // Create D3D11 device with BGRA support (required for DXGI desktop frames)
    D3D_FEATURE_LEVEL featLevel;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION,
        &sc.d3dDev, &featLevel, &sc.d3dCtx);
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] D3D11CreateDevice failed: 0x%08X\n", (unsigned)hr);
        return false;
    }

    // Get IDXGIDevice
    IDXGIDevice* dxgiDev = nullptr;
    hr = sc.d3dDev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] QueryInterface IDXGIDevice failed: 0x%08X\n", (unsigned)hr);
        scShutdown(sc);
        return false;
    }

    // Get adapter
    IDXGIAdapter* adapter = nullptr;
    hr = dxgiDev->GetAdapter(&adapter);
    dxgiDev->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] GetAdapter failed: 0x%08X\n", (unsigned)hr);
        scShutdown(sc);
        return false;
    }

    // Get primary output
    IDXGIOutput* output = nullptr;
    hr = adapter->EnumOutputs(0, &output);
    adapter->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] EnumOutputs failed: 0x%08X\n", (unsigned)hr);
        scShutdown(sc);
        return false;
    }

    // Get IDXGIOutput1 for DuplicateOutput
    IDXGIOutput1* output1 = nullptr;
    hr = output->QueryInterface(__uuidof(IDXGIOutput1), (void**)&output1);
    output->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] QueryInterface IDXGIOutput1 failed: 0x%08X\n", (unsigned)hr);
        scShutdown(sc);
        return false;
    }

    // Create desktop duplication
    hr = output1->DuplicateOutput(sc.d3dDev, &sc.dupl);
    output1->Release();
    if (FAILED(hr)) {
        fprintf(stderr, "[DXGI] DuplicateOutput failed: 0x%08X (try running without admin)\n", (unsigned)hr);
        scShutdown(sc);
        return false;
    }

    fprintf(stderr, "[DXGI] Desktop capture ready: %dx%d\n", sc.width, sc.height);
    return true;
}

bool scAcquireFrame(ScreenCapture& sc, ID3D11Texture2D*& outTex) {
    outTex = nullptr;

    // Release previous frame before acquiring new one
    if (sc.dupl) sc.dupl->ReleaseFrame();

    IDXGIResource* frameRes = nullptr;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = {};

    HRESULT hr = sc.dupl->AcquireNextFrame(50, &frameInfo, &frameRes);

    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return false;  // No new frame available, caller reuses last texture
    if (FAILED(hr))
        return false;

    hr = frameRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&outTex);
    frameRes->Release();

    return SUCCEEDED(hr) && outTex != nullptr;
}

void scShutdown(ScreenCapture& sc) {
    if (sc.dupl)   { sc.dupl->Release();   sc.dupl   = nullptr; }
    if (sc.d3dCtx) { sc.d3dCtx->Release(); sc.d3dCtx = nullptr; }
    if (sc.d3dDev) { sc.d3dDev->Release(); sc.d3dDev = nullptr; }
}
