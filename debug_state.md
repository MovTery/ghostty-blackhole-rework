# Debug State — Blackhole v10: 最终状态

## 当前状态
编译 ✅ 零警告零错误 (2026-06-27)
所有视频检测功能正常

## 最终修改文件

### src/main.cpp
| 修改 | 说明 |
|------|------|
| GetProcessName | Unicode API (PROCESSENTRY32W) + UTF-8 输出 + 终止符修复 |
| isVideo 匹配列表 | 扩充中英文进程名：哔哩哔哩/爱奇艺/优酷/芒果/抖音/快手/腾讯视频 + nvidia |
| Method 3 音频匹配 | 统一进程名匹配（去掉 isBrowser 分支），兼容多进程架构 |
| UWP 窗口标题检测 | GetWindowTextW + wcsstr，检测 applicationframehost.exe 壳内媒体应用 |
| UWP 音频检测 | 匹配所有音频会话（非按进程名过滤） |

### CMakeLists.txt
| 修改 | 说明 |
|------|------|
| -fexec-charset=UTF-8 | 中文字符串编译为 UTF-8 |
| -mwindows → link options | 双击 exe 不弹控制台 |

### README.md
| 新增 | 说明 |
|------|------|
| 空闲检测原理 | 三层检测机制流程图 |
| 匹配策略 | 进程名匹配、UWP 特殊处理、编码兼容 |
| 支持的应用表 | 浏览器/国内平台/本地播放器/UWP |
| 桌面壁纸说明 | 壁纸音频不阻止黑洞触发 |
