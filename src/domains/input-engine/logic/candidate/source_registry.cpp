// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/source_registry.cpp

#include "source_registry.hpp"

#include "english_passthrough_source.hpp"
#include "extension_candidate_source.hpp"
#include "number_currency_source.hpp"
#include "pinyin_candidate_source.hpp"
#include "punctuation_source.hpp"
#include "wubi_candidate_source.hpp"

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

SourceRegistry BuildDefaultSourceRegistry(LibPinyinEngine& engine, const BihuoTable& bihuo_table,
                                          bool bihuo_enabled, const WubiTable& wubi_table,
                                          char number_lead_key,
                                          ExtensionBridge* extension_bridge) {
    SourceRegistry reg;
    reg.Add(std::make_unique<ExtensionCandidateSource>(extension_bridge));
    reg.Add(std::make_unique<EnglishPassthroughSource>());
    reg.Add(std::make_unique<NumberCurrencySource>(number_lead_key));
    reg.Add(std::make_unique<PunctuationSource>());
    reg.Add(std::make_unique<PinyinCandidateSource>(engine, bihuo_table, bihuo_enabled));
    reg.Add(std::make_unique<WubiCandidateSource>(wubi_table));
    return reg;
}

}  // namespace myabc::engine
