# 语言/构建：C++ / CMake，MSVC 与 MinGW 拆分工具链

- 日期：2026-09-09
- 领域：project-setup
- 状态：生效中
- 触发来源：用户对话（2026-09-09）+ deep-planner 调研

## 背景
本机工具链（2026-09-09 检测）：VS Community 2022 17.14（MSVC v143）、Windows SDK 10.0.26100、
git 2.48、Python 3.12 均就绪；CMake 未在 PATH（VS 自带）；vcpkg 未安装。
TSF TIP 必须是 MSVC COM DLL；libpinyin 实际只在 MinGW-w64 UCRT64 可靠构建（见 pinyin-engine ADR）。

## 决策内容
1. 语言 **C++20**，构建系统 **CMake + CMakePresets.json**（version 6）。
2. **拆分工具链**：
   - MSVC v143 侧：myabc-tip.dll（x86+x64）、myabc-ui.exe、myabc-deployer.exe；依赖用 vcpkg
     **manifest 模式**（vcpkg.json）+ **git submodule**（third_party/vcpkg），目前仅 WIL。
   - MinGW-w64 UCRT64 侧（MSYS2）：myabc-engine.exe；依赖用 pacman（glib2、db）+ 源码构建 libpinyin。
   - 两侧独立配置/构建，产物由 scripts/stage.ps1 汇总到 out/。
3. **CMake 用 VS 自带的**（scripts/env.ps1 用 vswhere 定位
   Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe），不要求用户单独安装；
   仅当版本 < 3.25 才 winget 装 Kitware.CMake。
4. 顶层 CMakeLists 按 cache 变量 MYABC_TOOLCHAIN=msvc|mingw 分派子目录，不同时构建两侧。
5. 双架构：x64 全量；x86 仅 TIP DLL + deployer（引擎保持 x64 单一，IPC 架构无关）。

## 被否决的备选方案
- **全 MSVC 单工具链**：需让 libpinyin 在 MSVC 编译，POSIX 依赖移植风险高。降为 Plan C。
- **全 MinGW 单工具链**（TSF DLL 也用 MinGW）：TSF/COM + WIL/ATL 在 MinGW 支持差，
  与系统 msctf 交互、AppContainer 加载、调试体验都不如 MSVC。否决。
- **vcpkg classic 模式 / 全局安装**：不可复现、CI 不友好、版本漂移。否决。
- **vcpkg 作为独立 clone（非 submodule）**：版本不随仓库锁定。否决。
- **Meson / 纯 MSBuild / Bazel**：Meson 对 MSVC COM 项目生态弱；MSBuild 手写 .vcxproj 难跨双工具链
  统一；Bazel 对 Windows C++/COM 过重。CMake 是 TSF 开源实现（SampleIME 除外）与 libpinyin 的共同语言。否决。
- **要求用户单独装 CMake**：VS 已自带可用版本，增加安装负担无收益。否决（除非版本过低）。

## 影响范围
- 仓库根：CMakeLists.txt、CMakePresets.json、vcpkg.json、cmake/、scripts/、.gitmodules
- 所有 src/domains/*/CMakeLists.txt
- CI（tests/review/run_all）
- docs/plan/00 全部

## 关联
- 构建计划：docs/plan/00-toolchain-and-build-plan.md
- 引擎工具链依据：docs/decisions/pinyin-engine/20260909-adopt-libpinyin-mingw-engine-process.md
- 调研：docs/reference/windows-ime-framework-reference.md §4
