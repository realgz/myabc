# 拼音引擎集成 libpinyin，构建为独立 MinGW 引擎进程

- 日期：2026-09-09
- 领域：pinyin-engine
- 状态：生效中
- 触发来源：用户对话（2026-09-09）+ deep-planner 调研

## 背景
需求要智能整句拼音（句子级最优路径，非逐字选词），并要简拼/混拼/音节切分等经典特性。
候选引擎主要有 libpinyin（统计语言模型，明确面向智能整句，ibus-libpinyin / fcitx5 核心）与
librime（方案驱动，BSD）。
libpinyin 上游以 autotools/Linux 为主，依赖 glib-2.0 + POSIX + 一个 DB 后端；已核实其
CMakeLists.txt（master）：cmake>=3.5、find_package(GLIB2 REQUIRED)、BerkeleyDB 优先否则
KyotoCabinet、有 if(WIN32) 分支、Debug 用 GCC-only flag。2024-2025 上游合并的 Windows 移植补丁
（issue #182/#183、PR #164/#184/#186/#187/#188/#189）全部针对 MinGW-w64 UCRT64，无 MSVC 证据。

## 决策内容
1. 拼音引擎采用 **libpinyin**（用户已确认）。
2. 引擎不与 TSF DLL 同工具链：**整个引擎进程 myabc-engine.exe 用 MSYS2 MinGW-w64 UCRT64 构建**
   （libpinyin 从源码构建为 DLL + glib/DB 用 MSYS2 pacman 包），TSF DLL 用 MSVC 构建且完全不碰
   glib/libpinyin。两者通过命名管道 + 长度前缀 JSON 消息 IPC，字节级边界，ABI 不混合。
3. DB 后端默认 **Berkeley DB**（MSYS2 有 ucrt64 预编译包；libpinyin 默认优先它），
   通过 DbBackend 适配器隔离，可切 KyotoCabinet。
4. 设 Plan B 触发判据（见 docs/plan/00-toolchain-and-build-plan.md §6）：若 libpinyin 在
   UCRT64 三日内不能构建出可用引擎、或启动性能/内存不达标、或运行时 DLL 冲突不可解，
   则改用 librime（MSVC + vcpkg），IPC 边界与领域划分不变。

## 被否决的备选方案
- **libpinyin 直连 MSVC v143**：源码依赖 unistd.h/mmap/getline/fsync，CMake 有 GCC-only flag，
  无上游 MSVC 支持。需移植大量 POSIX 调用，风险高、收益仅"单工具链"。降为 Plan C。
- **librime 作为首选引擎**：Windows/MSVC 构建成熟、Weasel 背书，但方案驱动，"智能ABC 式整句"
  需要额外配置且模型特性与 libpinyin 不同；用户已明确选 libpinyin。保留为 Plan B。
- **自研 / Python 引擎（如接 pypinyin + 自训 n-gram）**：整句质量与工程量不可控，Python 运行时
  注入引擎进程也无必要。否决。
- **Rust 重写引擎**：无现成等价整句模型库，等于自研，超出 MVP 范围。否决。
- **把 libpinyin 静态链进 TSF DLL**：违反"薄 DLL"不变量（每个宿主进程注入 glib + 数百 MB 词库
  + MinGW 运行时），reference §1 明确禁止。否决。

## 影响范围
- src/domains/input-engine/（全部）
- src/domains/user-dict/（libpinyin 用户库）
- docs/plan/00（构建路径、Plan B）
- 构建工具链：新增 MSYS2 依赖（docs/decisions/project-setup ADR）
- License：libpinyin GPLv3 -> 整体 GPLv3

## 关联
- 调研：docs/reference/windows-ime-framework-reference.md §3、§5
- 构建计划：docs/plan/00-toolchain-and-build-plan.md
- 架构：docs/architecture/system-overview.md §2、§4
- libpinyin CMakeLists 核实：https://github.com/libpinyin/libpinyin/blob/master/CMakeLists.txt
