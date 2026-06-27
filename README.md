# Black Hole — Windows 桌面黑洞屏保

![demo](demo.gif)

基于 Eric Bruneton 黑洞着色器的 Windows 桌面黑洞可视化程序。捕获桌面画面，实时渲染史瓦西黑洞的引力透镜、吸积盘、光子环等相对论效应。

## 快速开始

1. 双击 elease\blackhole.exe
2. 配置参数 → 点击 **"启动"**
3. 黑洞在**空闲时自动显示**，动鼠标/键盘即消失
4. 右下角托盘图标 → 右键可退出

## 两种模式

| 模式 | 行为 |
|------|------|
| **始终显示** | 黑洞常驻桌面 |
| **空闲检测** | 空闲 N 秒后显示，活跃时自动隐藏 |

空闲时间在配置页面设置（默认 300 秒 / 5 分钟）。
## 空闲检测原理

程序使用**三层检测机制**判断用户是否在观看视频：

### 检测流程

```
每 5 秒执行一次
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
| **国内视频平台** | 哔哩哔哩, 爱奇艺, 优酷, 芒果TV, 抖音, 快手, 腾讯视频 |
| **本地播放器** | VLC, MPV, PotPlayer, MPC, Windows Media Player, NVIDIA Overlay |
| **UWP 播放器** | 电影和电视, 媒体播放器（窗口标题关键词检测） |

### 桌面壁纸音频

壁纸引擎（如 Wallpaper Engine）播放的音频**不会阻止黑洞触发**——程序通过窗口/进程名过滤，仅前景视频应用才计入活跃状态。


## 配置参数

### 黑洞预设
- 14 个可调参数（色温、倾角、旋转、半径、不透明度、多普勒、光束指数、亮度增益、条纹对比度、缠绕紧度、旋转速度、曝光度、星空亮度）
- 全部使用**滑块调节**
- 支持**复制/粘贴**预设、**上移/下移**排序
- 三种**播放模式**：顺序 / 循环 / 随机

### 播放模式
- **顺序播放**：从第 1 个预设播到最后一个
- **循环播放**：播完回到第一个，无限循环
- **随机播放**：每个时段随机抽取预设

## 架构：双进程分离

`
blackhole.exe             配置器 + 空闲监控器（托盘图标）
blackhole.exe --render    黑洞渲染器（由监控器自动启停）
`

### 为什么这样设计？

Windows 11 的 DWM 会对全屏透明窗口施加**强调色边框**（黄边框），且 show/hide 窗口会触发 DWM 重建缓存。经过多轮迭代，发现以下方案均存在缺陷：

| 尝试方案 | 问题 |
|----------|------|
| opacity=0 隐藏 | 破坏 swap chain，导致黑屏 |
| ShowWindow 隐藏 | 触发 DWM 黄色 accent 边框 |
| 缩成 1×1 像素 | 仍有 DWM 合成开销 |
| WM hook 拦截 | 无法阻止 DWM 的缓存 accent layer |

**最终方案：进程级隔离**
- 渲染器启动即创建窗口（带 DWM 防护：DWMWA_BORDER_COLOR=0 + DWMNCRP_DISABLED + WS_EX_NOREDIRECTIONBITMAP + DwmFlush）
- 活跃时**直接终止渲染器进程**（优雅 WM_CLOSE 退出）
- 空闲时**重新启动渲染器进程**（全新 GL 上下文 + 全新 DWM 状态）
- 每次启动使用**唯一窗口标题**防止 DWM 缓存匹配

### DWM 防护层
- DWMWA_BORDER_COLOR = 0 — accent 边框透明
- DWMNCRP_DISABLED — 禁用非客户区渲染
- WS_EX_NOREDIRECTIONBITMAP — 阻止 DWM 创建重定向表面
- DwmEnableBlurBehindWindow(FALSE) — 禁用模糊背景
- DwmFlush() — 清除合成器缓存
- WM_NCCALCSIZE return 0 — 移除整个非客户区

## 文件结构

`
release/
├── blackhole.exe          # 主程序
├── blackhole.glsl         # 黑洞着色器
├── glfw3.dll              # GLFW 运行时
├── libgcc_s_seh-1.dll     # MinGW 运行时
├── libstdc++-6.dll        # MinGW C++ 运行时
└── shaders/
    ├── vert.glsl
    ├── frag_desktop_header.glsl
    ├── frag_header.glsl
    └── frag_simple.glsl
`


## 桌面捕获：WGC vs DXGI

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

### 本项目实测

| 测试项 | WGC | DXGI |
|--------|-----|------|
| 编译通过 | ✅ | ✅ |
| 启动捕获 | ✅ | ✅ |
| 画面持续刷新 | ✅ | ❌ INVALID_CALL |
| 黑洞效果正常 | ✅ | ❌ 背景冻结 |
| 切应用无闪烁 | ⚠️ 偶发轻微 | 未测 |
| 高帧率 165Hz | ✅ | 未测 |

DXGI 失败原因是 `ReleaseFrame / AcquireNextFrame` 配对极其严格——循环前未预取第一帧导致首个 `ReleaseFrame` 无对应帧，返回 `INVALID_CALL`，后续全部失效。当前项目默认使用 **WGC**，DXGI 代码保留在 `capture_dxgi.cpp` 中。

## 技术栈

- **OpenGL 3.3** — 渲染
- **GLFW** — 窗口管理
- **ImGui** — 配置界面
- **Windows Graphics Capture (WGC)** — 桌面捕获
- **DXGI Duplication** — 备用捕获方案
- **MinGW-w64** — 编译工具链
- **CMake** — 构建系统

## 灵感来源

[Eric Bruneton's black hole shader](https://github.com/ebruneton/black_hole_shader) (BSD-3-Clause)

## License

MIT — 见 [LICENSE](LICENSE)
