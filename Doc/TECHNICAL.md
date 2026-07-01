# Black Hole 技术文档

> 本文档记录 blackhole.exe 的详细技术实现，包括设计决策、已知问题和实验记录。
> 快速上手指南请参见 [README.md](README.md)。

---

## 一、空闲检测原理

程序使用**三层检测机制**判断用户是否在观看视频/游戏：

### 检测流程

```
每 1 秒执行一次
├── Method 1: D3D 独占全屏检测
│   └── SHQueryUserNotificationState → 检测游戏/全屏独占应用
├── Method 2: 窗口覆盖整个屏幕
│   └── 前景窗口尺寸 ≥ 屏幕分辨率 → 全屏播放
└── Method 3: 音频会话检测
    ├── 获取前景窗口进程名
    ├── 匹配已知视频应用列表（中/英文进程名）
    ├── 对 UWP 应用（电影和电视等）检测窗口标题
    └── 枚举 Windows 音频会话，比对进程名 + 音频峰值
```

### 匹配策略

- **进程名匹配**：所有视频应用统一使用进程名比对（而非 PID），兼容浏览器、Electron 等多进程架构
- **UWP 特殊处理**：`ApplicationFrameHost.exe` 是 UWP 应用的外壳进程，程序会读取窗口标题识别媒体播放器，并检测**所有**音频会话
- **编码兼容**：进程名使用 UTF-8 统一编码，支持中英文混用场景

### 支持的应用

| 类别 | 应用 |
|------|------|
| **浏览器** | Chrome, Edge, Firefox, Opera, Brave |
| **游戏启动器** | Steam, Epic Games, Ubisoft Connect, EA App, Battle.net, Riot, GOG, Xbox, Game Bar |
| **国内视频平台** | 哔哩哔哩, 爱奇艺, 优酷, 芒果TV, 抖音, 快手, 腾讯视频 |
| **本地播放器** | VLC, MPV, PotPlayer, MPC, Windows Media Player, NVIDIA Overlay |
| **UWP 播放器** | 电影和电视, 媒体播放器（窗口标题关键词检测） |

### 桌面壁纸音频

壁纸引擎（如 Wallpaper Engine）播放的音频**不会阻止黑洞触发**——程序通过窗口/进程名过滤，仅前景视频应用才计入活跃状态。

---

## 二、双进程架构设计

```
blackhole.exe             配置器 + 空闲监控器（托盘图标）
blackhole.exe --render    黑洞渲染器（由监控器自动启停）
```

### 为什么这样设计？

Windows 11 的 DWM 会对全屏透明窗口施加**强调色边框**（黄边框），且 show/hide 窗口会触发 DWM 重建缓存。经过多轮迭代，发现以下方案均存在缺陷：

| 尝试方案 | 问题 |
|----------|------|
| opacity=0 隐藏 | 破坏 swap chain，导致黑屏 |
| ShowWindow 隐藏 | 触发 DWM 黄色 accent 边框 |
| 缩成 1×1 像素 | 仍有 DWM 合成开销 |
| WM hook 拦截 | 无法阻止 DWM 的缓存 accent layer |

**最终方案：进程级隔离**
- 渲染器启动即创建窗口（带 DWM 防护：`DWMWA_BORDER_COLOR=0` + `DWMNCRP_DISABLED` + `WS_EX_NOREDIRECTIONBITMAP` + `DwmFlush`）
- 活跃时**直接终止渲染器进程**（优雅 `WM_CLOSE` 退出）
- 空闲时**重新启动渲染器进程**（全新 GL 上下文 + 全新 DWM 状态）
- 每次启动使用**唯一窗口标题**防止 DWM 缓存匹配

---

## 三、窗口层级保护

黑洞作为桌面覆盖层，窗口 Z 序分层经历了多次迭代才稳定。

### 最终方案：WS_EX_TOPMOST 扩展样式

```cpp
// win32_gl.cpp — 窗口创建
DWORD exStyle = WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW |
                WS_EX_TRANSPARENT | WS_EX_LAYERED;

// 初始化时一次置顶即可，无需运行时轮询
SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
```

- **WS_EX_NOACTIVATE** — 永不抢焦点
- **WS_EX_TOPMOST** — DWM 合成层最高 Z 序，一锤定音
- **WS_EX_TOOLWINDOW** — 不出现在任务栏 / Alt+Tab
- **WS_EX_TRANSPARENT** — 鼠标穿透（配合 `WM_NCHITTEST → HTTRANSPARENT`）
- **WS_EX_LAYERED** — DWM 合成层专属窗口

### DWM 防护层

