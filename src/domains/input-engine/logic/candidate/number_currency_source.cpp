// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/number_currency_source.cpp
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.2

#include "number_currency_source.hpp"

#include <cctype>

#include "cn_number.hpp"

namespace myabc::engine {

namespace {
bool LooksLikeNumberBody(const std::string& body) {
    if (body.empty()) return false;
    bool has_digit = false;
    for (char c : body) {
        if (c == '.' || c == '-') continue;
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        has_digit = true;
    }
    return has_digit;
}
}  // namespace

bool NumberCurrencySource::Handles(const InputContext& ctx) const {
    if (ctx.mode != InputMode::kChinese || ctx.raw.size() < 2) return false;
    if (ctx.raw[0] != lead_key_) return false;
    return LooksLikeNumberBody(ctx.raw.substr(1));
}

std::vector<CandidateItem> NumberCurrencySource::Produce(const InputContext& ctx) {
    using namespace myabc::shared;

    const std::string body = ctx.raw.substr(1);
    std::vector<CandidateItem> items;

    if (body.find('.') == std::string::npos) {
        // 整数：数量读法 + 逐位读法(年份式) + 大写金额(整) + 原样。
        if (const auto n = ParseInteger(body)) {
            items.push_back(CandidateItem{.text = PlaceValueChinese(*n), .is_sentence = false});
            if (const auto digit_reading = DigitByDigitChinese(body)) {
                items.push_back(CandidateItem{.text = *digit_reading, .is_sentence = false});
            }
            if (const auto cents = ParseDecimalToCents(body)) {
                items.push_back(CandidateItem{.text = UppercaseCurrency(*cents), .is_sentence = false});
            }
        }
    } else if (const auto cents = ParseDecimalToCents(body)) {
        // 带小数点：视为金额，只给大写金额（数量/逐位读法对小数没有约定展示形式）。
        items.push_back(CandidateItem{.text = UppercaseCurrency(*cents), .is_sentence = false});
    }

    items.push_back(CandidateItem{.text = body, .is_sentence = false});   // 原样阿拉伯数字兜底，永远可选
    return items;
}

}  // namespace myabc::engine
