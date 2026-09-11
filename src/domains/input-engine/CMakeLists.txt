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
    logic/candidate/source_registry.cpp
    logic/charset/charset_filter.cpp
    logic/session/session.cpp
    logic/session/session_manager.cpp
    backend/pipe_server.cpp
    backend/single_instance.cpp
    backend/libpinyin_wrapper.cpp
    backend/ui_bridge.cpp
    backend/crash_guard.cpp
)
add_library(myabc::engine-core ALIAS myabc-engine-core)

target_include_directories(myabc-engine-core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/backend"
    "${CMAKE_CURRENT_SOURCE_DIR}/logic"
)

target_link_libraries(myabc-engine-core PUBLIC
    myabc::ipc
    myabc::config
    LibPinyin::LibPinyin
    PkgConfig::GLIB2
)
target_link_libraries(myabc-engine-core PRIVATE advapi32 dbghelp)

myabc_set_warnings(myabc-engine-core)

# --- myabc-engine.exe -----------------------------------------------------
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
