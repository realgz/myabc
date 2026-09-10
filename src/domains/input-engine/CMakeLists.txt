# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/domains/input-engine --- myabc-engine.exe（引擎进程，MinGW UCRT64）
#
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
#       docs/plan/00-toolchain-and-build-plan.md §3.4（-static-libgcc/-static-libstdc++）
#       docs/architecture/system-overview.md §7 不变量 6（跨边界只走字节协议）

if(NOT MYABC_TOOLCHAIN STREQUAL "mingw")
    message(FATAL_ERROR "input-engine 仅在 MinGW 侧构建（MYABC_TOOLCHAIN=mingw）。")
endif()

add_executable(myabc-engine
    logic/engine_main.cpp
    logic/dispatcher.cpp
    backend/pipe_server.cpp
    backend/single_instance.cpp
)

target_include_directories(myabc-engine PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/backend"
    "${CMAKE_CURRENT_SOURCE_DIR}/logic"
)

target_link_libraries(myabc-engine PRIVATE
    myabc::ipc
    myabc::config
    advapi32          # ConvertSidToStringSid / OpenProcessToken
)

myabc_set_warnings(myabc-engine)

# 降低运行时 DLL 依赖：静态链 libgcc / libstdc++ / winpthread（plan 00 §3.4）。
target_link_options(myabc-engine PRIVATE
    -static-libgcc -static-libstdc++
    -Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive,-Bdynamic
)

# M1 起：libpinyin 只由本 target 链接（不变量 1：TIP DLL 永不链 libpinyin）。
# target_link_libraries(myabc-engine PRIVATE libpinyin)  # TODO(M1)
