// win32_gl.cpp  Win32 原生窗口 + WGL OpenGL 上下文实现
// 替代 GLFW 渲染器窗口，直接控制所有桌面特效属性
#include "win32_gl.h"
#include <cstdio>
#include <cstring>
#include <dwmapi.h>

#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMNCRP_DISABLED
#define DWMNCRP_DISABLED 1
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

// ---- WGL 扩展常量 ----
#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB      0x2091
#endif
#ifndef WGL_CONTEXT_MINOR_VERSION_ARB
#define WGL_CONTEXT_MINOR_VERSION_ARB      0x2092
#endif
#ifndef WGL_CONTEXT_PROFILE_MASK_ARB
#define WGL_CONTEXT_PROFILE_MASK_ARB       0x9126
#endif
#ifndef WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#endif
#ifndef WGL_CONTEXT_FLAGS_ARB
#define WGL_CONTEXT_FLAGS_ARB              0x2094
#endif

typedef HGLRC (WINAPI *PFN_wglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL (WINAPI *PFN_wglSwapIntervalEXT)(int);

// ---- 窗口状态 ----
struct Win32GLState {
    bool shouldClose;
    int  newWidth;
    int  newHeight;
};

static LRESULT CALLBACK WGLWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Win32GLState* state = (Win32GLState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        state = (Win32GLState*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;
    }
    case WM_CLOSE:        if (state) state->shouldClose = true; return 0;
    case WM_KEYDOWN:      if (wp == VK_ESCAPE && state) state->shouldClose = true; return 0;
    case WM_SIZE:         if (state) { state->newWidth = LOWORD(lp); state->newHeight = HIWORD(lp); } return 0;
    case WM_NCHITTEST:    return HTTRANSPARENT;
    case WM_NCACTIVATE:   return FALSE;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATEANDEAT;
    case WM_NCCALCSIZE:   return 0;
    case WM_ERASEBKGND:   return 1;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- 公开 API ----

bool Win32GL_Init(Win32GL& wgl, const char* title, int width, int height) {
    wgl.active = false;

    int wx = 0, wy = 0;
    wgl.capFullW = GetSystemMetrics(SM_CXSCREEN);
    wgl.capFullH = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) {
        width  = wgl.capFullW;
        height = wgl.capFullH;
    }
    wgl.capOffX = 0;
    wgl.capOffY = 0;
    wgl.width   = width;
    wgl.height  = height;

    // 1. 注册窗口类
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = WGLWndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.lpszClassName = L"BlackHoleWGL";
    wc.hIcon         = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(100));

    static bool registered = false;
    if (!registered) { RegisterClassExW(&wc); registered = true; }

    // 2. 创建窗口（全屏无边框桌面特效窗口）
    DWORD exStyle = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
                    WS_EX_TRANSPARENT | WS_EX_LAYERED;
    DWORD style   = WS_POPUP;

    WCHAR wTitle[128];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, 128);

    wgl.hwnd = CreateWindowExW(
        exStyle, L"BlackHoleWGL", wTitle, style,
        wx, wy, width, height,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!wgl.hwnd) {
        fprintf(stderr, "[Win32GL] CreateWindowEx failed: %lu\n", GetLastError());
        return false;
    }

    // 分层窗口：设 alpha=255 保持不透明，但启用 DWM 分层合成以支持鼠标穿透
    SetLayeredWindowAttributes(wgl.hwnd, 0, 255, LWA_ALPHA);

    // 3. 获取 DC
    wgl.hdc = GetDC(wgl.hwnd);
    if (!wgl.hdc) {
        DestroyWindow(wgl.hwnd); wgl.hwnd = nullptr;
        return false;
    }

    // 4. 像素格式
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;

    int pf = ChoosePixelFormat(wgl.hdc, &pfd);
    if (!pf || !SetPixelFormat(wgl.hdc, pf, &pfd)) {
        fprintf(stderr, "[Win32GL] SetPixelFormat failed\n");
        ReleaseDC(wgl.hwnd, wgl.hdc); DestroyWindow(wgl.hwnd);
        wgl.hwnd = nullptr;
        return false;
    }

    // 5. 临时上下文（加载 WGL 扩展用）
    HGLRC dummyRC = wglCreateContext(wgl.hdc);
    if (!dummyRC) {
        ReleaseDC(wgl.hwnd, wgl.hdc); DestroyWindow(wgl.hwnd);
        wgl.hwnd = nullptr;
        return false;
    }
    wglMakeCurrent(wgl.hdc, dummyRC);

    // 6. 加载 wglCreateContextAttribsARB
    PFN_wglCreateContextAttribsARB wglCreateContextAttribsARB =
        (PFN_wglCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");

    // 7. 创建 OpenGL 3.3 兼容上下文
    if (wglCreateContextAttribsARB) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            WGL_CONTEXT_FLAGS_ARB,         0,
            0
        };
        wgl.hglrc = wglCreateContextAttribsARB(wgl.hdc, nullptr, attribs);
    }
    if (!wgl.hglrc) {
        fprintf(stderr, "[Win32GL] wglCreateContextAttribsARB failed, fallback\n");
        wgl.hglrc = wglCreateContext(wgl.hdc);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummyRC);

    if (!wgl.hglrc) {
        ReleaseDC(wgl.hwnd, wgl.hdc); DestroyWindow(wgl.hwnd);
        wgl.hwnd = nullptr;
        return false;
    }
    wglMakeCurrent(wgl.hdc, wgl.hglrc);

    // 8. VSync
    PFN_wglSwapIntervalEXT wglSwapIntervalEXT =
        (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);

    // 9. DWM 属性（关键：防止 Win11 黄边框、焦点抢占）
    DWM_BLURBEHIND bb = {};
    bb.dwFlags = DWM_BB_ENABLE;
    bb.fEnable = FALSE;
    DwmEnableBlurBehindWindow(wgl.hwnd, &bb);

    COLORREF bc = 0;
    DwmSetWindowAttribute(wgl.hwnd, DWMWA_BORDER_COLOR, &bc, sizeof(bc));

    DWMNCRENDERINGPOLICY ncrp = (DWMNCRENDERINGPOLICY)DWMNCRP_DISABLED;
    DwmSetWindowAttribute(wgl.hwnd, DWMWA_NCRENDERING_POLICY, &ncrp, sizeof(ncrp));

    if (!SetWindowDisplayAffinity(wgl.hwnd, WDA_EXCLUDEFROMCAPTURE))
        fprintf(stderr, "[Win32GL] WDA_EXCLUDEFROMCAPTURE failed: %lu\n", GetLastError());

    // 10. 显示窗口（WS_EX_LAYERED + WS_EX_TRANSPARENT 处理鼠标穿透）
    SetWindowPos(wgl.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(wgl.hwnd, SW_SHOWNOACTIVATE);

    // 11. 窗口状态结构
    Win32GLState* state = new Win32GLState();
    state->shouldClose = false;
    state->newWidth    = width;
    state->newHeight   = height;
    SetWindowLongPtrW(wgl.hwnd, GWLP_USERDATA, (LONG_PTR)state);

    wgl.active = true;
    fprintf(stderr, "[Win32GL] Window ready: %dx%d\n", width, height);
    return true;
}

