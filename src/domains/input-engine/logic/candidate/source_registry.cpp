// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/source_registry.cpp

#include "source_registry.hpp"

#include "english_passthrough_source.hpp"
#include "pinyin_candidate_source.hpp"
#include "punctuation_source.hpp"

namespace myabc::engine {

void SourceRegistry::Add(std::unique_ptr<CandidateSource> source) {
    sources_.push_back(std::move(source));
}

CandidateSource* SourceRegistry::Resolve(const InputContext& ctx) const {
    for (const auto& s : sources_) {
        if (s->Handles(ctx)) return s.get();
    }
    return nullptr;
}

SourceRegistry BuildDefaultSourceRegistry(LibPinyinEngine& engine) {
    SourceRegistry reg;
    reg.Add(std::make_unique<EnglishPassthroughSource>());
    reg.Add(std::make_unique<PunctuationSource>());
    reg.Add(std::make_unique<PinyinCandidateSource>(engine));
    return reg;
}

}  // namespace myabc::engine
