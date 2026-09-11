// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/pinyin_candidate_source.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2

#include "pinyin_candidate_source.hpp"

namespace myabc::engine {

bool PinyinCandidateSource::Handles(const InputContext& ctx) const {
    return ctx.mode == InputMode::kChinese && !ctx.raw.empty();
}

std::vector<CandidateItem> PinyinCandidateSource::Produce(const InputContext& ctx) {
    engine_.ParseAndGuess(ctx.raw);
    return engine_.candidates();
}

}  // namespace myabc::engine
