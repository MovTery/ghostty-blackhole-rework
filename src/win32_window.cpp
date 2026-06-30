// win32_window.cpp  Win32 原生窗口实现 (纯窗口, 无渲染 API)
// 从 win32_gl.cpp 抽取, 移除所有 WGL / OpenGL 代码
#include "win32_window.h"
#include <cstdio>
#include <cstring>

// ---- 窗口状态 (挂在 GWLP_USERDATA) ----
struct Win32WindowState {
    bool shouldClose;
    int  newWidth;
    int  newHeight;
};

// ---- 窗口过程 ----
static LRESULT CALLBACK Win32WndProc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp) {
    Win32WindowState* state =
        (Win32WindowState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        state = (Win32WindowState*)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)state);
        return 0;
    }
    case WM_CLOSE:
        if (state) state->shouldClose = true;
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE && state) state->shouldClose = true;
        return 0;
    case WM_SIZE:
        if (state) {
            state->newWidth  = LOWORD(lp);
            state->newHeight = HIWORD(lp);
        }
        return 0;

    // ---- 桌面 Overlay 必需的消息处理 ----
    case WM_NCHITTEST:
        return HTTRANSPARENT;           // 鼠标穿透
    case WM_NCACTIVATE:
        return FALSE;                   // 拒绝焦点视觉反馈
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATEANDEAT;     // 不激活
    case WM_NCCALCSIZE:
        return 0;                       // 无边距
    case WM_ERASEBKGND:
        return 1;                       // 阻止擦除背景闪烁
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- 公开 API ----

bool Win32Window_Init(Win32Window& w, const char* title,
                      int width, int height,
                      DWORD extraExStyle, DWORD extraStyle) {
    w.active = false;

    // 1. 获取屏幕尺寸
    w.capFullW = GetSystemMetrics(SM_CXSCREEN);
    w.capFullH = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) {
        width  = w.capFullW;
        height = w.capFullH;
    }
    w.capOffX  = 0;
    w.capOffY  = 0;
    w.width    = width;
    w.height   = height;

    // 2. 注册窗口类 (只注册一次)
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = Win32WndProc;
    wc.hInstance     = GetModuleHandleW(nullptr);
    wc.hCursor       = LoadCursorA(nullptr, IDC_ARROW);
    wc.lpszClassName = L"BlackHoleWin32";
    wc.hIcon         = LoadIconW(GetModuleHandleW(nullptr),
                                  MAKEINTRESOURCEW(100));

    static bool registered = false;
    if (!registered) {
        RegisterClassExW(&wc);
        registered = true;
    }

    // 3. 构建窗口样式
    // 基础: Overlay 桌面窗口, 无边框, 不抢焦点, 不在任务栏
    DWORD exStyle = WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | extraExStyle;
    DWORD style   = WS_POPUP | extraStyle;

    // 4. 转换标题为宽字符
    WCHAR wTitle[128];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, 128);

    // 5. 创建窗口
    w.hwnd = CreateWindowExW(
        exStyle, L"BlackHoleWin32", wTitle, style,
        0, 0, width, height,
        nullptr, nullptr, wc.hInstance, nullptr);

    if (!w.hwnd) {
        fprintf(stderr, "[Win32Window] CreateWindowEx failed: %lu\n",
                GetLastError());
        return false;
    }

    // 6. 创建窗口状态
    Win32WindowState* state = new Win32WindowState();
    state->shouldClose = false;
    state->newWidth    = width;
    state->newHeight   = height;
    SetWindowLongPtrW(w.hwnd, GWLP_USERDATA, (LONG_PTR)state);

    // 7. 设为 TopMost, 不激活显示
    SetWindowPos(w.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ShowWindow(w.hwnd, SW_SHOWNOACTIVATE);

    w.active = true;
    fprintf(stderr, "[Win32Window] Ready: %dx%d\n", width, height);
    return true;
}

bool Win32Window_PollEvents(Win32Window& w) {
    if (!w.active || !w.hwnd) return false;

    MSG msg;
    // 处理本窗口消息
    while (PeekMessageW(&msg, w.hwnd, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    // 处理线程消息 (WM_QUIT 等)
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) {
            w.active = false;
            return false;
        }
    }

    // 检查窗口关闭标志
    Win32WindowState* state =
        (Win32WindowState*)GetWindowLongPtrW(w.hwnd, GWLP_USERDATA);
    if (state && state->shouldClose) {
        w.active = false;
        return false;
    }
    return w.active;
}

double Win32Window_GetTime() {
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

void Win32Window_SetTitle(Win32Window& w, const char* title) {
    if (w.active && w.hwnd) {
        WCHAR wTitle[256];
        MultiByteToWideChar(CP_UTF8, 0, title, -1, wTitle, 256);
        SetWindowTextW(w.hwnd, wTitle);
    }
}

void Win32Window_SetTopMost(Win32Window& w, bool topmost) {
    if (!w.active || !w.hwnd) return;
    SetWindowPos(w.hwnd,
                 topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void Win32Window_ShowSystemCursor(bool show) {
    ShowCursor(show ? TRUE : FALSE);
}

void Win32Window_Shutdown(Win32Window& w) {
    if (!w.active) return;

    Win32WindowState* state =
        (Win32WindowState*)GetWindowLongPtrW(w.hwnd, GWLP_USERDATA);
    if (state) delete state;

    if (w.hwnd) {
        DestroyWindow(w.hwnd);
        w.hwnd = nullptr;
    }
    w.active = false;
    fprintf(stderr, "[Win32Window] Shutdown complete\n");
}
