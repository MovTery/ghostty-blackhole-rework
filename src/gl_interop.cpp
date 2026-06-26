// gl_interop.cpp ? WGL_NV_DX_interop2: zero-copy D3D11 ? OpenGL texture sharing
//
// Pipeline: DXGI frame ? CopyResource ? fixed sharedTex ? WGL interop ? OpenGL sample
// NO CPU memcpy, NO Map(), NO staging buffers.
#include "gl_interop.h"
#include <cstdio>

// ---- WGL_NV_DX_interop2 function pointers (loaded via wglGetProcAddress) ----
typedef HANDLE (WINAPI *PFN_WGL_DX_OPEN_DEVICE_NV)(void* dxDevice);
typedef BOOL   (WINAPI *PFN_WGL_DX_CLOSE_DEVICE_NV)(HANDLE hDevice);
typedef HANDLE (WINAPI *PFN_WGL_DX_REGISTER_OBJECT_NV)(HANDLE hDevice, void* dxObject,
                         GLuint name, GLenum type, GLenum access);
typedef BOOL   (WINAPI *PFN_WGL_DX_UNREGISTER_OBJECT_NV)(HANDLE hDevice, HANDLE hObject);
typedef BOOL   (WINAPI *PFN_WGL_DX_LOCK_OBJECTS_NV)(HANDLE hDevice, GLsizei count,
                         HANDLE* hObjects);
typedef BOOL   (WINAPI *PFN_WGL_DX_UNLOCK_OBJECTS_NV)(HANDLE hDevice, GLsizei count,
                         HANDLE* hObjects);

static PFN_WGL_DX_OPEN_DEVICE_NV        p_wglDXOpenDeviceNV       = nullptr;
static PFN_WGL_DX_CLOSE_DEVICE_NV       p_wglDXCloseDeviceNV      = nullptr;
static PFN_WGL_DX_REGISTER_OBJECT_NV    p_wglDXRegisterObjectNV   = nullptr;
static PFN_WGL_DX_UNREGISTER_OBJECT_NV  p_wglDXUnregisterObjectNV = nullptr;
static PFN_WGL_DX_LOCK_OBJECTS_NV       p_wglDXLockObjectsNV      = nullptr;
static PFN_WGL_DX_UNLOCK_OBJECTS_NV     p_wglDXUnlockObjectsNV    = nullptr;

