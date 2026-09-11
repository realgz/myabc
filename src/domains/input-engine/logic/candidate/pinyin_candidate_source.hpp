// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/pinyin_candidate_source.hpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2
//       docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.1（笔形辅助码二级筛选）

#ifndef MYABC_ENGINE_PINYIN_CANDIDATE_SOURCE_HPP
#define MYABC_ENGINE_PINYIN_CANDIDATE_SOURCE_HPP

#include "bihuoma_table.hpp"
#include "candidate_source.hpp"

namespace myabc::engine {

// 中文模式下、raw 是拼音字母（不含独立标点触发字符）时接管。raw 可能带 bihuo_lead_key
// 引导的笔形码后缀（如 "wo`3"）——先用拼音部分（"wo"）查 libpinyin，再按笔形码过滤结果。
// bihuo_table 为空表时过滤是安全的空操作（fail-open，见 bihuo_filter.hpp DECISION）。
class PinyinCandidateSource final : public CandidateSource {
public:
    PinyinCandidateSource(LibPinyinEngine& engine, const BihuoTable& bihuo_table,
                         bool bihuo_enabled, char bihuo_lead_key)
        : engine_(engine), bihuo_table_(bihuo_table), bihuo_enabled_(bihuo_enabled),
          bihuo_lead_key_(bihuo_lead_key) {}

    bool Handles(const InputContext& ctx) const override;
    std::vector<CandidateItem> Produce(const InputContext& ctx) override;

private:
    LibPinyinEngine& engine_;
    const BihuoTable& bihuo_table_;
    bool bihuo_enabled_;
    char bihuo_lead_key_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_PINYIN_CANDIDATE_SOURCE_HPP
