# SPDX-License-Identifier: GPL-3.0-or-later
#
# tests/unit --- 单元测试（无第三方框架，assert + 非零退出，CTest 判定）
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.1

add_executable(ipc_roundtrip_test ipc_roundtrip_test.cpp)
target_link_libraries(ipc_roundtrip_test PRIVATE myabc::ipc)
myabc_set_warnings(ipc_roundtrip_test)
add_test(NAME ipc_roundtrip COMMAND ipc_roundtrip_test)
