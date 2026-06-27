# Debug State — Blackhole v10: B站视频播放误触发问题修复

## 问题描述
- **症状**: B站播放视频时（客户端/网页、窗口/全屏），黑洞屏保仍然会触发
- **预期**: 播放视频时应视为"活跃"，不应触发屏保

## 根因分析

### Bug 1: `GetProcessName()` 字符串未正确终止 ✅ 已修复
`src/main.cpp:267` — 循环复制进程名后只在 `out[maxLen-1]` 写 `\0`，数据末尾保留了栈上随机垃圾。导致 `strcmp(spname, pname)` 永远返回非零，Method 3 音频检测失效。

### Bug 2: 不支持中文进程名 ✅ 已修复
`PROCESSENTRY32`（ANSI版）获取的进程名使用系统代码页编码，中文 `"哔哩哔哩.exe"` 的 GBK 字节无法被 ASCII `strstr("bilibili")` 匹配。

## 修改文件

### `src/main.cpp` — 2处修改

**修改1: `GetProcessName` (line 267)**
- 改用 `PROCESSENTRY32W` + `Process32FirstW/NextW`（Unicode API）
- 使用 `WideCharToMultiByte(CP_UTF8, ...)` 转换为 UTF-8
- 仅对 ASCII 字节（< 0x80）做 `tolower`，保护 UTF-8 多字节序列
- 正确在数据末尾写 `\0`

**修改2: `isVideo` 进程名检测 (line 314)**
新增以下匹配模式：
| 平台 | 新增模式 |
|------|----------|
| B站 | `哔哩哔哩`, `bili` |
| 爱奇艺 | `iqiyi`, `爱奇艺` |
| 优酷 | `youku`, `优酷` |
| 芒果TV | `mgtv`, `芒果` |
| 抖音 | `douyin`, `抖音` |
| 快手 | `kuaishou`, `快手` |
| 腾讯视频 | `腾讯视频`, `qqlive` |

## 修改函数
- `GetProcessName()` — 改用 Unicode API + UTF-8 输出
- `isWatchingVideo()` — 扩充进程名匹配模式

## 编译检查
✅ 零警告零错误（MSYS2 MinGW GCC）

## 风险分析
- **影响模块**: 空闲检测核心逻辑
- **潜在风险**: 
  - UTF-8 转换使用了 `CP_UTF8`，需确认目标系统支持
  - `tolower` 仅应用于 ASCII 字节，多字节 UTF-8 字符保持原样
- **需要验证**: 
  - B站桌面客户端播放视频时屏保不触发
  - B站网页版（Chrome/Edge）窗口/全屏播放视频时屏保不触发
  - 桌面壁纸播放音频时屏保正常触发（不误判）
  - 其他视频客户端（腾讯视频、爱奇艺等）正常检测
