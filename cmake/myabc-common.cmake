# SPDX-License-Identifier: GPL-3.0-or-later
#
# cmake/myabc-common.cmake --- 两侧工具链共享的公共设置
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §4
#       docs/review-standards/code-review-checklist.md
#
# 由顶层 CMakeLists.txt include。不放任何领域逻辑，只放编译器口径 / 警告级别 / 语言标准。

include_guard(GLOBAL)

# --- 产物集中输出（便于 scripts/stage.ps1 收集）----------------------------
# 所有可执行文件 / DLL / 导入库落到 <binaryDir>/bin，静态库落 <binaryDir>/lib。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" CACHE PATH "")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib" CACHE PATH "")

# --- C++ 标准（system-overview：C++20）--------------------------------------
set(CMAKE_CXX_STANDARD 20 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- 警告级别 -------------------------------------------------------------
function(myabc_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

# --- 每个产物统一加 SPDX 检查钩子的占位（src/review 落地后接入）-----------
# TODO(M0 R2+): 接入 src/review 的 DECISION: 注释覆盖率 / 硬编码扫描
