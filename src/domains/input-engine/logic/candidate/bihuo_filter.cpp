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
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        CandidateItem c = candidates[i];
        const std::string first_char = FirstUtf8Char(c.text);
        const auto code = table.Lookup(first_char);
        const bool keep = !code || code->rfind(bihuo_suffix, 0) == 0;   // 未收录 fail-open，
                                                                        // 或 code 以后缀为前缀
        if (!keep) continue;
        // 过滤会让结果下标跟原始（libpinyin 内部）下标错位——必须记住原始下标，
        // 否则 Session::SelectCandidate 选中显示的第 N 项会实际选中 libpinyin 里
        // 完全不同的第 N 项（M5 前修复的真 bug，见 _debt-log.md）。
        c.engine_index = static_cast<int>(i);
        result.push_back(std::move(c));
    }
    return result;
}

}  // namespace myabc::engine
