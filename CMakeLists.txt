# SPDX-License-Identifier: GPL-3.0-or-later
#
# myabc 顶层 CMakeLists --- 按 MYABC_TOOLCHAIN 分派要构建的子目录
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §4
#       docs/architecture/system-overview.md §2 / §7
#
# 两套工具链完全独立配置，绝不在一次 configure 里同时构建两侧：
#   MYABC_TOOLCHAIN=msvc   -> TIP DLL / 候选窗 / 部署工具（VS 2022 v143 + vcpkg）
#   MYABC_TOOLCHAIN=mingw  -> 引擎进程 myabc-engine.exe（MSYS2 UCRT64）
#
# 不变量（system-overview §7）：TIP DLL 侧 CMake 不得引用 glib / libpinyin / db。

cmake_minimum_required(VERSION 3.25)

project(myabc
    VERSION 0.0.0
    DESCRIPTION "Smart-ABC-style Chinese pinyin IME for Windows 10/11 (TSF)"
    HOMEPAGE_URL "https://github.com/az/myabc"
    LANGUAGES CXX C
)

if(NOT WIN32)
    message(FATAL_ERROR "myabc 仅面向 Windows 10/11。")
endif()

set(MYABC_TOOLCHAIN "" CACHE STRING "构建侧：msvc | mingw")
set_property(CACHE MYABC_TOOLCHAIN PROPERTY STRINGS "msvc" "mingw")

if(NOT MYABC_TOOLCHAIN)
    if(MSVC)
        set(MYABC_TOOLCHAIN "msvc")
    elseif(MINGW)
        set(MYABC_TOOLCHAIN "mingw")
    else()
        message(FATAL_ERROR "无法推断 MYABC_TOOLCHAIN，请显式传 -DMYABC_TOOLCHAIN=msvc|mingw。")
    endif()
    message(STATUS "MYABC_TOOLCHAIN 未指定，按编译器推断为 '${MYABC_TOOLCHAIN}'。")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(myabc-common)

message(STATUS "myabc: 构建侧 = ${MYABC_TOOLCHAIN}")

# --- 公共层：IPC 协议 + 配置（两侧都编）----------------------------------
# 子目录仅在其 CMakeLists.txt 存在时加入，便于分步落地（plan 01 §3.1-3.2）。
foreach(_common IN ITEMS src/config src/shared/ipc-protocol src/shared/cn-number)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_common}/CMakeLists.txt")
        add_subdirectory(${_common})
    endif()
endforeach()

# --- 按侧分派领域子目录 -------------------------------------------------
if(MYABC_TOOLCHAIN STREQUAL "msvc")
    find_package(wil CONFIG REQUIRED)   # B5：验证 vcpkg manifest 拉起 wil
    # 候选窗鼠标点击跨进程契约（candidate-ui + tsf-service 共用，Windows-only，
    # 不进 myabc-ipc——见 click_bridge_protocol.hpp 文件头 DECISION）。
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/shared/click-bridge-protocol/CMakeLists.txt")
        add_subdirectory(src/shared/click-bridge-protocol)
    endif()
    # 顺序有意义：tsf-service 链接 deployment 的 myabc::deploy-core 和
    # candidate-ui 的 myabc::candidate-ui，两者必须先构建。
    set(_domains
        src/domains/deployment
        src/domains/candidate-ui
        src/domains/tsf-service
    )
elseif(MYABC_TOOLCHAIN STREQUAL "mingw")
    # 在顶层（而非 input-engine 子目录）find_package，让 tests/unit 也能看到结果变量/
    # target——CMake 子目录变量不向兄弟目录传播，GLOBAL 导入 target 才跨目录可见。
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLIB2 REQUIRED IMPORTED_TARGET GLOBAL glib-2.0)
    find_package(LibPinyin REQUIRED)   # M1：input-engine 用；见 cmake/FindLibPinyin.cmake
    set(_domains
        src/domains/input-engine
        src/domains/user-dict
    )
else()
    message(FATAL_ERROR "MYABC_TOOLCHAIN 取值非法：'${MYABC_TOOLCHAIN}'（仅 msvc | mingw）。")
endif()

foreach(_d IN LISTS _domains)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${_d}/CMakeLists.txt")
        add_subdirectory(${_d})
    else()
        message(STATUS "  (跳过未落地的领域目录 ${_d})")
    endif()
endforeach()

# --- 单元测试（默认开；打包构建可 -DMYABC_BUILD_TESTS=OFF）------------------
option(MYABC_BUILD_TESTS "构建 tests/unit 下的单元测试" ON)
if(MYABC_BUILD_TESTS AND EXISTS "${CMAKE_SOURCE_DIR}/tests/unit/CMakeLists.txt")
    enable_testing()
    add_subdirectory(tests/unit)
endif()
