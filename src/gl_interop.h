// gl_interop.h ? WGL_NV_DX_interop2: zero-copy D3D11 ? OpenGL texture sharing
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>

struct GLInterop {
    HANDLE            hDev      = nullptr;  // wglDXOpenDeviceNV handle
    HANDLE            hObj      = nullptr;  // wglDXRegisterObjectNV handle
    ID3D11Texture2D*  sharedTex = nullptr;  // fixed shared texture (created once)
    ID3D11DeviceContext* d3dCtx = nullptr;  // for CopyResource
    GLuint            glTex     = 0;        // OpenGL texture name
    int               width     = 0;
    int               height    = 0;
    bool              active    = false;
};

// Initialize: load WGL functions, open DX device, create shared texture, register with OpenGL
// d3dCtx is used for CopyResource during giUpdate
bool giInit(GLInterop& gi, ID3D11Device* d3dDev, ID3D11DeviceContext* d3dCtx,
            int width, int height);

// Copy a new desktop frame into the fixed shared texture (GPU-side CopyResource, no CPU)
void giUpdate(GLInterop& gi, ID3D11Texture2D* srcTex);

// Lock interop object before rendering (must be paired with giUnlock)
void giLock(GLInterop& gi);

// Unlock interop object after rendering
void giUnlock(GLInterop& gi);

// Get the OpenGL texture name for shader binding
GLuint giGetTexture(const GLInterop& gi);

// Release all interop resources
void giShutdown(GLInterop& gi);
