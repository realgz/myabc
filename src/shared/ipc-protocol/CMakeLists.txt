# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/shared/ipc-protocol --- 命名管道协议编解码（STATIC，两套工具链都编）
#
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.1
#       docs/architecture/system-overview.md §7 不变量 6（只用标准库，不碰 windows.h）

add_library(myabc-ipc STATIC
    protocol.cpp
    frame.cpp
    json_codec.cpp
)
add_library(myabc::ipc ALIAS myabc-ipc)

target_include_directories(myabc-ipc PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/third_party/json"   # -> <nlohmann/json.hpp>
)

# nlohmann/json.hpp 以 <nlohmann/json.hpp> 引入；单头放在 third_party/json/nlohmann/。
# 见 third_party/json/README.md。

myabc_set_warnings(myabc-ipc)

if(MSVC)
    target_compile_definitions(myabc-ipc PUBLIC _CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()
