// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/source_registry.hpp
// 依据：docs/architecture/system-overview.md §6；docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2
//
// 顺序：english -> number(占位, M4) -> punctuation -> pinyin（第一个 Handles() 为 true 的胜出）。
// 新增来源：加一个 CandidateSource 子类 + 在 BuildDefault() 里 push_back，不改这里的分发循环。

#ifndef MYABC_ENGINE_SOURCE_REGISTRY_HPP
#define MYABC_ENGINE_SOURCE_REGISTRY_HPP

#include <memory>
#include <vector>

#include "candidate_source.hpp"

namespace myabc::engine {

class SourceRegistry {
public:
    void Add(std::unique_ptr<CandidateSource> source);

    // 返回第一个 Handles(ctx) 为 true 的来源；均不接手时返回 nullptr。
    CandidateSource* Resolve(const InputContext& ctx) const;

private:
    std::vector<std::unique_ptr<CandidateSource>> sources_;
};

// M1 默认注册顺序：english -> punctuation -> pinyin（number 留 M4）。
SourceRegistry BuildDefaultSourceRegistry(LibPinyinEngine& engine);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_SOURCE_REGISTRY_HPP
