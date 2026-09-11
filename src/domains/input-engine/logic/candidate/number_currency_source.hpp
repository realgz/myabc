// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/number_currency_source.hpp
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.2（M4-3/4/5）

#ifndef MYABC_ENGINE_NUMBER_CURRENCY_SOURCE_HPP
#define MYABC_ENGINE_NUMBER_CURRENCY_SOURCE_HPP

#include <string>

#include "candidate_source.hpp"

namespace myabc::engine {

// raw 以 config.input.number_lead_key（默认 "i"）开头、后接数字/小数点/负号时接管。
// 整数输入给"数量读法 + 逐位读法 + 大写金额(整) + 原样阿拉伯数字"四种候选；
// 带小数点的输入视为金额，给"大写金额 + 原样"两种（数量读法/逐位读法对小数没有
// 计划要求的展示形式，见 docs/decisions/_debt-log.md 2026-09-11）。
class NumberCurrencySource final : public CandidateSource {
public:
    explicit NumberCurrencySource(char lead_key) : lead_key_(lead_key) {}

    bool Handles(const InputContext& ctx) const override;
    std::vector<CandidateItem> Produce(const InputContext& ctx) override;

private:
    char lead_key_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_NUMBER_CURRENCY_SOURCE_HPP