```cpp
DWMWA_BORDER_COLOR = 0           // accent 边框透明
DWMNCRP_DISABLED                 // 禁用非客户区渲染
WS_EX_NOREDIRECTIONBITMAP        // 阻止 DWM 创建重定向表面
DwmEnableBlurBehindWindow(FALSE) // 清除合成器缓存
DwmFlush()                       // 强制刷新合成器
WM_NCCALCSIZE return 0           // 移除整个非客户区
WDA_EXCLUDEFROMCAPTURE           // 排除自身被 WGC 捕获（避免反馈循环）
```

---

## 四、桌面捕获：WGC vs DXGI

**WGC 帧可能不完整，但持续更新。DXGI 帧完整，但生命周期敏感。**

| 维度 | WGC | DXGI Duplication |
|------|-----|------------------|
| 帧来源 | DWM 合成快照 | GPU backbuffer 拷贝 |
| 帧完整性 | 大画面变动时可能拿到半帧 | 每帧完整 |
| 帧稳定性 | 持续输出，不卡死 | 配对错误则全部失效 |
| 光标 | 默认捕获（可关） | 不捕获 |
| 多 GPU | 支持 | 需同 GPU |
| DRM 内容 | 可捕获 | 可能黑屏 |
| 全屏独占程序 | 可捕获 | 可能失败 |
| 延迟 | 有 DWM 合成延迟 | 低延迟 |
| API 复杂度 | WinRT（高） | COM（低） |
| 恢复机制 | 自动 | 需手动重建 |

### WGC 黄边框抑制

Win11 会对屏幕捕获绘制黄色强调色边框。通过 `IGraphicsCaptureSession3::put_IsBorderRequired(false)` 通知 DWM 不要绘制边框（Win11 22H2+）。

```cpp
// capture_wgc.cpp — StartCapture() 后
IGraphicsCaptureSession3* sess3 = nullptr;
sess->QueryInterface(IID_IGCS3, (void**)&sess3);
if (sess3) {
    sess3->put_IsBorderRequired(false);
    sess3->Release();
}
```

> ⚠️ 非官方关闭开关，Windows 在某些场景下仍可能显示边框。

### 本项目实测

| 测试项 | WGC | DXGI |
|--------|-----|------|
| 编译通过 | ✅ | ✅ |
| 启动捕获 | ✅ | ✅ |
| 画面持续刷新 | ✅ | ❌ INVALID_CALL |
| 黑洞效果正常 | ✅ | ❌ 背景冻结 |
| 切应用无闪烁 | ⚠️ 偶发轻微 | 未测 |
| 高帧率 165Hz | ✅ | 未测 |

DXGI 失败原因是 `ReleaseFrame / AcquireNextFrame` 配对极其严格——循环前未预取第一帧导致首个 `ReleaseFrame` 无对应帧，返回 `INVALID_CALL`，后续全部失效。当前项目默认使用 **WGC**，DXGI 代码保留在 `src/capture_dxgi.cpp` 中。

---

## 五、D3D11 渲染路径实验

尝试过将渲染栈从 OpenGL+WGL 迁移到原生 D3D11（WGC 输出本身就是 `ID3D11Texture2D`，无需 CPU 往返）。

### 已完整实现的模块

- `src/d3d11_renderer.cpp/h` — D3D11 渲染器（SwapChain、Shader 编译、ConstantBuffer、全屏四边形）
- `src/win32_window.cpp/h` — 纯 Win32 窗口（无 WGL/D3D11 绑定，职责分离）
- `shaders/blackhole.hlsl` — GLSL→HLSL 精确翻译（Schwarzschild 光线积分、吸积盘、星空）
- `shaders/fullscreen_vs.hlsl` — 全屏顶点着色器
- `src/renderer_interface.h` — `IRenderer` 抽象接口（OpenGL/D3D11 双实现路径）
- `src/texture_source.h` — 纹理源抽象接口（Desktop/Video/Image/Camera 可扩展）

### 遇到的核心问题

```
WGC 帧池 (3 texture 轮换)
        ↓
CopyResource (GPU 异步)
        ↓
Pixel Shader 采样
        ↓
Present
```

**根因**：WGC 内部对 3 个 D3D11 纹理做池化复用。GPU 异步管线下 `CopyResource` 未完成时 WGC 可能复用同一纹理，导致 shader 采样到旧帧（画面冻结/残影叠加）。

### 尝试过的修复

| 尝试 | 结果 |
|------|------|
| 帧指针去重跳过 | 主动丢弃新帧，WGC 队列冻结 |
| GPU Query fence 同步 | 伪同步，CPU busy-wait 破坏 WGC 采集节奏 |
| 帧缓冲队列 (2~3 buffer 延迟) | 理论上正确，但 WGC/DWM 纹理生命周期不保证帧连续性 |
| SRV 每帧重建 + 延迟 Release | GPU 仍在读旧内存时被 CPU 释放，资源竞争 |

