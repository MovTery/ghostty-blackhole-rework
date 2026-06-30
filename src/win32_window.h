// win32_window.h  Win32 原生窗口 (不含任何渲染 API)
// 职责严格限定: 创建窗口 / 消息循环 / 窗口属性
// 不包含 WGL / OpenGL / D3D11 / SwapChain 等渲染相关代码
#pragma once
#include <windows.h>

struct Win32Window {
    HWND   hwnd   = nullptr;
    int    width  = 0;
    int    height = 0;
    bool   active = false;

    // WGC 捕获偏移 (全屏分辨率 → 工作区裁剪)
    int    capFullW = 0;
    int    capFullH = 0;
    int    capOffX  = 0;
    int    capOffY  = 0;
};

// 初始化窗口
// title:        窗口标题
// width/height: 0 = 使用主显示器分辨率
// extraExStyle: 额外扩展样式 (不同渲染路径按需传入 WS_EX_LAYERED 等)
bool Win32Window_Init(Win32Window& w, const char* title,
                      int width = 0, int height = 0,
                      DWORD extraExStyle = 0,
                      DWORD extraStyle = 0);

// 消息循环 — 返回 false 表示窗口应关闭
bool Win32Window_PollEvents(Win32Window& w);

// 高精度计时器 (秒)
double Win32Window_GetTime();

// 设置窗口标题
void Win32Window_SetTitle(Win32Window& w, const char* title);

// 调整窗口 Z 序 (TopMost)
void Win32Window_SetTopMost(Win32Window& w, bool topmost);

// 显示 / 隐藏系统光标
void Win32Window_ShowSystemCursor(bool show);

// 销毁窗口
void Win32Window_Shutdown(Win32Window& w);
