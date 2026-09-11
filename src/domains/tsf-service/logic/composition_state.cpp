// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/composition_state.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5

#include "composition_state.hpp"

#include "text_convert.hpp"

namespace myabc::tsf {

void CompositionState::ApplyResult(const ipc::Json& result) {
    Reset();

    const std::string preedit_utf8 = result.value("preedit", std::string{});
    preedit = Widen(preedit_utf8);
    composing = !preedit_utf8.empty();

    if (const auto it = result.find("candidates"); it != result.end() && it->is_array()) {
        for (const auto& c : *it) {
            candidates.push_back(CandidateLine{Widen(c.value("text", std::string{}))});
        }
    }
    if (!candidates.empty()) composing = true;

    if (const auto it = result.find("page"); it != result.end() && it->is_object()) {
        page_index = it->value("index", 0);
        page_size = it->value("size", 0);
        page_total = it->value("total", 0);
    }

    if (const auto it = result.find("commit"); it != result.end() && it->is_string()) {
        has_commit = true;
        commit_text = Widen(it->get<std::string>());
        composing = false;   // 上屏即结束组字
    }
}

void CompositionState::Reset() {
    composing = false;
    preedit.clear();
    candidates.clear();
    page_index = page_size = page_total = 0;
    has_commit = false;
    commit_text.clear();
}

}  // namespace myabc::tsf
