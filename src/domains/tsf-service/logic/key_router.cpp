// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/key_router.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5

#include "key_router.hpp"

#include <windows.h>

#include <array>
#include <cwctype>

namespace myabc::tsf {

namespace {
bool IsAsciiLetter(wchar_t ch) {
    return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z');
}

constexpr std::array<wchar_t, 8> kPunctuationAscii{L',', L'.', L';', L':', L'?', L'!', L'(', L')'};
}  // namespace

bool KeyRouter::IsMappedPunctuationAscii(wchar_t ch) noexcept {
    for (wchar_t p : kPunctuationAscii) {
        if (p == ch) return true;
    }
    return false;
}

bool KeyRouter::IsInterestedKey(int vk, wchar_t ch, bool composing, InputMode mode, bool ctrl,
                                bool shift) const noexcept {
    // 方案切换热键优先于一切其它判断——包括英文模式（用户可能想从英文模式直接
    // 切到五笔/普通拼音打字），见类头 2026-09-13 DECISION。
    if (scheme_hotkey_vk_ != 0 && vk == scheme_hotkey_vk_ && ctrl && shift) return true;

    if (mode == InputMode::kEnglish) return false;   // M1-9：直接放行

    if (composing) {
        if (vk == VK_BACK || vk == VK_ESCAPE || vk == VK_SPACE) return true;
        if (ch != L'\0') {
            if (candidates_cfg_.select_keys.find(static_cast<char>(ch)) != std::string::npos) {
                return true;
            }
            if (candidates_cfg_.page_prev_keys.find(static_cast<char>(ch)) != std::string::npos) {
                return true;
            }
            if (candidates_cfg_.page_next_keys.find(static_cast<char>(ch)) != std::string::npos) {
                return true;
            }
            if (IsAsciiLetter(ch)) return true;
        }
        return false;
    }

    if (ch == L'\0') return false;
    return IsAsciiLetter(ch) || IsMappedPunctuationAscii(ch);
}

}  // namespace myabc::tsf
