// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/scheme_state_store.cpp

#include "scheme_state_store.hpp"

#include <fstream>
#include <iterator>

namespace myabc::engine {

std::optional<std::string> LoadSchemeState(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return std::nullopt;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // 去掉可能的行尾空白（不同工具写入时常见 CRLF/尾随空格）。
    while (!content.empty() &&
           (content.back() == '\n' || content.back() == '\r' || content.back() == ' ')) {
        content.pop_back();
    }
    if (content.empty()) return std::nullopt;
    return content;
}

void SaveSchemeState(const std::string& path, const std::string& method) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return;   // 静默忽略，见头文件 DECISION
    f << method;
}

}  // namespace myabc::engine
