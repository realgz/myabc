# SPDX-License-Identifier: GPL-3.0-or-later
#
# src/shared/cn-number --- 阿拉伯数字 <-> 中文数字/大写金额 纯函数库
#
# 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.2
#       code-quality-standards.md §1（纯技术、无业务逻辑）
#
# 只用标准库，两套工具链都编（目前只有 input-engine/MinGW 用，但按 src/shared 惯例
# 两侧都构建，供以后任何一侧需要时直接取用）。

add_library(myabc-cn-number STATIC cn_number.cpp)
add_library(myabc::cn-number ALIAS myabc-cn-number)

target_include_directories(myabc-cn-number PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
myabc_set_warnings(myabc-cn-number)
