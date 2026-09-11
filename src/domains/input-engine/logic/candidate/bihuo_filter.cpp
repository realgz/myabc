// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/bihuo_filter.cpp
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.1

#include "bihuo_filter.hpp"

#include <cctype>

namespace myabc::engine {

SplitRawResult SplitPinyinAndBihuo(const std::string& raw, char lead_key) {
    const auto pos = raw.rfind(lead_key);
    if (pos == std::string::npos || pos == 0) {
        // 没有 lead_key，或 lead_key 在最前面（没有拼音前缀）：不当笔形输入处理，
        // 整串按拼音处理（见 bihuo_filter.hpp 头注释、plan §4 不改动清单）。
        return SplitRawResult{raw, ""};
    }
    return SplitRawResult{raw.substr(0, pos), raw.substr(pos + 1)};
}

std::vector<CandidateItem> FilterByBihuo(const std::vector<CandidateItem>& candidates,
                                         const std::string& bihuo_suffix,
                                         const BihuoTable& table) {
    if (bihuo_suffix.empty()) return candidates;

    std::vector<CandidateItem> result;
    result.reserve(candidates.size());
    for (const auto& c : candidates) {
        const std::string first_char = FirstUtf8Char(c.text);
        const auto code = table.Lookup(first_char);
        if (!code) {
            result.push_back(c);   // 未收录：fail-open，不过滤
            continue;
        }
        if (code->rfind(bihuo_suffix, 0) == 0) {   // code 以 bihuo_suffix 为前缀
            result.push_back(c);
        }
    }
    return result;
}

}  // namespace myabc::engine
