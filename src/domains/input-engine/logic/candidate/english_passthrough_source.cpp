// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/english_passthrough_source.cpp

#include "english_passthrough_source.hpp"

namespace myabc::engine {

bool EnglishPassthroughSource::Handles(const InputContext& ctx) const {
    return ctx.mode == InputMode::kEnglish;
}

std::vector<CandidateItem> EnglishPassthroughSource::Produce(const InputContext& ctx) {
    return {CandidateItem{ctx.raw, /*is_sentence=*/false}};
}

}  // namespace myabc::engine
