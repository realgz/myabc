# 00 · 工具链与构建落地计划

- 状态：生效
- 更新日期：2026-09-09
- 前置：无（这是所有里程碑的地基）
- 关联决策：docs/decisions/project-setup/20260909-cpp-cmake-split-toolchain.md
- 关联调研：docs/reference/windows-ime-framework-reference.md 第3-4节

## 1. 背景与目标

问题定位：libpinyin 依赖 glib-2.0 + POSIX + 一个 DB 后端，上游只有 autotools/CMake，
2024-2025 的 Windows 移植补丁全部针对 MinGW-w64 UCRT64，无 MSVC 支持证据；而 TSF TIP 必须是
MSVC 构建的 COM DLL。需要一条可实机复现的构建路径把两边接起来。

完成后可观察效果：
- 一条命令从干净机器拉起全部依赖并构建出 myabc-tip.dll(x86+x64)、myabc-deployer.exe、myabc-engine.exe。
- myabc-engine.exe --selftest 能加载 libpinyin 系统模型并把 nihao 转成含"你好"的候选。
- CI 脚本 tests/review/run_all.sh 全绿。

## 2. 工具链决策（拆分工具链）

| 侧 | 工具链 | 依赖来源 |
|---|---|---|
| TIP DLL / 候选窗 / 部署工具 | MSVC v143（VS 2022 17.14）+ Windows SDK 10.0.26100 + CMake | vcpkg manifest 模式（仅 Win32/WIL/纯技术库，无 glib） |
| 引擎进程 myabc-engine.exe | MSYS2 MinGW-w64 UCRT64 工具链（gcc + CMake + ninja） | MSYS2 pacman：glib2、db（Berkeley DB）；libpinyin 从源码构建 |

两侧完全独立构建，产物在 build/msvc/ 与 build/mingw/，由顶层 CMakePresets.json 分别驱动，
最后 scripts/stage.ps1 汇总到 out/<config>/（含 MinGW 运行时 DLL）。

为什么不强求单工具链：见 project-setup ADR。核心：字节级 IPC 边界让 ABI 不混合，
用构建体系强制薄 DLL，比跟 libpinyin 的 MSVC 移植死磕成本低、风险可控。

## 3. 依赖获取与验证步骤（命令级，执行方逐条执行并记录输出）

### 3.1 CMake（本机 CMake 不在 PATH）

决策：不要求用户单独装 CMake，使用 VS 自带的。在脚本里定位（PowerShell）：

    $vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
    $cmake = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    $ninja = Join-Path $vs "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    & $cmake --version    # 期望 >= 3.25（VS 2022 17.14 自带约 3.29+）

把该路径写进 scripts/env.ps1 的 MyabcCMake 变量（配置化，不散落）。
若版本 < 3.25（CMakePresets v6 需要），回退：winget install Kitware.CMake 并在 env.ps1 切换。

### 3.2 vcpkg（MSVC 侧依赖）

决策：manifest 模式 + git submodule。理由：可复现、随仓库版本锁定、CI 免全局安装。

    git submodule add https://github.com/microsoft/vcpkg.git third_party/vcpkg
    .\third_party\vcpkg\bootstrap-vcpkg.bat -disableMetrics

仓库根 vcpkg.json（manifest）：dependencies 目前只列 wil（Windows Implementation Library，
COM/RAII 辅助，MIT），builtin-baseline 填 bootstrap 后 third_party/vcpkg 的 commit sha。
不要在 MSVC 侧依赖 glib / libpinyin。若后续候选窗要 JSON，用 header-only。
验证：cmake --preset x64-debug 配置阶段 vcpkg 自动装 wil，无报错。

### 3.3 MSYS2 + MinGW-w64 UCRT64（引擎侧）

    winget install MSYS2.MSYS2          # 或 https://www.msys2.org/ ，装到 C:\msys64
    C:\msys64\usr\bin\bash.exe -lc "pacman -Syu --noconfirm"   # 可能需跑两次
    C:\msys64\usr\bin\bash.exe -lc "pacman -S --noconfirm --needed \
      mingw-w64-ucrt-x86_64-toolchain \
      mingw-w64-ucrt-x86_64-cmake \
      mingw-w64-ucrt-x86_64-ninja \
      mingw-w64-ucrt-x86_64-pkgconf \
      mingw-w64-ucrt-x86_64-glib2 \
      mingw-w64-ucrt-x86_64-db"

已核实（2026-09-09，packages.msys2.org）：mingw-w64-ucrt-x86_64-glib2 与
mingw-w64-ucrt-x86_64-db（Berkeley DB）存在预编译包；mingw-w64-ucrt-x86_64-libpinyin
与 mingw-w64-ucrt-x86_64-kyotocabinet 不存在预编译包（libpinyin 需自建）。

