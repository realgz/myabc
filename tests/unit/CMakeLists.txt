# SPDX-License-Identifier: GPL-3.0-or-later
#
# tests/unit --- 单元测试（无第三方框架，assert + 非零退出，CTest 判定）
# 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.1

add_executable(ipc_roundtrip_test ipc_roundtrip_test.cpp)
target_link_libraries(ipc_roundtrip_test PRIVATE myabc::ipc)
myabc_set_warnings(ipc_roundtrip_test)
add_test(NAME ipc_roundtrip COMMAND ipc_roundtrip_test)

# cn_number_test：纯函数，两套工具链都能编（依据 plan 05 §5 M4-7）。
add_executable(cn_number_test cn_number_test.cpp)
target_link_libraries(cn_number_test PRIVATE myabc::cn-number)
myabc_set_warnings(cn_number_test)
add_test(NAME cn_number COMMAND cn_number_test)

# engine_conversion_test：只在 MinGW 侧 + 已跑过 scripts/build-engine.sh 生成词库数据时构建
# （依据 docs/plan/02-m1-libpinyin-quanpin-plan.md §5 M1-2）。
if(MYABC_TOOLCHAIN STREQUAL "mingw" AND TARGET myabc::engine-core AND LibPinyin_DATA_DIR)
    add_executable(engine_conversion_test engine_conversion_test.cpp)
    target_link_libraries(engine_conversion_test PRIVATE myabc::engine-core)
    target_compile_definitions(engine_conversion_test PRIVATE
        MYABC_TEST_MODEL_DIR="${LibPinyin_DATA_DIR}"
    )
    myabc_set_warnings(engine_conversion_test)
    add_test(NAME engine_conversion COMMAND engine_conversion_test)

    add_custom_command(TARGET engine_conversion_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${LibPinyin_DLL}" "$<TARGET_FILE_DIR:engine_conversion_test>"
    )
else()
    message(STATUS "  (跳过 engine_conversion_test：非 MinGW 侧，或 libpinyin 词库数据未生成)")
endif()

# bihuoma_filter_test：笔形辅助码 拆分/过滤 逻辑，用合成表数据，不依赖真实词库
# （依据 docs/plan/05-m4-bihuo-numbers-gbk-plan.md §5 M4-8）。只需 engine-core 里的
# bihuo_filter.cpp / bihuoma_table.cpp，跟 engine_conversion_test 一样只在 MinGW 侧构建。
if(MYABC_TOOLCHAIN STREQUAL "mingw" AND TARGET myabc::engine-core)
    add_executable(bihuoma_filter_test bihuoma_filter_test.cpp)
    target_link_libraries(bihuoma_filter_test PRIVATE myabc::engine-core)
    myabc_set_warnings(bihuoma_filter_test)
    add_test(NAME bihuoma_filter COMMAND bihuoma_filter_test)
else()
    message(STATUS "  (跳过 bihuoma_filter_test：非 MinGW 侧)")
endif()

# wubi_table_test：五笔编码表查表逻辑，用合成表数据 + §3.2 已验证的真实数据行，
# 不依赖真实词库文件（依据 docs/plan/08-wubi-input-scheme-plan.md §5.16）。只需
# engine-core 里的 wubi_table.cpp，跟 bihuoma_filter_test 一样只在 MinGW 侧构建。
if(MYABC_TOOLCHAIN STREQUAL "mingw" AND TARGET myabc::engine-core)
    add_executable(wubi_table_test wubi_table_test.cpp)
    target_link_libraries(wubi_table_test PRIVATE myabc::engine-core)
    myabc_set_warnings(wubi_table_test)
    add_test(NAME wubi_table COMMAND wubi_table_test)
else()
    message(STATUS "  (跳过 wubi_table_test：非 MinGW 侧)")
endif()

# learning_policy_test：AutosaveCounter，纯逻辑不依赖 libpinyin，两套工具链都能编
# （依据 docs/plan/06-m5-user-dict-learning-plan.md §5 M5-5）。只需头文件，不链
# engine-core（避免不必要的 libpinyin 依赖）。
add_executable(learning_policy_test learning_policy_test.cpp)
target_include_directories(learning_policy_test PRIVATE
    "${CMAKE_SOURCE_DIR}/src/domains/input-engine/logic"
)
myabc_set_warnings(learning_policy_test)
add_test(NAME learning_policy COMMAND learning_policy_test)
