// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/wubi_table.cpp
// 依据：docs/plan/08-wubi-input-scheme-plan.md §5.4

#include "wubi_table.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace myabc::engine {

namespace {

void ParseLines(std::istream& in,
                 std::unordered_map<std::string, std::vector<std::pair<std::string, std::int64_t>>>& out) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF 容错
        if (line.empty() || line[0] == '#') continue;

        const auto tab1 = line.find('\t');
        if (tab1 == std::string::npos) continue;
        const auto tab2 = line.find('\t', tab1 + 1);
        if (tab2 == std::string::npos) continue;

        std::string code = line.substr(0, tab1);
        std::string text = line.substr(tab1 + 1, tab2 - tab1 - 1);
        std::string weight_str = line.substr(tab2 + 1);
        if (code.empty() || text.empty() || weight_str.empty()) continue;

        const std::int64_t weight = std::strtoll(weight_str.c_str(), nullptr, 10);
        out[code].emplace_back(std::move(text), weight);
    }

    // 一次性按权重降序排序，Lookup 不需要每次查询都排序。
    for (auto& [code, items] : out) {
        std::sort(items.begin(), items.end(),
                   [](const auto& a, const auto& b) { return a.second > b.second; });
    }
}

}  // namespace

bool WubiTable::LoadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    ParseLines(f, table_);
    return true;
}

void WubiTable::LoadFromText(const std::string& tsv_text) {
    std::istringstream iss(tsv_text);
    ParseLines(iss, table_);
}

std::vector<std::string> WubiTable::Lookup(const std::string& code) const {
    std::vector<std::string> out;
    const auto it = table_.find(code);
    if (it == table_.end()) return out;
    out.reserve(it->second.size());
    for (const auto& [text, weight] : it->second) out.push_back(text);
    return out;
}

}  // namespace myabc::engine