验证 glib 可被 pkg-config 找到（libpinyin 的 FindGLIB2.cmake 依赖它）：

    C:\msys64\ucrt64\bin\pkg-config.exe --modversion glib-2.0    # 期望 2.80+
    C:\msys64\ucrt64\bin\pkg-config.exe --cflags --libs glib-2.0

### 3.4 libpinyin 从源码构建为 DLL

在 MSYS2 UCRT64 shell 内（C:\msys64\ucrt64.exe）：

    cd /e/work/myabc/third_party
    git clone https://github.com/libpinyin/libpinyin.git
    cd libpinyin
    git checkout <锁定一个 tag，如 2.11.92 或已验证的 commit>
    git submodule update --init          # data 子模块（词库源）
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DBUILD_UTILS=ON
    cmake --build build -j

已核实 libpinyin/master 的 CMakeLists.txt：cmake_minimum_required(VERSION 3.5)；
find_package(GLIB2 REQUIRED)；先 find_package(BerkeleyDB)，找不到才 find_package(KyotoCabinet REQUIRED)；
有 if(WIN32) 分支调整库前缀；Debug 构建会 add_definitions(-O0 -g3)（GCC flag，故必须用
Release 或 RelWithDebInfo，不要 Debug）。

风险点与应对（构建期逐一确认）：
1. find_package(GLIB2) 失败 -> 传 -DCMAKE_PREFIX_PATH=/ucrt64 或
   PKG_CONFIG_PATH=/ucrt64/lib/pkgconfig；必要时 -DGLIB2_INCLUDE_DIR / -DGLIB2_LIBRARIES 显式给。
2. Berkeley DB 头/库找不到 -> -DDB_INCLUDE_DIR=/ucrt64/include -DDB_LIBRARIES=/ucrt64/lib/libdb.dll.a。
3. 编译报 POSIX 缺失（fsync/getline/O_BINARY 等）-> 确认 checkout 的版本已含上游 PR
   #184/#186/#187/#188 修复；若旧 tag 报这些错，改用 master 最新 commit（在 debt-log 记版本）。
4. data/ 子目录构建需要 Python + 网络下载 model.text（数十 MB）；若卡在数据生成，
   先 -DBUILD_UTILS=OFF 只出 libpinyin DLL，词库二进制改用 ibus-libpinyin 发行版或另跑 utils（06 计划细化）。

