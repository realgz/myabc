# Windows 输入法框架调研（reference）

- 主题：Win10/11 下开发中文输入法的框架选型与可复用实现
- 状态：生效
- 更新日期：2026-09-09
- 来源：联网调研（见文末链接）

## 1. 框架层：TSF vs IMM32

| 方案 | 说明 | Win10/11 适配 |
|---|---|---|
| **TSF**（`msctf.dll`，Windows XP 引入） | 现代 API。输入法 = COM in-proc DLL，注册为 TIP（Text Input Processor / 文本输入处理器） | ✅ 唯一正解；支持 UWP/商店应用与多进程沙箱 |
| IMM32（`imm32.dll`） | 旧 API。Win8 起部分场景有 bug，商店应用不支持 | ❌ 仅作兼容回退，不作主实现 |

**关键约束**：TSF 的 TIP DLL 会被加载进*每一个*有文本输入的应用进程（含 AppContainer 沙箱进程）。
因此重逻辑 + 词库 + 语言模型**必须**放独立引擎进程，DLL 只做薄转发 + 候选窗协调。
Weasel、PIME 均采用此"薄 DLL + 引擎进程 + IPC"架构。

## 2. 可参考的开源 TSF 实现

| 项目 | 语言 | 价值 | License |
|---|---|---|---|
| [rime/weasel（小狼毫）](https://github.com/rime/weasel) | C++ | **工业级参考架构**：`WeaselTSF` / `WeaselServer`+`WeaselIPC` / `WeaselUI` / `WeaselDeployer` / `WeaselSetup`；单包支持 win32/x64/arm/arm64 | GPLv3 |
| [rime/librime](https://github.com/rime/librime) | C++ | Weasel 的引擎；方案(schema)驱动，整句智能组词 | BSD-3 |
| [EasyIME/PIME](https://github.com/EasyIME/PIME) | C++ + Python/Node | 通用 TSF 骨架 + 命名管道后端；`libIME` 是 TSF 的轻封装 | LGPL-2.1 |
| [jrywu/DIME](https://github.com/jrywu/DIME) | C++ | 单体 TSF，桌面+商店应用，`.cin` 码表（拼音/仓颉/五笔） | — |
| [shenmin/cassotis-ime](https://github.com/shenmin/cassotis-ime) | Delphi | 本地神经网络重排序（无 ONNX/PyTorch 依赖）思路可借鉴 | — |
| [microsoft/Windows-classic-samples · Samples/IME](https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME) | C++ | 微软官方 `SampleIME`：TSF TIP 最小骨架、注册、候选窗、字典加载。**注意**：其商店应用支持仅限内置 edit 控件 | MIT |

## 3. 拼音转换引擎

| 引擎 | 说明 | License |
|---|---|---|
| **[libpinyin](https://github.com/libpinyin/libpinyin)** | 基于统计语言模型，明确"面向智能整句拼音输入"。ibus-libpinyin / fcitx5-chinese-addons 的核心 | GPLv3 |
| [librime](https://github.com/rime/librime) | 方案驱动、跨平台、整句智能 | BSD-3 |

### libpinyin 在 Windows/MSVC 的构建风险（需在 plan 阶段落实）
- 传统依赖 **glib-2.0** 和一个 DB 后端（**Berkeley DB** 或 **KyotoCabinet**），历史上以 autotools/Linux 为主。
- 需确认：libpinyin 自带的 CMake 支持是否可用于 MSVC；glib 经 vcpkg 获取；DB 后端选型。
- 备选：若 MSVC 直连困难，考虑 (a) vcpkg 三元组统一依赖，(b) 退回 librime（BSD，Windows 构建成熟，有 Weasel 背书）。

## 4. 本机工具链现状（2026-09-09 检测）

- Visual Studio Community 2022 17.14（MSVC v143）✅
- Windows SDK 10.0.26100 ✅
- git 2.48 ✅ / Python 3.12 ✅
- CMake ❌ 未在 PATH（VS 自带，路径 `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`，或另装）
- vcpkg ❌ 未安装（依赖管理需要，建议作为 submodule 或 manifest 模式）

## 5. License 影响

集成 libpinyin(GPLv3) → 本项目整体须 **GPLv3**（或兼容）。TSF DLL 静态/动态链接 libpinyin 都会传染。
若日后想放宽授权，engine 进程与 DLL 通过 IPC 解耦的架构可降低传染面（engine 进程 GPL，DLL 层协议独立），
但需法律层面确认，plan 阶段先按整体 GPLv3 推进。

## 来源

- https://github.com/rime/weasel
- https://github.com/rime/librime
- https://github.com/EasyIME/PIME
- https://github.com/jrywu/DIME
- https://github.com/shenmin/cassotis-ime
- https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME
- https://github.com/libpinyin/libpinyin
- https://learn.microsoft.com/en-us/windows/win32/tsf/text-services-framework
- https://en.wikipedia.org/wiki/Text_Services_Framework
