// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/unit/wubi_table_test.cpp --- 五笔编码表查表逻辑单测
//
// 依据：docs/plan/08-wubi-input-scheme-plan.md §5.16
//       docs/decisions/input-engine/20260913-wubi-input-scheme.md
//
// 用合成小表测试查表机制本身（权重排序/未收录/空表安全），另外用 §3.2 已验证过的
// 真实数据行（不编造）覆盖"真实重码"场景。

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "wubi_table.hpp"

namespace {

int g_failures = 0;

template <typename T>
void CheckEq(const T& actual, const T& expect, const char* what) {
    if (!(actual == expect)) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    } else {
        std::printf("OK: %s\n", what);
    }
}

}  // namespace

int main() {
    using namespace myabc::engine;

    // ---- 空表：安全空操作，不崩溃 -------------------------------------------
    {
        WubiTable table;
        const auto r = table.Lookup("a");
        CheckEq(r.empty(), true, "空表查询任意编码返回空 vector，不崩溃");
        CheckEq(table.size(), static_cast<std::size_t>(0), "空表 size()==0");
    }

    // ---- 合成表：权重降序排序 ------------------------------------------------
    {
        WubiTable table;
        table.LoadFromText(
            "aet\t㣉\t4190\n"
            "aet\t散\t96000000\n"   // 权重更高，加载顺序在后，排序后应排到前面
            "a\t工\t99454797\n");

        const auto aet = table.Lookup("aet");
        CheckEq(aet.size(), static_cast<std::size_t>(2), "\"aet\" 命中两个候选（重码）");
        if (aet.size() == 2) {
            CheckEq(aet[0], std::string("散"), "重码按权重降序：\"散\"(权重更高)排第一");
            CheckEq(aet[1], std::string("㣉"), "重码按权重降序：\"㣉\"(权重更低)排第二");
        }

        const auto a = table.Lookup("a");
        CheckEq(a.size(), static_cast<std::size_t>(1), "\"a\" 命中唯一候选");
        if (!a.empty()) CheckEq(a[0], std::string("工"), "\"a\" -> \"工\"");
    }

    // ---- 未收录编码：返回空 vector，不是错误 ---------------------------------
    {
        WubiTable table;
        table.LoadFromText("a\t工\t99454797\n");
        const auto r = table.Lookup("zz");
        CheckEq(r.empty(), true, "未收录编码返回空 vector（组字继续，不报错）");
    }

    // ---- '#' 开头注释行 / 空行 / CRLF 容错 -----------------------------------
    {
        WubiTable table;
        table.LoadFromText(
            "# 这是注释行，应被跳过\r\n"
            "\r\n"
            "a\t工\t99454797\r\n");
        CheckEq(table.size(), static_cast<std::size_t>(1), "注释行/空行被正确跳过，只加载1条编码");
        const auto a = table.Lookup("a");
        CheckEq(a.size(), static_cast<std::size_t>(1), "CRLF 容错：\"a\" 仍能正确查到");
    }

    // ---- 真实数据行（docs/plan/08-wubi-input-scheme-plan.md §3.2 已验证，不编造）--
    {
        WubiTable table;
        table.LoadFromText(
            "aaad\t工期\t5350000\n"
            "aet\t散\t96000000\n"
            "aet\t㣉\t4190\n");

        const auto aaad = table.Lookup("aaad");
        CheckEq(aaad.size(), static_cast<std::size_t>(1), "真实数据：\"aaad\" 唯一候选");
        if (!aaad.empty()) CheckEq(aaad[0], std::string("工期"), "真实数据：\"aaad\" -> \"工期\"");

        const auto aet = table.Lookup("aet");
        CheckEq(aet.size(), static_cast<std::size_t>(2), "真实数据：\"aet\" 重码，2个候选");
        if (aet.size() == 2) CheckEq(aet[0], std::string("散"), "真实数据：\"aet\" 重码，权重更高的\"散\"排第一");
    }

    if (g_failures == 0) {
        std::puts("wubi_table_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "wubi_table_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
