// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/pinyin_candidate_source.hpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2

#ifndef MYABC_ENGINE_PINYIN_CANDIDATE_SOURCE_HPP
#define MYABC_ENGINE_PINYIN_CANDIDATE_SOURCE_HPP

#include "candidate_source.hpp"

namespace myabc::engine {

// 中文模式下、raw 是拼音字母（不含独立标点触发字符）时接管。
// 真正的解析/候选生成整个委托给 LibPinyinEngine（本类只是登记进 CandidateSource 接口）。
class PinyinCandidateSource final : public CandidateSource {
public:
    explicit PinyinCandidateSource(LibPinyinEngine& engine) : engine_(engine) {}

    bool Handles(const InputContext& ctx) const override;
    std::vector<CandidateItem> Produce(const InputContext& ctx) override;

private:
    LibPinyinEngine& engine_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_PINYIN_CANDIDATE_SOURCE_HPP
