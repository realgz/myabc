# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/domains/deployment --- myabc-deployer.exe + 注册核心静态库（MSVC）
#
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4 / §3.3
#       （§3.3：DllRegisterServer 调 deployment 的 RegisterProfiles/RegisterCategories）

if(NOT MYABC_TOOLCHAIN STREQUAL "msvc")
    message(FATAL_ERROR "deployment 仅在 MSVC 侧构建。")
endif()

find_package(wil CONFIG REQUIRED)

# --- 注册核心：TIP DLL 与 deployer 共用（同一套 COM/TSF 注册逻辑）-----------
add_library(myabc-deploy-core STATIC
    backend/tsf_register.cpp
    backend/com_register.cpp
)
add_library(myabc::deploy-core ALIAS myabc-deploy-core)
target_include_directories(myabc-deploy-core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/backend")
target_link_libraries(myabc-deploy-core PUBLIC myabc::guids WIL::WIL ole32 oleaut32 uuid)
target_compile_definitions(myabc-deploy-core PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
myabc_set_warnings(myabc-deploy-core)

# --- myabc-deployer.exe -------------------------------------------------
add_executable(myabc-deployer
    ui/main.cpp
    logic/deploy_flow.cpp
    backend/engine_client.cpp
)
target_include_directories(myabc-deployer PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/logic"
    "${CMAKE_CURRENT_SOURCE_DIR}/backend"
)
# M5（plan 06 §3.4）：--export/import/clear-userdict 经命名管道跟引擎对话，只此三个
# 命令需要 myabc::ipc；不进 myabc-deploy-core（那个库也被 DllRegisterServer/TIP DLL
# 用，不该多背一个 ipc 依赖）。
target_link_libraries(myabc-deployer PRIVATE myabc::deploy-core myabc::config myabc::ipc)
myabc_set_warnings(myabc-deployer)
set_target_properties(myabc-deployer PROPERTIES
    LINK_FLAGS "/SUBSYSTEM:CONSOLE /ENTRY:wmainCRTStartup"
)
