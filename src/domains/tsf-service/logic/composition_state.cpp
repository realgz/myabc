// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/composition_state.cpp
// 依据：docs/plan/03-m2-engine-process-ipc-plan.md §3.2

#include "composition_state.hpp"

#include "text_convert.hpp"

namespace myabc::tsf {

void CompositionState::ApplyResult(const ipc::Json& result) {
    Reset();

    preedit = Widen(result.value("preedit", std::string{}));
    composing = result.value("composing", false);

    if (const auto it = result.find("commit"); it != result.end() && it->is_string()) {
        has_commit = true;
        commit_text = Widen(it->get<std::string>());
        composing = false;   // 上屏即结束组字
    }
}

void CompositionState::Reset() {
    composing = false;
    preedit.clear();
    has_commit = false;
    commit_text.clear();
}

}  // namespace myabc::tsf
