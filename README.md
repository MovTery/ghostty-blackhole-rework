# Black Hole，Windows 桌面黑洞屏保（修改版）

详细信息请查看原项目 [ghostty-blackhole-main](https://github.com/XboxNahida/ghostty-blackhole-main/blob/main/README.md)，我只是觉得原本的项目有点不太行，包括但不限于发行包直接打包进源码里、imgui 不跟窗口，非常割裂、未提供 Actions 构建脚本等问题

项目协议：[MIT](./LICENSE)

## 改动

- 使 imgui 始终跟随窗口变化，隐藏自带的缩放手柄
- 改动构建脚本，静态链接依赖库，直接打包为单个 exe 程序
- 改动部分代码，修复一些设备无法正常渲染黑洞的问题
- 记忆选择的预设配置，不需要每次打开都手动选择预设
- 开机自启动时，跳过配置窗口，直接启动本体程序
- 修复软件进程残留的问题（指退出有残留，以及中途配置后重新启动失败）
- 移除了一大堆该项目不需要的文件
- 打包 GLFW 类库方便编译
