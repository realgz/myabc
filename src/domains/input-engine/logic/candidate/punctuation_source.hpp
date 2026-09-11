// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/punctuation_source.hpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2（M1-10：中文模式标点全角化）

#ifndef MYABC_ENGINE_PUNCTUATION_SOURCE_HPP
#define MYABC_ENGINE_PUNCTUATION_SOURCE_HPP

#include "candidate_source.hpp"

namespace myabc::engine {

// 中文模式下、raw 是单个"非组字"标点字符（不在字母表里）时接管，直接给出全角映射作为
// 唯一候选（上层 session 通常直接自动提交，不停留组字）。
//
// DECISION: 映射表当前内嵌常量（非环境相关值，属语言数据而非硬编码配置项，不违反
// code-quality-standards §3.1 反硬编码）；计划 §3.6 的 assets/config/punctuation.toml
// 已建档待 TOML loader 落地后接管，见 docs/decisions/_debt-log.md 2026-09-11。
class PunctuationSource final : public CandidateSource {
public:
    bool Handles(const InputContext& ctx) const override;
    std::vector<CandidateItem> Produce(const InputContext& ctx) override;
    bool AutoCommit() const override { return true; }

    // 供 key_router 判定"这个字符是否有标点映射"，不产出候选。
    static bool IsMappedPunctuation(char ascii);
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_PUNCTUATION_SOURCE_HPP