产物：build/src/libpinyin.dll（或带前缀）、libpinyin.dll.a（导入库）、src/*.h 头。
连同 MinGW 运行时（用 ldd build/src/libpinyin.dll 列全：libglib-2.0-0.dll、libstdc++-6.dll、
libwinpthread-1.dll、libgcc_s_seh-1.dll、libintl-8.dll、libpcre2-8-0.dll、libiconv-2.dll、libdb-*.dll 等）
拷进 out/<config>/engine/。

降低运行时 DLL 数：引擎 exe 自身用 -static-libgcc -static-libstdc++ 并静态链 winpthread，
只剩 glib 系列 DLL 需随附。

### 3.5 引擎 selftest 验证

引擎实现 --selftest 子命令：pinyin_init(model_dir, user_dir) -> 解析 nihao ->
pinyin_guess_sentence -> 打印候选。判据：

    myabc-engine.exe --selftest --model-dir out/Release/data
    # 期望 stdout 含： sentence[0] = 你好
    # 退出码 0

## 4. 顶层 CMake 结构草案

    myabc/
      CMakeLists.txt                # 顶层，project(myabc CXX C)，按 MYABC_TOOLCHAIN 分派子目录
      CMakePresets.json
      vcpkg.json
      cmake/
        myabc-common.cmake          # 公共 warning 级别、C++20、SPDX 检查钩子
        FindLibPinyin.cmake         # 供 MinGW 侧定位自建的 libpinyin
      src/
        config/CMakeLists.txt       # INTERFACE 库 myabc::config
        shared/ipc-protocol/CMakeLists.txt   # STATIC，两侧都编
        domains/tsf-service/CMakeLists.txt   # MSVC only：add_library(myabc-tip SHARED ...)
        domains/candidate-ui/CMakeLists.txt  # MSVC only
        domains/deployment/CMakeLists.txt    # MSVC only
        domains/input-engine/CMakeLists.txt  # MinGW only
        domains/user-dict/CMakeLists.txt     # MinGW only
      third_party/vcpkg/       (submodule)
      third_party/libpinyin/  (submodule 或 clone)

顶层 CMakeLists.txt 用 if(MYABC_TOOLCHAIN STREQUAL "mingw") / "msvc" 分派要构建的子目录，
不尝试一次配置同时构建两侧。

### 4.1 CMakePresets.json 草案（x86 + x64 双目标）

- version 6，cmakeMinimumRequired 3.25。
- configurePresets：
  - msvc-base（hidden，generator Ninja，binaryDir build/msvc/<presetName>，
    cacheVariables：MYABC_TOOLCHAIN=msvc、CMAKE_TOOLCHAIN_FILE 指向 third_party/vcpkg/scripts/buildsystems/vcpkg.cmake、CMAKE_CXX_STANDARD=20）
  - x64-debug   inherits msvc-base，architecture x64/external，CMAKE_BUILD_TYPE=Debug，VCPKG_TARGET_TRIPLET=x64-windows
  - x64-release inherits msvc-base，architecture x64/external，CMAKE_BUILD_TYPE=RelWithDebInfo，triplet x64-windows
  - x86-release inherits msvc-base，architecture x86/external，CMAKE_BUILD_TYPE=RelWithDebInfo，triplet x86-windows
  - mingw-engine generator Ninja，binaryDir build/mingw/engine，
    cacheVariables：MYABC_TOOLCHAIN=mingw、CMAKE_BUILD_TYPE=Release、LibPinyin_ROOT=third_party/libpinyin/build
- buildPresets 一一对应。

MSVC preset 由 scripts/build.ps1 在对应的 vcvarsall.bat x64|x86 环境里调用。
MinGW preset 由 scripts/build-engine.sh 在 C:\msys64\ucrt64.exe 环境里调用。

## 5. 脚本约定（放 scripts/，不放 tests/）

- scripts/env.ps1：定位 cmake/ninja/msys2/vcvars，导出变量。
- scripts/setup-deps.ps1：submodule init、bootstrap vcpkg、pacman 装 MSYS2 包、构建 libpinyin。
- scripts/build.ps1 [-Config Release] [-Arch x64,x86]：构建 MSVC 侧。
- scripts/build-engine.sh：MSYS2 内构建引擎。
- scripts/stage.ps1：汇总产物 + ldd 收集 DLL 到 out/。
- scripts/register.ps1 [-Unregister]：调 myabc-deployer.exe。

## 6. Plan B —— 触发判据与替代路线

Plan A（默认）：MSYS2 UCRT64 构建 libpinyin + 引擎进程，MSVC 构建 DLL，命名管道 IPC。

触发 Plan B 的判据（满足任一）：
1. 投入 <= 3 个工作日，仍无法让锁定版本 libpinyin 在 UCRT64 编译+链接出可运行的
   myabc-engine.exe 并让 3.5 selftest 通过。
2. 引擎冷启动 > 800ms 或满模型常驻内存 > 250MB，惰性加载/裁剪词库后仍不达标。
3. MinGW 运行时 DLL 与引擎进程内其它组件（或 AV/加载器）冲突，静态链 libgcc/libstdc++/winpthread
   后仍无法解决，耗时 > 1 天。
4. glib 主循环/线程模型与自研 IPC 服务端冲突，调和成本 > 1 天。

Plan B（librime 替代）：改用 librime（BSD-3，Windows/MSVC 构建成熟，Weasel 背书）。
- 引擎进程改为 MSVC 构建（librime 从源码或 Weasel 构建脚本）。
- IPC 边界、领域划分、候选协议全部不变（这正是解耦架构的价值）。
- 方案用 rime luna_pinyin + 句子模式；简拼/混拼靠 rime 的 speller/translator 配置。
- 代价：失去 libpinyin 面向智能ABC式整句的特定模型；收益：单工具链、依赖成熟。
- 落地时在 docs/decisions/pinyin-engine/ 追加已废弃小节 + 新 ADR。

Plan C（仅当 A、B 都受阻）：为 libpinyin + glib 写 vcpkg overlay port，全 MSVC 构建。
风险最高（要移植 POSIX 调用），只在被逼到墙角时做。

## 7. 不改动清单

- 不修改 third_party/libpinyin/ 源码（除非必须打补丁；打补丁则以 patches/*.patch 形式保存并在
  debt-log 登记，不直接改 submodule 工作树）。
- 不把 vcpkg 依赖列表扩展到 glib/libpinyin/gtk 系列。
- 不在 MSVC 侧 CMake 引用 MinGW 产物的 C++ 符号（只有引擎 exe 是产物，DLL 不链接它）。

## 8. M0 之前的验收判据

| # | 命令 | 期望 |
|---|---|---|
| B1 | cmake --version（VS 自带） | >= 3.25 |
| B2 | pkg-config --modversion glib-2.0（UCRT64） | 打印版本号，退出 0 |
| B3 | cmake --build third_party/libpinyin/build（UCRT64 Release） | 生成 libpinyin DLL + 导入库，退出 0 |
| B4 | ldd .../libpinyin.dll | 列出的 DLL 均可在 /ucrt64/bin 找到 |
| B5 | cmake --preset x64-release | vcpkg 装 wil 成功，配置无错 |
