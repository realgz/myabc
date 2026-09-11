// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/bihuo_filter.hpp --- 笔形辅助码二级筛选
//
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.1
//
// 纯逻辑：拆分"拼音+笔形码后缀"、按笔形码过滤候选列表。不碰 libpinyin，
// PinyinCandidateSource 负责先用拼音部分查完候选，再交给这里过滤。

#ifndef MYABC_ENGINE_BIHUO_FILTER_HPP
#define MYABC_ENGINE_BIHUO_FILTER_HPP

#include <string>
#include <vector>

#include "bihuoma_table.hpp"
#include "libpinyin_wrapper.hpp"

namespace myabc::engine {

struct SplitRawResult {
    std::string pinyin_part;
    std::string bihuo_suffix;   // 空 = raw 里没有笔形码后缀
};

// raw 里最后一个 lead_key 之后的部分是笔形码后缀，之前的部分是拼音：
// "wo`3"（lead_key='`'） -> {"wo", "3"}；"wo" -> {"wo", ""}；"`3"（没有拼音前缀）
// 不算笔形输入，整串当拼音部分返回，bihuo_suffix 为空（笔形只做辅助筛选，不做纯笔形
// 输入，见 plan §4）。lead_key 是独立触发键、不是拼音后直接接数字——原因见
// docs/decisions/_debt-log.md 2026-09-11「笔形辅助码触发键」（与 select_keys 默认全
// 数字冲突）。
SplitRawResult SplitPinyinAndBihuo(const std::string& raw, char lead_key);

// bihuo_suffix 为空时原样返回 candidates（不过滤）。非空时：候选的第一个字若在 table
// 里查到笔形码且该码以 bihuo_suffix 为前缀则保留；查不到该字（未收录，fail-open）
// 也保留；只有"查到了但前缀不匹配"才被过滤掉。
std::vector<CandidateItem> FilterByBihuo(const std::vector<CandidateItem>& candidates,
                                         const std::string& bihuo_suffix,
                                         const BihuoTable& table);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_BIHUO_FILTER_HPP
