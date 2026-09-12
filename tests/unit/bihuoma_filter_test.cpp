// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/unit/bihuoma_filter_test.cpp --- 笔形辅助码 拆分/过滤 逻辑单测（M4-8）
//
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §5
//
// 用合成表数据测试过滤机制本身的正确性，不依赖 assets/data/bihuoma.txt 的真实
// 笔顺是否精确（那张表故意很小，见 docs/decisions/_debt-log.md 2026-09-11）。

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "candidate/bihuo_filter.hpp"
#include "bihuoma_table.hpp"

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

    // ---- SplitPinyinAndBihuo（不再需要触发键，见 config_defaults.hpp DECISION）--
    {
        const auto r = SplitPinyinAndBihuo("wo3");
        CheckEq(r.pinyin_part, std::string("wo"), "Split(\"wo3\").pinyin_part == \"wo\"");
        CheckEq(r.bihuo_suffix, std::string("3"), "Split(\"wo3\").bihuo_suffix == \"3\"");
    }
    {
        const auto r = SplitPinyinAndBihuo("ji31");
        CheckEq(r.pinyin_part, std::string("ji"), "Split(\"ji31\").pinyin_part == \"ji\"");
        CheckEq(r.bihuo_suffix, std::string("31"), "Split(\"ji31\").bihuo_suffix == \"31\"");
    }
    {
        const auto r = SplitPinyinAndBihuo("nihao");
        CheckEq(r.pinyin_part, std::string("nihao"), "Split(\"nihao\") 没有尾随数字时整串是拼音");
        CheckEq(r.bihuo_suffix, std::string(""), "Split(\"nihao\").bihuo_suffix 为空");
    }
    {
        const auto r = SplitPinyinAndBihuo("3");
        CheckEq(r.pinyin_part, std::string("3"),
               "Split(\"3\")：全是数字，没有拼音前缀，不当笔形输入（plan §4 不改动清单）");
        CheckEq(r.bihuo_suffix, std::string(""), "Split(\"3\").bihuo_suffix 为空");
    }

    // ---- FilterByBihuo（用合成表，不依赖真实笔顺数据）----------------------
    BihuoTable table;
    table.LoadFromText(
        "我\t3\n"
        "握\t2\n"
        "窝\t1\n");

    std::vector<CandidateItem> candidates = {
        {.text = "我", .is_sentence = false},
        {.text = "握", .is_sentence = false},
        {.text = "窝", .is_sentence = false},
        {.text = "卧", .is_sentence = false} /* 未收录 */,
    };

    {
        const auto filtered = FilterByBihuo(candidates, "3", table);
        CheckEq(filtered.size(), static_cast<std::size_t>(2),
               "笔形3过滤：保留\"我\"(收录且匹配) + \"卧\"(未收录,fail-open)，共2个");
        bool has_wo = false, has_wo2 = false;
        for (const auto& c : filtered) {
            if (c.text == "我") has_wo = true;
            if (c.text == "卧") has_wo2 = true;
        }
        if (!has_wo || !has_wo2) {
            std::fprintf(stderr, "FAIL: 笔形3过滤结果内容不对\n");
            ++g_failures;
        } else {
            std::printf("OK: 笔形3过滤结果内容正确\n");
        }
    }
    {
        const auto filtered = FilterByBihuo(candidates, "", table);
        CheckEq(filtered.size(), candidates.size(), "空后缀不过滤，原样返回全部候选");
    }
    {
        // 表命中但前缀不匹配的字应该被排除（"握"=2 不匹配"9"）。
        const auto filtered = FilterByBihuo(candidates, "9", table);
        bool has_wo_hand = false;
        for (const auto& c : filtered) {
            if (c.text == "握") has_wo_hand = true;
        }
        if (has_wo_hand) {
            std::fprintf(stderr, "FAIL: \"握\"笔形码是2，不应该匹配后缀\"9\"\n");
            ++g_failures;
        } else {
            std::printf("OK: 表命中但前缀不匹配的字被正确排除\n");
        }
    }
    {
        // 回归（M5 前修复的真 bug，见 _debt-log.md）：过滤后每一项必须记住自己在
        // *原始*（未过滤）列表里的下标（engine_index），否则 Session::SelectCandidate
        // 传给 libpinyin 的下标会跟用户看到的候选错位，选中显示的第 N 项实际会选中
        // libpinyin 内部完全不同的候选。candidates = ["我"(0), "握"(1), "窝"(2), "卧"(3)]，
        // 笔形"1" 应保留 "窝"(原始下标2) 和 "卧"(原始下标3，未收录 fail-open)。
        const auto filtered = FilterByBihuo(candidates, "1", table);
        CheckEq(filtered.size(), static_cast<std::size_t>(2), "笔形1过滤应保留2项（窝+卧）");
        bool ok = true;
        for (const auto& c : filtered) {
            if (c.text == "窝" && c.engine_index != 2) ok = false;
            if (c.text == "卧" && c.engine_index != 3) ok = false;
        }
        if (ok) {
            std::printf("OK: 过滤后每项的 engine_index 正确指回原始下标（选中不会错位）\n");
        } else {
            std::fprintf(stderr, "FAIL: engine_index 没有正确指回原始下标——选中会错位到别的候选\n");
            ++g_failures;
        }
    }
    {
        // 空后缀不过滤：原样返回，engine_index 保持默认 -1（Session 约定：-1 时
        // 显示下标本身就是 libpinyin 内部下标，未过滤/未重排时天然成立）。
        const auto filtered = FilterByBihuo(candidates, "", table);
        bool all_default = true;
        for (const auto& c : filtered) {
            if (c.engine_index != -1) all_default = false;
        }
        if (all_default) {
            std::printf("OK: 空后缀不过滤时 engine_index 保持默认 -1\n");
        } else {
            std::fprintf(stderr, "FAIL: 空后缀不该改动 engine_index\n");
            ++g_failures;
        }
    }

    if (g_failures == 0) {
        std::puts("bihuoma_filter_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "bihuoma_filter_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
