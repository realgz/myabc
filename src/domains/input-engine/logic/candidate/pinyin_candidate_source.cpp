// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/pinyin_candidate_source.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2
//       docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.1

#include "pinyin_candidate_source.hpp"

#include "bihuo_filter.hpp"

namespace myabc::engine {

bool PinyinCandidateSource::Handles(const InputContext& ctx) const {
    return ctx.mode == InputMode::kChinese && !ctx.raw.empty();
}

std::vector<CandidateItem> PinyinCandidateSource::Produce(const InputContext& ctx) {
    const SplitRawResult split = SplitPinyinAndBihuo(ctx.raw);

    engine_.ParseAndGuess(split.pinyin_part);
    const auto& candidates = engine_.candidates();

    if (!bihuo_enabled_ || split.bihuo_suffix.empty()) return candidates;
    return FilterByBihuo(candidates, split.bihuo_suffix, bihuo_table_);
}

}  // namespace myabc::engine
