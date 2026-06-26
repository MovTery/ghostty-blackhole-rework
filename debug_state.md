 # Debug State — Blackhole Desktop Pet v2
 
 ## 完成状态
 
 ✅ **三种模式 + 桌面叠加窗口 + 空闲检测**
 
 ### 架构
 ```
 VS Code → tasks.json（选择模式）
               ↓
          blackhole.exe [mode]
               ↓
          main.cpp（模式解析）
               ├── always → 桌面叠加窗口 + 渲染
               ├── idle   → GetLastInputInfo 检测空闲
               └── off    → 直接退出
               ↓
          shaders/frag_simple.glsl（简化版黑洞着色器）
 ```
 
 ## 模式说明
 
 | 模式 | 命令 | 行为 |
|------|------|------|
 | **常驻** | `blackhole.exe always` | 桌面叠加，始终显示，点击穿透 |
 | **待机** | `blackhole.exe idle 300` | 空闲 5 分钟显示，活跃时隐藏 |
 | **关闭** | `blackhole.exe off` | 直接退出 |
 
 ### 桌面叠加特性
 - 无边框窗口
 - `WS_EX_TOPMOST` — 始终置顶
 - `WS_EX_TRANSPARENT` — 鼠标点击穿透
 - `glfwHideWindow` / `glfwShowWindow` — 不销毁重建
 
 ### 空闲检测
 - `GetLastInputInfo` Windows API
 - 默认 300 秒（5 分钟）空闲触发
 - 检测到活动 → `glfwHideWindow` + `Sleep(250)` 省 CPU
 - 检测到空闲 → `glfwShowWindow` + `glfwSetWindowOpacity`
 
 ## VS Code 操作
 
 | 操作 | 方式 |
|------|------|
 | 编译 (F5) | `build-debug` 任务 |
 | 常驻模式运行 | 任务面板 → "Run Always" |
 | 待机模式运行 | 任务面板 → "Run Idle (5min)" |
 | 结束黑洞 | 任务面板 → "Kill Blackhole" |
 
 ## 修改文件
 
 | 文件 | 操作 | 说明 |
|------|------|------|
 | `src/main.cpp` | **重写** | 增加三种模式、空闲检测、桌面叠加 |
 | `shaders/frag_simple.glsl` | 新增 | 简化黑洞着色器 |
 | `.vscode/tasks.json` | 更新 | 添加 Always/Idle/Kill 任务 |
 | `debug_state.md` | 更新 | 本文档 |
 
 ## 风险分析
 - 只新增/修改了项目文件，不影响原始 `blackhole.glsl`
 - 原始 Ghostty 着色器保持不动
 - C++ 代码仅影响 Windows 桌面版路径
 - 已验证编译通过 + 运行正常
