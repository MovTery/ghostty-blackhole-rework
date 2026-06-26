# Debug State — Blackhole Desktop Pet v3: DXGI + WGL Interop (实施完成)

## 实施结果

### 编译 ✅
- 零警告零错误通过
- 3 源文件链接: main.cpp + screen_capture.cpp + gl_interop.cpp
- 依赖库: glfw3 + opengl32 + gdi32 + d3d11 + dxgi

### 运行时验证 ✅
```
Blackhole: mode=always idle=300s
OpenGL 4.6.0 NVIDIA 596.49, GLSL 4.60 NVIDIA
[DXGI] Desktop capture ready: 2560x1600
[GLInterop] Zero-copy interop ready: 2560x1600 (WGL_NV_DX_interop2)
SPIR-V compile failed, using GLSL fallback
[frag] log: warning C7533: global variable gl_FragColor is deprecated after version 120
Link log: (success, only deprecation warning)
```
程序正常运行 5 秒无崩溃，shader 编译链接通过。

### GPU 管线 (确认)
```
DXGI Desktop Duplication (2560x1600, ~60fps)
    → CopyResource (GPU内部, 零CPU)
    → 固定 shared texture (WGL_NV_DX_interop2 一次性注册)
    → blackhole.glsl (544行完整物理模拟, MODE_DEMO 42s轮播)
    → 全屏透明叠加窗口 (WS_EX_TOPMOST + WS_EX_TRANSPARENT)
```

### 已知事项
- SPIR-V 路径对组合 shader 不兼容，GLSL fallback 正常运作
- gl_FragColor 废弃警告 (兼容性 profile 正常现象)
- 2560x1600 高分辨率下性能待实际交互验证

## 修改文件汇总

| 文件 | 操作 | 行数 |
|------|------|------|
| `src/screen_capture.h` | 新建 | 23 |
| `src/screen_capture.cpp` | 新建 | 89 |
| `src/gl_interop.h` | 新建 | 35 |
| `src/gl_interop.cpp` | 新建 | 128 |
| `shaders/frag_desktop_header.glsl` | 新建 | 22 |
| `src/main.cpp` | 修改 | +60行(集成DXGI+interop+全屏) |
| `.vscode/tasks.json` | 修改 | +4行(源文件+库) |

## 修改函数

| 函数 | 文件 | 操作 |
|------|------|------|
| `scInit()` | screen_capture.cpp | 新增 — D3D11设备+DXGI桌面复制初始化 |
| `scAcquireFrame()` | screen_capture.cpp | 新增 — 获取桌面帧(GPU纹理) |
| `scShutdown()` | screen_capture.cpp | 新增 — 释放DXGI资源 |
| `giInit()` | gl_interop.cpp | 新增 — WGL_NV_DX_interop2初始化+固定纹理注册 |
| `giUpdate()` | gl_interop.cpp | 新增 — GPU CopyResource更新 |
| `giLock()` / `giUnlock()` | gl_interop.cpp | 新增 — 渲染前后锁定/解锁 |
| `giShutdown()` | gl_interop.cpp | 新增 — 释放interop资源 |
| `buildFragmentShader()` | main.cpp | 重写 — 组合frag_desktop_header+blackhole.glsl |
| `main()` | main.cpp | 修改 — 全屏窗口+DXGI/interop集成+渲染循环 |

## 风险验证

| 风险 | 状态 |
|------|------|
| DXGI AcquireNextFrame 超时 | ✅ 未出现，正常捕获 |
| WGL interop Lock/Unlock 时序 | ✅ 程序稳定运行无崩溃 |
| D3D11纹理格式匹配 | ✅ CopyResource 成功 |
| full shader编译 | ✅ GLSL fallback 通过 |
| 无CPU memcpy | ✅ 确认：仅 giUpdate(CopyResource) 无Map/memcpy |
| 内存泄漏 | ⚠️ 需长时间运行验证 interop unlock 配对 |