void Win32GL_SwapBuffers(Win32GL& wgl) {
    if (wgl.active && wgl.hdc) ::SwapBuffers(wgl.hdc);
}

void Win32GL_GetFramebufferSize(Win32GL& wgl, int* w, int* h) {
    if (wgl.active) {
        Win32GLState* state = (Win32GLState*)GetWindowLongPtrW(wgl.hwnd, GWLP_USERDATA);
        if (state) {
            *w = state->newWidth;
            *h = state->newHeight;
            return;
        }
    }
    *w = wgl.width;
    *h = wgl.height;
}

void* Win32GL_GetProcAddress(const char* name) {
    void* proc = (void*)wglGetProcAddress(name);
    if (!proc) {
        HMODULE gl = GetModuleHandleA("opengl32.dll");
        if (gl) proc = (void*)GetProcAddress(gl, name);
    }
    return proc;
}

double Win32GL_GetTime() {
    static LARGE_INTEGER freq = {};
    static bool init = false;
    if (!init) {
        QueryPerformanceFrequency(&freq);
        init = true;
    }
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)freq.QuadPart;
}

void Win32GL_SetWindowTitle(Win32GL& wgl, const char* title) {
    if (wgl.active && wgl.hwnd) {
        WCHAR wTitle[256];
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, 256);
        SetWindowTextW(wgl.hwnd, wTitle);
    }
}

bool Win32GL_SetSwapInterval(int interval) {
    PFN_wglSwapIntervalEXT fn =
        (PFN_wglSwapIntervalEXT)wglGetProcAddress("wglSwapIntervalEXT");
    if (fn) return fn(interval) != FALSE;
    return false;
}

bool Win32GL_PollEvents(Win32GL& wgl) {
    if (!wgl.active) return false;

    MSG msg;
    while (PeekMessageW(&msg, wgl.hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) {
            wgl.active = false;
            return false;
        }
    }

    Win32GLState* state = (Win32GLState*)GetWindowLongPtrW(wgl.hwnd, GWLP_USERDATA);
    if (state && state->shouldClose) {
        wgl.active = false;
        return false;
    }
    return wgl.active;
}

void Win32GL_Shutdown(Win32GL& wgl) {
    if (!wgl.active) return;

    Win32GLState* state = (Win32GLState*)GetWindowLongPtrW(wgl.hwnd, GWLP_USERDATA);
    if (state) delete state;

    if (wgl.hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(wgl.hglrc);
        wgl.hglrc = nullptr;
    }
    if (wgl.hdc) {
        ReleaseDC(wgl.hwnd, wgl.hdc);
        wgl.hdc = nullptr;
    }
    if (wgl.hwnd) {
        DestroyWindow(wgl.hwnd);
        wgl.hwnd = nullptr;
    }
    wgl.active = false;
    fprintf(stderr, "[Win32GL] Shutdown complete\n");
}
