# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/domains/input-engine --- myabc-engine.exe（引擎进程，MinGW UCRT64）
#
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
#       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.1-3.3
#       docs/plan/00-toolchain-and-build-plan.md §3.4（-static-libgcc/-static-libstdc++）
#       docs/architecture/system-overview.md §7 不变量 1/6

if(NOT MYABC_TOOLCHAIN STREQUAL "mingw")
    message(FATAL_ERROR "input-engine 仅在 MinGW 侧构建（MYABC_TOOLCHAIN=mingw）。")
endif()

# PkgConfig::GLIB2 / LibPinyin::LibPinyin / LibPinyin_* 变量已在顶层 CMakeLists.txt 里
# find_package 过（子目录变量不传播给兄弟目录 tests/unit，见顶层注释）。

# --- myabc-engine-core：除 main() 外的全部逻辑，供 exe 与 tests/unit 的
#     engine_conversion_test 共用（避免重复文件列表）--------------------------
add_library(myabc-engine-core STATIC
    logic/dispatcher.cpp
    logic/candidate/pinyin_candidate_source.cpp
    logic/candidate/english_passthrough_source.cpp
    logic/candidate/punctuation_source.cpp
    logic/candidate/number_currency_source.cpp
    logic/candidate/bihuo_filter.cpp
    logic/candidate/source_registry.cpp
    logic/charset/charset_filter.cpp
    logic/session/session.cpp
    logic/session/session_manager.cpp
    backend/pipe_server.cpp
    backend/single_instance.cpp
    backend/libpinyin_wrapper.cpp
    backend/ui_bridge.cpp
    backend/crash_guard.cpp
    backend/bihuoma_table.cpp
)
add_library(myabc::engine-core ALIAS myabc-engine-core)

target_include_directories(myabc-engine-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/backend"
    "${CMAKE_CURRENT_SOURCE_DIR}/logic"
)

target_link_libraries(myabc-engine-core PUBLIC
    myabc::ipc
    myabc::config
    myabc::cn-number
    LibPinyin::LibPinyin
    PkgConfig::GLIB2
)
target_link_libraries(myabc-engine-core PRIVATE advapi32 dbghelp)

myabc_set_warnings(myabc-engine-core)

# --- myabc-engine.exe -----------------------------------------------------
# DECISION（2026-09-12，"导入更多的 versioninfo"实施中发现的真实冲突）：
# engine.rc（跟其它 3 个可执行文件一样的 VERSIONINFO 资源）特意 NOT 加进这个
# target 的源列表——加了会导致链接失败：
#   ld.exe: .rsrc merge failure: duplicate leaf: type: 10 (VERSION) name: 1 lang: 409
# 根因：下面 target_link_options 里 `-Wl,-Bstatic,--whole-archive -lwinpthread`
# 是 M0 就有的决定（降低运行时 DLL 依赖，见 docs/plan/00-toolchain-and-build-plan.md
# §3.4）——`--whole-archive` 会强制把 libwinpthread.a 里*所有*目标文件都链进来，
# 包括它自带的一个 version.o（自己的 VERSION 资源，name=1 lang=0x409，跟我们自己
# 想加的资源撞了同一个键）。两边都是 name=1（Win32 版本资源 API 按固定 ID=1 查找，
# 不能改），只能改语言 ID 来避开冲突，但那样 GetFileVersionInfo 按语言查找时会
# 出现"不确定选中哪一份"的新问题——不如干脆不碰这条链接选项（M0 的决定不是这次
# 任务要动的范围），myabc-engine.exe 保持不带 VERSIONINFO。
# 影响可接受：myabc-engine.exe 是后台进程，用户从不会在资源管理器里双击它/看它的
# 属性页，四个可执行文件里"最不需要"这份元数据的恰好就是它。engine.rc 文件本身
# 保留在仓库里（连同这条 DECISION），供以后如果 winpthread 静态链方式变了再启用。
add_executable(myabc-engine logic/engine_main.cpp)
target_link_libraries(myabc-engine PRIVATE myabc::engine-core)
myabc_set_warnings(myabc-engine)

# 降低运行时 DLL 依赖：静态链 libgcc / libstdc++ / winpthread（plan 00 §3.4）。
# libpinyin.dll 自身仍带 glib 等运行时 DLL，随 scripts/stage.ps1 一并拷贝。
target_link_options(myabc-engine PRIVATE
    -static-libgcc -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic
)

# 不变量校验（人工 + tests/review）：本 CMakeLists 是 MinGW 侧唯一引用 LibPinyin 的地方；
# tsf-service / deployment / candidate-ui 的 CMakeLists 不得出现 find_package(LibPinyin)。

# 运行 / ctest 都要能找到 libpinyin.dll：复制到与可执行文件同目录。
add_custom_command(TARGET myabc-engine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${LibPinyin_DLL}" "$<TARGET_FILE_DIR:myabc-engine>"
    COMMENT "复制 libpinyin.dll 到 myabc-engine.exe 同目录（ctest/本地运行用）"
)
