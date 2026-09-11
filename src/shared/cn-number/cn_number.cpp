// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/cn-number/cn_number.cpp
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.2

#include "cn_number.hpp"

#include <array>
#include <cctype>
#include <cmath>

namespace myabc::shared {

namespace {

constexpr std::array<const char*, 10> kDigitCn = {"零", "一", "二", "三", "四",
                                                  "五", "六", "七", "八", "九"};
constexpr std::array<const char*, 10> kDigitUpper = {"零", "壹", "贰", "叁", "肆",
                                                     "伍", "陆", "柒", "捌", "玖"};
constexpr std::array<const char*, 4> kSmallUnitCn = {"", "十", "百", "千"};
constexpr std::array<const char*, 4> kSmallUnitUpper = {"", "拾", "佰", "仟"};
constexpr std::array<const char*, 4> kBigUnit = {"", "万", "亿", "万亿"};

// 核心算法：把 |n| 的十进制串按"数量读法"转成中文，digit_name/small_unit 决定
// 小写"二三四..."还是大写"贰叁肆..."（大写金额也复用这份 4 位分组 + 两/二处理逻辑）。
// use_liang：true 时把"作为整个数字最高位、且紧跟千/百/万/亿（不含十）"的 2 换成"两"
// （大写金额传统上不用"两"，只用小写数量读法——GB/T 15835 大写金额固定用"贰"）。
std::string ConvertUnsignedDigits(const std::string& digits,
                                  const std::array<const char*, 10>& digit_name,
                                  const std::array<const char*, 4>& small_unit,
                                  bool use_liang) {
    const auto L = static_cast<int>(digits.size());
    std::string result;
    bool any_output = false;   // 整个数字范围内是否已经输出过任何数字
    bool pending_zero = false;  // 跳过的非末尾零，待下一个非零数字前补一个"零"
    int current_group = -1;    // 当前处理到第几个万级分组（0=个级，1=万，2=亿，3=万亿）
    bool group_has_content = false;

    auto flush_group = [&](int group_idx) {
        if (group_has_content && group_idx > 0 && group_idx < static_cast<int>(kBigUnit.size())) {
            result += kBigUnit[static_cast<std::size_t>(group_idx)];
        }
    };

    for (int i = 0; i < L; ++i) {
        const int d = digits[static_cast<std::size_t>(i)] - '0';
        const int p = L - 1 - i;   // 从右往左的位序：0=个,1=十,2=百,3=千,4=个(万级)...
        const int g = p / 4;
        const int u = p % 4;

        if (g != current_group) {
            if (current_group >= 0) flush_group(current_group);
            current_group = g;
            group_has_content = false;
        }

        if (d == 0) {
            if (any_output) pending_zero = true;
            continue;
        }

        if (pending_zero) {
            result += kDigitCn[0];   // 组间补零永远用"零"（大写金额里"零"字形一致，不区分大小写）
            pending_zero = false;
        }

        const bool use_two = use_liang && d == 2 && !any_output &&
                            (u == 2 || u == 3 || (u == 0 && g > 0));
        if (u == 1 && d == 1 && !any_output) {
            result += small_unit[1];   // 最高位是"十几"时省略前导"一"：15 -> 十五，非 一十五
        } else {
            result += use_two ? "两" : digit_name[static_cast<std::size_t>(d)];
            if (u > 0) result += small_unit[static_cast<std::size_t>(u)];
        }
        any_output = true;
        group_has_content = true;
    }
    flush_group(current_group);

    return result.empty() ? kDigitCn[0] : result;
}

std::string ToUnsignedDigitString(std::uint64_t n) {
    if (n == 0) return "0";
    std::string s;
    while (n > 0) {
        s.push_back(static_cast<char>('0' + n % 10));
        n /= 10;
    }
    return std::string(s.rbegin(), s.rend());
}

}  // namespace

std::optional<std::string> DigitByDigitChinese(const std::string& digits) {
    if (digits.empty()) return std::nullopt;
    // 逐位读法的"0"用〇（年份习惯：二〇二五），不是数量读法补零用的"零"
    // （一千零五）——两个字形在这个场景下不通用，弄混是常见笔误。
    static constexpr std::array<const char*, 10> kDigitYearStyle = {
        "〇", "一", "二", "三", "四", "五", "六", "七", "八", "九"};
    std::string result;
    for (char c : digits) {
        if (c == '-') {
            result += "负";
            continue;
        }
        if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
        result += kDigitYearStyle[static_cast<std::size_t>(c - '0')];
    }
    return result;
}

std::string PlaceValueChinese(std::int64_t n) {
    if (n == 0) return "零";
    const std::string prefix = n < 0 ? "负" : "";
    const std::uint64_t un = n < 0 ? static_cast<std::uint64_t>(-(n + 1)) + 1
                                   : static_cast<std::uint64_t>(n);
    return prefix + ConvertUnsignedDigits(ToUnsignedDigitString(un), kDigitCn, kSmallUnitCn,
                                          /*use_liang=*/true);
}

std::string UppercaseCurrency(std::int64_t amount_cents) {
    const std::string prefix = amount_cents < 0 ? "负" : "";
    std::uint64_t cents = amount_cents < 0 ? static_cast<std::uint64_t>(-(amount_cents + 1)) + 1
                                           : static_cast<std::uint64_t>(amount_cents);

    const std::uint64_t yuan = cents / 100;
    const int jiao = static_cast<int>(cents / 10 % 10);
    const int fen = static_cast<int>(cents % 10);

    std::string result = prefix;
    result += ConvertUnsignedDigits(ToUnsignedDigitString(yuan), kDigitUpper, kSmallUnitUpper,
                                    /*use_liang=*/false);
    result += "圆";

    if (jiao == 0 && fen == 0) {
        result += "整";
        return result;
    }
    if (jiao == 0) {
        result += "零";
    } else {
        result += kDigitUpper[static_cast<std::size_t>(jiao)];
        result += "角";
    }
    if (fen != 0) {
        result += kDigitUpper[static_cast<std::size_t>(fen)];
        result += "分";
    }
    return result;
}

std::optional<std::int64_t> ParseInteger(const std::string& text) {
    if (text.empty()) return std::nullopt;
    if (text.find('.') != std::string::npos) return std::nullopt;

    std::size_t i = 0;
    bool neg = false;
    if (text[0] == '-') {
        neg = true;
        i = 1;
    }
    if (i >= text.size()) return std::nullopt;

    std::int64_t value = 0;
    for (; i < text.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(text[i]))) return std::nullopt;
        value = value * 10 + (text[i] - '0');
    }
    return neg ? -value : value;
}

