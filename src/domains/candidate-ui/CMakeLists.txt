# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/domains/candidate-ui --- 候选窗（M1：TIP 进程内静态库，GDI 渲染，MSVC）
#
# 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.4
#
# DECISION: 用 GDI 而非 Direct2D/DirectWrite，见 backend/candidate_window.hpp 顶部注释
# 与 docs/decisions/_debt-log.md 2026-09-11。因此不链 d2d1/dwrite/dcomp/windowscodecs
# （plan 原文列的库），只需 gdi32/user32。

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