// ---------------------------------------------------------------------------
bool giInit(GLInterop& gi, ID3D11Device* d3dDev, ID3D11DeviceContext* d3dCtx,
            int width, int height)
{
    memset(&gi, 0, sizeof(gi));
    gi.width  = width;
    gi.height = height;
    gi.d3dCtx = d3dCtx;

    // 1. Load WGL_NV_DX_interop2 entry points
    p_wglDXOpenDeviceNV       = (PFN_WGL_DX_OPEN_DEVICE_NV)
        wglGetProcAddress("wglDXOpenDeviceNV");
    p_wglDXCloseDeviceNV      = (PFN_WGL_DX_CLOSE_DEVICE_NV)
        wglGetProcAddress("wglDXCloseDeviceNV");
    p_wglDXRegisterObjectNV   = (PFN_WGL_DX_REGISTER_OBJECT_NV)
        wglGetProcAddress("wglDXRegisterObjectNV");
    p_wglDXUnregisterObjectNV = (PFN_WGL_DX_UNREGISTER_OBJECT_NV)
        wglGetProcAddress("wglDXUnregisterObjectNV");
    p_wglDXLockObjectsNV      = (PFN_WGL_DX_LOCK_OBJECTS_NV)
        wglGetProcAddress("wglDXLockObjectsNV");
    p_wglDXUnlockObjectsNV    = (PFN_WGL_DX_UNLOCK_OBJECTS_NV)
        wglGetProcAddress("wglDXUnlockObjectsNV");

    if (!p_wglDXOpenDeviceNV || !p_wglDXRegisterObjectNV || !p_wglDXLockObjectsNV) {
        fprintf(stderr, "[GLInterop] WGL_NV_DX_interop2 not available\n");
        return false;
    }

    // 2. Open the D3D11 device for WGL interop
    gi.hDev = p_wglDXOpenDeviceNV(d3dDev);
    if (!gi.hDev) {
        fprintf(stderr, "[GLInterop] wglDXOpenDeviceNV failed\n");
        return false;
    }

    // 3. Create fixed shared D3D11 texture (created ONCE, reused every frame)
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width          = width;
    desc.Height         = height;
    desc.MipLevels      = 1;
    desc.ArraySize      = 1;
    desc.Format         = DXGI_FORMAT_B8G8R8A8_UNORM;  // match DXGI desktop output
    desc.SampleDesc.Count = 1;
    desc.Usage          = D3D11_USAGE_DEFAULT;
    desc.BindFlags      = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags      = D3D11_RESOURCE_MISC_SHARED;   // required for WGL interop

    HRESULT hr = d3dDev->CreateTexture2D(&desc, nullptr, &gi.sharedTex);
    if (FAILED(hr)) {
        fprintf(stderr, "[GLInterop] CreateTexture2D failed: 0x%08X\n", (unsigned)hr);
        p_wglDXCloseDeviceNV(gi.hDev);
        gi.hDev = nullptr;
        return false;
    }

    // 4. Create OpenGL texture and register with D3D texture
    glGenTextures(1, &gi.glTex);
    gi.hObj = p_wglDXRegisterObjectNV(gi.hDev, gi.sharedTex, gi.glTex,
                                       GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV);
    if (!gi.hObj) {
        fprintf(stderr, "[GLInterop] wglDXRegisterObjectNV failed\n");
        gi.sharedTex->Release();
        gi.sharedTex = nullptr;
        p_wglDXCloseDeviceNV(gi.hDev);
        gi.hDev = nullptr;
        glDeleteTextures(1, &gi.glTex);
        gi.glTex = 0;
        return false;
    }

    // 5. Set texture parameters (safe on interop-registered texture)
    glBindTexture(GL_TEXTURE_2D, gi.glTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    gi.active = true;
    fprintf(stderr, "[GLInterop] Zero-copy interop ready: %dx%d (WGL_NV_DX_interop2)\n",
            width, height);
    return true;
}

// ---------------------------------------------------------------------------
void giUpdate(GLInterop& gi, ID3D11Texture2D* srcTex) {
    if (!gi.active || !srcTex) return;
    // GPU-side copy: D3D11 immediate context copies the new frame into the
    // fixed shared texture. No CPU involvement, no Map, no memcpy.
    gi.d3dCtx->CopyResource(gi.sharedTex, srcTex);
}

// ---------------------------------------------------------------------------
void giLock(GLInterop& gi) {
    if (!gi.active) return;
    BOOL ok = p_wglDXLockObjectsNV(gi.hDev, 1, &gi.hObj);
    if (!ok) {
        // Non-fatal: rendering continues with possibly stale texture
        static int lockWarn = 0;
        if (lockWarn++ < 3)
            fprintf(stderr, "[GLInterop] wglDXLockObjectsNV failed\n");
    }
}

// ---------------------------------------------------------------------------
void giUnlock(GLInterop& gi) {
    if (!gi.active) return;
    p_wglDXUnlockObjectsNV(gi.hDev, 1, &gi.hObj);
}

// ---------------------------------------------------------------------------
GLuint giGetTexture(const GLInterop& gi) {
    return gi.glTex;
}

// ---------------------------------------------------------------------------
void giShutdown(GLInterop& gi) {
    if (gi.hObj) {
        p_wglDXUnregisterObjectNV(gi.hDev, gi.hObj);
        gi.hObj = nullptr;
    }
    if (gi.sharedTex) {
        gi.sharedTex->Release();
        gi.sharedTex = nullptr;
    }
    if (gi.hDev) {
        p_wglDXCloseDeviceNV(gi.hDev);
        gi.hDev = nullptr;
    }
    if (gi.glTex) {
        glDeleteTextures(1, &gi.glTex);
        gi.glTex = 0;
    }
    gi.active = false;
}