std::optional<std::int64_t> ParseDecimalToCents(const std::string& text) {
    if (text.empty()) return std::nullopt;

    std::size_t i = 0;
    bool neg = false;
    if (text[0] == '-') {
        neg = true;
        i = 1;
    }
    if (i >= text.size()) return std::nullopt;

    const auto dot = text.find('.', i);
    std::string int_part = text.substr(i, dot == std::string::npos ? std::string::npos : dot - i);
    std::string frac_part = dot == std::string::npos ? "" : text.substr(dot + 1);

    if (int_part.empty() && frac_part.empty()) return std::nullopt;
    for (char c : int_part) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
    }
    for (char c : frac_part) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
    }
    if (frac_part.size() > 2) {
        // 四舍五入到分：看第 3 位小数决定是否进位到第 2 位。
        const bool round_up = frac_part[2] >= '5';
        frac_part = frac_part.substr(0, 2);
        if (round_up) {
            int carry = 1;
            for (int k = 1; k >= 0 && carry; --k) {
                int v = (frac_part[static_cast<std::size_t>(k)] - '0') + carry;
                carry = v / 10;
                frac_part[static_cast<std::size_t>(k)] = static_cast<char>('0' + v % 10);
            }
            if (carry) {
                // 角分进位满 1 元：简单起见对整数部分 +1（int_part 可能为空 = 0）。
                std::int64_t whole = int_part.empty() ? 0 : std::stoll(int_part);
                int_part = std::to_string(whole + 1);
            }
        }
    }
    while (frac_part.size() < 2) frac_part.push_back('0');

    const std::int64_t whole = int_part.empty() ? 0 : std::stoll(int_part);
    const std::int64_t cents = whole * 100 + std::stoll(frac_part.empty() ? "0" : frac_part);
    return neg ? -cents : cents;
}

}  // namespace myabc::shared
