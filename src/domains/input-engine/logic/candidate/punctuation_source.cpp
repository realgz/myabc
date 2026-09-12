// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/punctuation_source.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2；assets/config/punctuation.toml

#include "punctuation_source.hpp"

#include <array>
#include <cstring>

namespace myabc::engine {

namespace {
struct PunctMap {
    char ascii;
    const char* utf8_full_width;
};

// M1 覆盖 assets/config/punctuation.toml 里标了 "m1" 的条目；引号配对（‘’“”）状态机式
// 全角化留 M3+（见该文件注释）。
constexpr std::array<PunctMap, 8> kMap{{
    {',', "\xef\xbc\x8c"},   // ，
    {'.', "\xe3\x80\x82"},   // 。
    {';', "\xef\xbc\x9b"},   // ；
    {':', "\xef\xbc\x9a"},   // ：
    {'?', "\xef\xbc\x9f"},   // ？
    {'!', "\xef\xbc\x81"},   // ！
    {'(', "\xef\xbc\x88"},   // （
    {')', "\xef\xbc\x89"},   // ）
}};
}  // namespace

bool PunctuationSource::IsMappedPunctuation(char ascii) {
    for (const auto& m : kMap) {
        if (m.ascii == ascii) return true;
    }
    return false;
}

bool PunctuationSource::Handles(const InputContext& ctx) const {
    return ctx.mode == InputMode::kChinese && ctx.raw.size() == 1 &&
           IsMappedPunctuation(ctx.raw[0]);
}

std::vector<CandidateItem> PunctuationSource::Produce(const InputContext& ctx) {
    for (const auto& m : kMap) {
        if (m.ascii == ctx.raw[0]) return {CandidateItem{.text = m.utf8_full_width, .is_sentence = false}};
    }
    return {};
}

}  // namespace myabc::engine
