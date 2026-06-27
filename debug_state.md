# Debug State — Blackhole v6: 自定义预设数量限制修复

## 当前状态
### 编译 ✓ - 零警告零错误
- 源文件: main.cpp + capture_wgc.cpp + capture_dxgi.cpp + gl_texture.cpp + gui_config.cpp + imgui/*
- 依赖: glfw + opengl32 + d3d11 + dxgi + runtimeobject + dwmapi + comctl32

## 文件结构
| 文件 | 说明 |
|------|------|
| src/main.cpp | 主程序：配置面板 + 渲染循环 |
| src/gui_config.h/cpp | ImGui 配置面板 |
| src/capture_wgc.h/cpp | WGC 捕获 |
| src/capture_dxgi.h/cpp | DXGI 捕获(备用) |
| src/gl_texture.h/cpp | GL 纹理上传 |
| shaders/frag_desktop_header.glsl | Shader 头文件(uniform 声明) |
| blackhole.glsl | Shader 主体 |

---

## 问题修复记录 (2026-06-27)

### 问题
自定义轮播功能已可用，但新增预设(超过8个)无效，最多只能轮播8个。

### 根因分析

原设计使用两个独立的 uniform 控制自定义预设：
- `uUseCustom` (bool) — 是否启用自定义预设
- `uPresetCount` (int, 默认=8) — 预设数量

当这两个 uniform 中任意一个的 `glGetUniformLocation` 返回 -1（shader 编译器可能将其优化掉），对应的 `glUniform*` 调用变为空操作，shader 使用硬编码默认值：
- `uUseCustom` 默认 false → 回退到 DEMO_TOUR(8个硬编码预设)
- `uPresetCount` 默认 8 → 即使 uUseCustom=true，也只循环8个

这就是用户看到的现象：自定义轮播"可用"(能看到黑洞了，因为 SIZE_MODE 已修复)但始终只循环8个预设。

### 修复方案

**去掉 `uUseCustom` uniform，改为用 `uPresetCount > 0` 判断**：
- 减少一个 uniform，消除一个故障点
- C++ 端：勾选自定义轮播时上传 `cfg.presetCount`，否则上传 0
- Shader 端：`uPresetCount > 0` 时走自定义路径，否则走 DEMO_TOUR

### 修改文件

**shaders/frag_desktop_header.glsl:**
- `uniform int uPresetCount = 8;` → `uniform int uPresetCount = 0;`
- 移除 `uniform bool uUseCustom = false;`

**src/main.cpp (4处):**
- buildFragmentShader 注入代码: `if (uUseCustom)` → `if (uPresetCount > 0)`
- 移除 `GLint loc_uCust = gl_GetUniformLocation(program, "uUseCustom");`
- 移除 `gl_Uniform1i(loc_uCust, cfg.useCustomPresets ? 1 : 0);`
- uPresetCount 上传改为: `gl_Uniform1i(loc_uPC, cfg.useCustomPresets ? cfg.presetCount : 0);`

### 编译结果
✓ 零错误零警告

---

## 当前修改状态

| 步骤 | 状态 |
|------|------|
| 修改 shader header | ✅ 完成 |
| 修改 buildFragmentShader 注入代码 | ✅ 完成 |
| 修改 main.cpp 上传逻辑 | ✅ 完成 |
| 编译验证 | ✅ 完成 (零错误零警告) |
