# SPDX-License-Identifier: GPL-3.0-or-later
#
# cmake/FindLibPinyin.cmake --- 定位 MinGW UCRT64 自建的 libpinyin（供 input-engine 用）
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §4（"供 MinGW 侧定位自建的 libpinyin"）
#       docs/plan/02-m1-libpinyin-quanpin-plan.md
#
# libpinyin 是 third_party/libpinyin 下的普通 clone（非 submodule，见 _debt-log），
# 由 scripts/setup-deps.ps1 + scripts/build-engine.sh 克隆/打补丁/构建，产物：
#   third_party/libpinyin/build/src/libpinyin.dll(.a)   —— 共享库运行时构建（B3/R1）
#   third_party/libpinyin/build/config.h                —— 特性探测头
#   third_party/libpinyin/build-data/data/               —— 静态工具生成的词库/模型二进制（R1）
#
# 只在 MYABC_TOOLCHAIN=mingw 时使用；MSVC 侧（tsf-service/deployment）禁止 include 本模块
# （不变量 1：TIP DLL 不链 libpinyin）。
#
# 提供变量：
#   LibPinyin_FOUND
#   LibPinyin_INCLUDE_DIRS   头文件搜索路径列表
#   LibPinyin_LIBRARY        导入库（.dll.a）
#   LibPinyin_DLL            运行时 DLL 绝对路径（stage.ps1 / ctest 需要复制到同目录）
#   LibPinyin_DATA_DIR       生成好的词库/模型二进制目录（table.conf 所在处）

set(_myabc_lp_root "${CMAKE_SOURCE_DIR}/third_party/libpinyin")
set(_myabc_lp_build "${_myabc_lp_root}/build")
set(_myabc_lp_build_data "${_myabc_lp_root}/build-data")

find_path(LibPinyin_INCLUDE_DIR
    NAMES pinyin.h
    PATHS "${_myabc_lp_root}/src"
    NO_DEFAULT_PATH
)

find_library(LibPinyin_LIBRARY
    NAMES libpinyin.dll.a pinyin
    PATHS "${_myabc_lp_build}/src"
    NO_DEFAULT_PATH
)

find_file(LibPinyin_DLL
    NAMES libpinyin.dll
    PATHS "${_myabc_lp_build}/src"
    NO_DEFAULT_PATH
)

if(EXISTS "${_myabc_lp_build_data}/data/table.conf")
    set(LibPinyin_DATA_DIR "${_myabc_lp_build_data}/data")
endif()

set(LibPinyin_INCLUDE_DIRS
    ${LibPinyin_INCLUDE_DIR}
    "${_myabc_lp_root}/src/include"
    "${_myabc_lp_root}/src/storage"
    "${_myabc_lp_root}/src/lookup"
    "${_myabc_lp_build}"          # config.h
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibPinyin
    REQUIRED_VARS LibPinyin_LIBRARY LibPinyin_INCLUDE_DIR LibPinyin_DLL LibPinyin_DATA_DIR
    FAIL_MESSAGE "未找到自建 libpinyin（先跑 scripts/setup-deps.ps1 + scripts/build-engine.sh）"
)

mark_as_advanced(LibPinyin_INCLUDE_DIR LibPinyin_LIBRARY LibPinyin_DLL)

if(LibPinyin_FOUND AND NOT TARGET LibPinyin::LibPinyin)
    add_library(LibPinyin::LibPinyin SHARED IMPORTED)
    set_target_properties(LibPinyin::LibPinyin PROPERTIES
        IMPORTED_IMPLIB "${LibPinyin_LIBRARY}"
        IMPORTED_LOCATION "${LibPinyin_DLL}"
        INTERFACE_INCLUDE_DIRECTORIES "${LibPinyin_INCLUDE_DIRS}"
    )
endif()
