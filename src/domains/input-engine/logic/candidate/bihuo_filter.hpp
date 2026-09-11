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

// raw 末尾连续的 1-5 数字是笔形码后缀，之前的部分是拼音：
// "wo31" -> {"wo", "31"}；"wo" -> {"wo", ""}；"31"（没有拼音前缀，纯数字）不算笔形
// 输入，整串当拼音部分返回，bihuo_suffix 为空（笔形只做辅助筛选，不做纯笔形输入，
// 见 plan §4）。
// DECISION（docs/decisions/_debt-log.md 2026-09-11「笔形辅助码触发键」条目已废弃，
// 见同日追加的更新条目）：M4 曾因为"拼音后裸数字"跟 select_keys（数字选字）冲突，
// 加过一个独立触发键（反引号）。用户进一步明确要求后改为：select_keys 只在按过一次
// 空格（Session::space_armed_）之后才生效，笔形数字（1-5）在此之前一直有效——这样
// 两者不再冲突，不需要额外的触发键，恢复到 plan 最初"拼音直接接数字"的字面设计。
// 这个函数本身跟 Session 的按键路由解耦，只管字符串拆分，不知道"什么时候允许追加
// 笔形数字"这个时机判断（那是 Session::ProcessKey 的责任）。
SplitRawResult SplitPinyinAndBihuo(const std::string& raw);

// bihuo_suffix 为空时原样返回 candidates（不过滤）。非空时：候选的第一个字若在 table
// 里查到笔形码且该码以 bihuo_suffix 为前缀则保留；查不到该字（未收录，fail-open）
// 也保留；只有"查到了但前缀不匹配"才被过滤掉。
std::vector<CandidateItem> FilterByBihuo(const std::vector<CandidateItem>& candidates,
                                         const std::string& bihuo_suffix,
                                         const BihuoTable& table);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_BIHUO_FILTER_HPP
