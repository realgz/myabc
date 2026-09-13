// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/scheme_hotkey_detector.cpp

#include "scheme_hotkey_detector.hpp"

#include <cctype>

namespace myabc::tsf {

int ParseHotkeyLetterVk(const std::string& spec, int fallback_vk) {
    // 只需要认得 "ctrl+shift+<字母>" 这一种格式（config_defaults.hpp 默认值
    // "ctrl+shift+w"）——不是通用热键语法解析器，够用即可，不过度设计。
    const auto last_plus = spec.find_last_of('+');
    if (last_plus == std::string::npos || last_plus + 1 >= spec.size()) return fallback_vk;
    const std::string letter_part = spec.substr(last_plus + 1);
    if (letter_part.size() != 1) return fallback_vk;
    const unsigned char c = static_cast<unsigned char>(letter_part[0]);
    if (!std::isalpha(c)) return fallback_vk;
    // 大写 ASCII 字母的 VK 值恰好等于其 ASCII 码（'A'-'Z' = 0x41-0x5A）。
    return std::toupper(c);
}

bool SchemeHotkeyDetector::OnKeyDown(int vk, bool ctrl, bool shift) {
    if (vk != letter_vk_ || !ctrl || !shift) return false;
    if (already_fired_) return false;   // 自动重复去抖
    already_fired_ = true;
    return true;
}

void SchemeHotkeyDetector::OnKeyUp(int vk) {
    if (vk == letter_vk_) already_fired_ = false;
}

}  // namespace myabc::tsf