### 结论

D3D11+WGC 组合在当前 Windows/WGC 版本下存在架构级阻抗：WGC 返回的是 DWM 合成快照的弱绑定 GPU 资源，不适合作为连续视频流纹理源。OpenGL+WGL 路径的 `Staging → Map → glTexSubImage2D` 虽然多一次 CPU 往返，但天然隔离了 GPU 异步竞争问题。

D3D11 代码完整保留，可通过 `CMakeLists.txt` 取消注释 `target_compile_definitions(blackhole PRIVATE BLACKHOLE_USE_D3D11)` 重新启用。

---

## 六、已知问题

| 问题 | 状态 | 说明 |
|------|------|------|
| 双鼠标 | 待修复 | 正常光标 + 黑洞扭曲后光标同时可见，根因未完全定位 |
| Win11 黄边框 | 已抑制 | `IsBorderRequired(false)` 降低概率，非 100% 消除 |
| 游戏时空闲检测 | 已修复 | 1s 检测 + 无边框全屏窗口提前判断 + 游戏启动器匹配 |
| 被其他窗口遮挡 | 已修复 | `WS_EX_TOPMOST` 扩展样式一锤定音 |

---

## 七、Shader 兼容性改造

为了在 Windows 屏保路径下运行时可调参数，原 shader 中的 `const float` 常量被运行时替换为 uniform 形式：

| 原 shader 写法 | 运行时替换为 |
|----------------|--------------|
| `const float HOLE_RADIUS = X;` | `float HOLE_RADIUS = uHoleRadius > 0.0 ? uHoleRadius : X;` |
| `const float DISK_GAIN = X;` | `float DISK_GAIN = uDiskGain > 0.0 ? uDiskGain : X;` |
| `DISK_TEMP` / `EXPOSURE` / `DRIFT_SPEED` / `STAR_GAIN` / `DISK_INCL` | 同上模式 |
| `const float TOKEN_HOME_X = 0.96;` | `float TOKEN_HOME_X = uHomeX;` |
| `const float TOKEN_HOME_Y = 0.04;` | `float TOKEN_HOME_Y = uHomeY;` |
| `#define SIZE_MODE MODE_TOKENS` | `#define SIZE_MODE MODE_DEMO` |
| `mod(iTime, DEMO_SEC) / DEMO_GROW_SEC` | `iTime / DEMO_GROW_SEC`（去掉时长回卷） |
| `lissa(t * TOKEN_CALM)` | `lissa(t * TOKEN_CALM + uRandPhase)`（随机化轨迹相位） |
| `DiskLook demoLook()` | 替换为读 uniform 数组版本，支持 3 种播放模式 |

> **关键陷阱**：GLSL `const` 必须是编译期常量，不能用 uniform 赋值。`const float TOKEN_HOME_X = uHomeX;` 会编译失败，必须改成 `float TOKEN_HOME_X = uHomeX;`（去掉 const）。

---

## 八、Bug 修复记录

### 1. 系统光标全局隐藏导致退出后异常
- **原方案**：`SetSystemCursor(空光标, OCR_NORMAL)` 全局替换系统箭头
- **问题**：程序异常退出或失焦时光标永久不可见
- **修复**：改用 WGC `IsCursorCaptureEnabled = false`，捕获纹理中不包含系统光标，无需全局隐藏
- **应急兜底**：`Win32GL_Shutdown` 中调用 `SystemParametersInfo(SPI_SETCURSORS, ...)` 重置系统光标

### 2. 渲染窗口被误判为全屏视频导致重启死循环
- **现象**：程序无操作运行一段时间后自动结束，几秒后又重启
- **根因**：`isWatchingVideo` 检测到前景窗口覆盖全屏 → 误判为视频 → 杀死渲染器 → 窗口消失 → 重新检测空闲 → 重启渲染器 → 循环
- **修复**：`isWatchingVideo` 中排除自身窗口（双校验：窗口样式组合 + 类名匹配）

### 3. 程序多实例冲突
- 启动时枚举进程（`CreateToolhelp32Snapshot`），杀掉其他 `blackhole.exe` 实例（`--render` 子进程除外，由 monitor 管理）

---

## 九、Blakhole_UI 相关

详见 [Blakhole_UI/](Blakhole_UI/) 目录和 [Doc/ARCHITECTURE.md](Doc/ARCHITECTURE.md)。

Blakhole_UI 是 Qt6/QML 重写的新版配置面板，替代了原始的 ImGui 配置界面。它通过 `QProcess` 启动 `blackhole.exe --render` 管理黑洞渲染进程，自带 OpenGL FBO 实时预览。