// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/bihuoma_table.cpp
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.1

#include "bihuoma_table.hpp"

#include <fstream>
#include <sstream>

namespace myabc::engine {

namespace {

void ParseLines(std::istream& in, std::unordered_map<std::string, std::string>& out) {
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();   // CRLF 容错
        if (line.empty() || line[0] == '#') continue;

        const auto tab = line.find('\t');
        if (tab == std::string::npos) continue;
        std::string ch = line.substr(0, tab);
        std::string code = line.substr(tab + 1);
        if (ch.empty() || code.empty()) continue;
        out[ch] = code;
    }
}

}  // namespace

bool BihuoTable::LoadFromFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    ParseLines(f, table_);
    return true;
}

void BihuoTable::LoadFromText(const std::string& tsv_text) {
    std::istringstream iss(tsv_text);
    ParseLines(iss, table_);
}

std::optional<std::string> BihuoTable::Lookup(const std::string& utf8_char) const {
    const auto it = table_.find(utf8_char);
    if (it == table_.end()) return std::nullopt;
    return it->second;
}

std::string FirstUtf8Char(const std::string& utf8_text) {
    if (utf8_text.empty()) return {};
    const unsigned char lead = static_cast<unsigned char>(utf8_text[0]);
    std::size_t len = 1;
    if ((lead & 0x80) == 0x00) {
        len = 1;
    } else if ((lead & 0xE0) == 0xC0) {
        len = 2;
    } else if ((lead & 0xF0) == 0xE0) {
        len = 3;
    } else if ((lead & 0xF8) == 0xF0) {
        len = 4;
    }
    len = len > utf8_text.size() ? utf8_text.size() : len;
    return utf8_text.substr(0, len);
}

}  // namespace myabc::engine
