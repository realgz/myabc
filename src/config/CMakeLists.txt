# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/config --- 配置默认值 + 运行期加载 + 设计常量 GUID
#
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.2
#       docs/architecture/system-overview.md §5 / §7 不变量 8

# --- myabc::config —— 头 + 轻量加载逻辑（两套工具链都可用，仅标准库）--------
add_library(myabc-config STATIC
    config_loader.cpp
)
add_library(myabc::config ALIAS myabc-config)
target_include_directories(myabc-config PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
myabc_set_warnings(myabc-config)

# --- myabc::guids —— GUID 定义（需要 Win32 头，仅 MSVC 侧领域使用）----------
if(MYABC_TOOLCHAIN STREQUAL "msvc")
    add_library(myabc-guids STATIC guids.cpp)
    add_library(myabc::guids ALIAS myabc-guids)
    target_include_directories(myabc-guids PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
    myabc_set_warnings(myabc-guids)
endif()
