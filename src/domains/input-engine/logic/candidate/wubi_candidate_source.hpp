// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/wubi_candidate_source.hpp
// 依据：docs/plan/08-wubi-input-scheme-plan.md §5.5
//       docs/decisions/input-engine/20260913-wubi-input-scheme.md

#ifndef MYABC_ENGINE_WUBI_CANDIDATE_SOURCE_HPP
#define MYABC_ENGINE_WUBI_CANDIDATE_SOURCE_HPP

#include "candidate_source.hpp"
#include "wubi_table.hpp"

namespace myabc::engine {

// scheme==kWubi 时接管。跟 NumberCurrencySource 走同一条既有分支：候选是跟
// libpinyin 无关的原子候选列表（UsesEngineChoose() 用默认 false，不重写），
// 选中即整句提交——五笔一个编码要么对应完整的字/词，没有 libpinyin 式的分段
// 整句路径搜索，这个组合完全匹配现有语义，不需要新增任何标志位。
class WubiCandidateSource final : public CandidateSource {
public:
    explicit WubiCandidateSource(const WubiTable& table) : table_(table) {}

    bool Handles(const InputContext& ctx) const override {
        return ctx.mode == InputMode::kChinese && ctx.scheme == InputScheme::kWubi &&
               !ctx.raw.empty();
    }

    std::vector<CandidateItem> Produce(const InputContext& ctx) override {
        std::vector<CandidateItem> out;
        for (const auto& text : table_.Lookup(ctx.raw)) {
            out.push_back(CandidateItem{.text = text, .is_sentence = false});
        }
        return out;  // 空 vector 是合法结果（编码暂无匹配，组字继续，不报错）
    }

private:
    const WubiTable& table_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_WUBI_CANDIDATE_SOURCE_HPP
