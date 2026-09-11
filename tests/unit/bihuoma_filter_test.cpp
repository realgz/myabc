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

    // ---- SplitPinyinAndBihuo（lead_key='`'，见 config_defaults.hpp DECISION）--
    const char kLead = '`';
    {
        const auto r = SplitPinyinAndBihuo("wo`3", kLead);
        CheckEq(r.pinyin_part, std::string("wo"), "Split(\"wo`3\").pinyin_part == \"wo\"");
        CheckEq(r.bihuo_suffix, std::string("3"), "Split(\"wo`3\").bihuo_suffix == \"3\"");
    }
    {
        const auto r = SplitPinyinAndBihuo("ji`31", kLead);
        CheckEq(r.pinyin_part, std::string("ji"), "Split(\"ji`31\").pinyin_part == \"ji\"");
        CheckEq(r.bihuo_suffix, std::string("31"), "Split(\"ji`31\").bihuo_suffix == \"31\"");
    }
    {
        const auto r = SplitPinyinAndBihuo("wo`", kLead);
        CheckEq(r.pinyin_part, std::string("wo"),
               "Split(\"wo`\")：刚按下触发键、还没打笔形数字，拼音部分仍是\"wo\"");
        CheckEq(r.bihuo_suffix, std::string(""), "Split(\"wo`\").bihuo_suffix 为空（不过滤）");
    }
    {
        const auto r = SplitPinyinAndBihuo("nihao", kLead);
        CheckEq(r.pinyin_part, std::string("nihao"), "Split(\"nihao\") 没有触发键时整串是拼音");
        CheckEq(r.bihuo_suffix, std::string(""), "Split(\"nihao\").bihuo_suffix 为空");
    }
    {
        const auto r = SplitPinyinAndBihuo("`3", kLead);
        CheckEq(r.pinyin_part, std::string("`3"),
               "Split(\"`3\")：触发键在最前面，没有拼音前缀，不当笔形输入（plan §4 不改动清单）");
        CheckEq(r.bihuo_suffix, std::string(""), "Split(\"`3\").bihuo_suffix 为空");
    }

    // ---- FilterByBihuo（用合成表，不依赖真实笔顺数据）----------------------
    BihuoTable table;
    table.LoadFromText(
        "我\t3\n"
        "握\t2\n"
        "窝\t1\n");

    std::vector<CandidateItem> candidates = {
        {"我", false}, {"握", false}, {"窝", false}, {"卧", false} /* 未收录 */,
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

    if (g_failures == 0) {
        std::puts("bihuoma_filter_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "bihuoma_filter_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
