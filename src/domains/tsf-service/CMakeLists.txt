# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/domains/tsf-service --- myabc-tip.dll（TSF TIP，MSVC，x86 + x64）
#
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
#       docs/architecture/system-overview.md §7 不变量 1（不链 libpinyin/glib/db）
#
# 不变量校验：本 target 的链接依赖里不得出现 libpinyin / glib / db / input-engine。

if(NOT MYABC_TOOLCHAIN STREQUAL "msvc")
    message(FATAL_ERROR "tsf-service 仅在 MSVC 侧构建。")
endif()

find_package(wil CONFIG REQUIRED)

add_library(myabc-tip SHARED
    backend/dll_main.cpp
    backend/class_factory.cpp
    backend/myabc_text_service.cpp
    backend/edit_session.cpp
    backend/ipc_client.cpp
    backend/dll_refcount.cpp
    logic/key_router.cpp
    "${CMAKE_CURRENT_SOURCE_DIR}/tsf-service.def"
)

target_include_directories(myabc-tip PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/backend"
    "${CMAKE_CURRENT_SOURCE_DIR}/logic"
)

target_link_libraries(myabc-tip PRIVATE
    myabc::ipc          # 仅标准库 —— 不引入 libpinyin
    myabc::config
    myabc::guids
    myabc::deploy-core  # DllRegisterServer 复用 TSF/COM 注册逻辑
    WIL::WIL
    ole32 oleaut32 uuid advapi32   # TSF 是 COM，无 msctf.lib；CLSID/IID 来自 uuid.lib
)

target_compile_definitions(myabc-tip PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
myabc_set_warnings(myabc-tip)

set_target_properties(myabc-tip PROPERTIES OUTPUT_NAME "myabc-tip" PREFIX "")
