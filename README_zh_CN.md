# GoDesk

> 一款面向 Windows、Linux、macOS 与 Android 的现代远程设备工作台。

[![平台构建](https://github.com/lieren2012/GoDesk/actions/workflows/godesk-windows.yml/badge.svg)](https://github.com/lieren2012/GoDesk/actions/workflows/godesk-windows.yml)
[![许可证：GPL-3.0](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)

GoDesk 基于 [RabbitRemoteControl](https://github.com/KangLin/RabbitRemoteControl) 重构，保留原项目成熟的多协议和插件架构，逐步改造桌面端的视觉、更新和使用体验。

第一版构建目标是 Windows x64、macOS Intel、macOS Apple Silicon 和 Linux x64。构建、安装包和自动更新链路都放在 GitHub Actions 中完成；Android/手机版暂不纳入这一版。

[English README](README_en.md) · [页面原型](docs/ui/godesk-dashboard.html) · [构建记录](https://github.com/lieren2012/GoDesk/actions)

## 界面预览

![GoDesk 主界面参考](docs/Image/Screenshot/MainWindows.png)

第一版界面采用清爽的浅色工作台、深色导航和蓝色主操作色。连接列表、最近记录、收藏和运行中的会话仍沿用原有功能，后续会逐步替换为 GoDesk 的统一交互。

## 主要能力

- **远程桌面**：VNC / RFB、RDP
- **远程终端**：SSH、Telnet、本地终端和串口
- **文件与网络工具**：FTP、SFTP、局域网唤醒、网页浏览器等
- **多种工作区布局**：标签页、分屏、停靠面板和最近连接
- **插件化架构**：协议和工具可以独立扩展，不把所有功能堆在主窗口里
- **跨平台基础**：Windows、Linux、macOS、Android（Android 版本仍需实机验证）
- **多语言**：默认中文，同时保留英文及其他翻译资源

## 下载与自动更新

Windows 客户端由 GitHub Actions 编译。进入 [Actions](https://github.com/lieren2012/GoDesk/actions) 查看构建状态，成功构建后可在对应的 `nightly` Release 下载测试安装包。

GoDesk 客户端启动后会在后台检查 GitHub Release：

1. 读取当前平台对应的更新清单；
2. 发现新版本后下载安装包并校验 MD5；
3. 下载完成后弹出更新窗口，由用户确认是否启动安装程序。

更新不会静默替换正在运行的程序。正式发布前会继续完善版本号、签名和回滚策略。

## 从源码编译

官方依赖较多，建议直接使用 GitHub Actions 编译 Windows 版本。

### 本地准备

- Visual Studio 2022（MSVC）
- Qt 6.10.x（Windows MSVC 64-bit）
- CMake 3.27 或更高版本
- vcpkg
- RabbitCommon（构建脚本会自动拉取）

### GitHub Actions

推送到 `main` 分支，或在仓库的 Actions 页面手动运行 **GoDesk platform builds** 工作流。工作流会分别编译 Windows x64、macOS Intel/Apple Silicon 和 Linux x64；Windows x64 构建成功后发布 `nightly` 安装包。

### 本地命令（仅用于开发调试）

```powershell
git clone https://github.com/lieren2012/GoDesk.git
cd GoDesk
git submodule update --init --recursive
cmake --preset msvc-x64-qt6
cmake --build build/x64 --config Release
```

本地编译需要自行设置 `QT_ROOT`、`Qt6_DIR` 和 `VCPKG_ROOT`。完整依赖说明见 [Windows 编译文档](docs/Compile/Windows_zh_CN.md)。

## 项目结构

```text
App/Client       Qt 桌面客户端
Plugins/          VNC、RDP、终端及工具插件
Src/              公共协议与连接逻辑
Update/           更新清单和更新配置
.github/workflows GitHub Actions 构建与发布
 docs/ui/         GoDesk 界面原型
```

## 开发计划

- [x] 建立 GoDesk 公共仓库和 GitHub Actions Windows 构建
- [x] 第一版桌面端视觉方案
- [x] GitHub Release 更新检查、下载和用户确认
- [ ] 将 Qt 主窗口全面替换为 GoDesk 信息架构
- [ ] 完善安装包签名、版本渠道和回滚
- [ ] 增加连接导入导出、设备搜索和更清晰的错误提示

## 参与贡献

欢迎提交 Issue 和 Pull Request。涉及协议、插件或安装包改动时，请同时说明：

- 使用的平台和 Qt 版本
- 是否能稳定复现
- 日志中与问题相关的片段
- 预期行为与实际行为

## 许可证

GoDesk 采用 [GNU General Public License v3.0](LICENSE)。原项目及其第三方组件的版权和许可证信息请查看 [Authors.md](Authors.md)、[License.md](License.md) 以及各插件目录中的说明。
