# Debug State — Blackhole: GLFW → Win32+WGL 迁移

## 当前状态
编译 ✅ (2026-06-28)
渲染 ✅ 黑洞正常显示
任务栏 ✅ 可点击，无双任务栏

## 已解决的问题

### 问题 A: 渲染不显示
根因: WS_EX_NOREDIRECTIONBITMAP 过早设置
修复: 从 CreateWindowEx 移除，移至 ShowWindow 之后

### 问题 B: DPI 缩放
根因: 未声明 DPI 感知
修复: main() 开头 SetProcessDPIAware()

### 问题 C: 双任务栏
根因: WGC 捕获全屏但窗口仅覆盖工作区
修复: 纹理上传时用 capOffX/capOffY 裁剪到工作区子区域

## 架构
```
Win32窗口(工作区尺寸) + WGL OpenGL 3.3
  ↓
WGC捕获(全屏) → 纹理裁剪(工作区偏移) → OpenGL纹理
  ↓
黑洞Shader → SwapBuffers
```
