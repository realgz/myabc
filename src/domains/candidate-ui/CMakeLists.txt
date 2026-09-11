# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/domains/candidate-ui --- 候选窗（M2 起：独立进程 myabc-ui.exe，GDI 渲染，MSVC）
#
# 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.4
#       docs/plan/03-m2-engine-process-ipc-plan.md §3.1（迁出 tip.dll，不再链入 TIP）
#
# DECISION: 用 GDI 而非 Direct2D/DirectWrite，见 backend/candidate_window.hpp 顶部注释
# 与 docs/decisions/_debt-log.md 2026-09-11。因此不链 d2d1/dwrite/dcomp/windowscodecs
# （plan 原文列的库），只需 gdi32/user32。
#
# myabc-ui.exe 直接连引擎的 uiShow/uiHide 推送管道（不经 TIP），复用 myabc::ipc 的帧/编解码
# （两套工具链都编译，见 src/shared/ipc-protocol），因此需要 myabc::config + myabc::ipc。

if(NOT MYABC_TOOLCHAIN STREQUAL "msvc")
    message(FATAL_ERROR "candidate-ui 仅在 MSVC 侧构建。")
endif()

add_library(myabc-candidate-ui STATIC
    backend/candidate_window.cpp
)
add_library(myabc::candidate-ui ALIAS myabc-candidate-ui)

target_include_directories(myabc-candidate-ui PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/backend"
    "${CMAKE_CURRENT_SOURCE_DIR}/logic"
)

target_link_libraries(myabc-candidate-ui PUBLIC gdi32 user32)
target_compile_definitions(myabc-candidate-ui PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
myabc_set_warnings(myabc-candidate-ui)

# --- myabc-ui.exe（M2）----------------------------------------------------
add_executable(myabc-ui ui/host_main.cpp)
target_link_libraries(myabc-ui PRIVATE myabc-candidate-ui myabc::config myabc::ipc advapi32)
target_compile_definitions(myabc-ui PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
myabc_set_warnings(myabc-ui)
