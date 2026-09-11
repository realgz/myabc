// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/english_passthrough_source.hpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2（M1-9：Shift 切英文直接上屏）

#ifndef MYABC_ENGINE_ENGLISH_PASSTHROUGH_SOURCE_HPP
#define MYABC_ENGINE_ENGLISH_PASSTHROUGH_SOURCE_HPP

#include "candidate_source.hpp"

namespace myabc::engine {

// 英文模式：raw 原样作为唯一候选（TIP 侧通常直接上屏，不弹候选窗，但保留接口一致性）。
class EnglishPassthroughSource final : public CandidateSource {
public:
    bool Handles(const InputContext& ctx) const override;
    std::vector<CandidateItem> Produce(const InputContext& ctx) override;
    bool AutoCommit() const override { return true; }
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_ENGLISH_PASSTHROUGH_SOURCE_HPP
