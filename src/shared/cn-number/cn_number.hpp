// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/cn-number/cn_number.hpp --- 阿拉伯数字 <-> 中文数字/大写金额 纯函数库
//
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.2
//       code-quality-standards.md §1（纯技术、无业务逻辑，放 src/shared）
//
// 全是无状态纯函数，两套工具链都能编（只用标准库）。金额一律用"分"为最小单位的
// 整数运算，不经浮点，避免精度问题（0.1+0.2 式的经典坑）。

#ifndef MYABC_SHARED_CN_NUMBER_HPP
#define MYABC_SHARED_CN_NUMBER_HPP

#include <cstdint>
#include <optional>
#include <string>

namespace myabc::shared {

// 逐位读法（年份/编号式）："2025" -> "二〇二五"；"10" -> "一〇"。
// 输入必须是纯数字（可选前导 '-'，逐位读法一般不用于负数场景，但仍支持）。
// 输入非法（含非数字字符）时返回 std::nullopt。
std::optional<std::string> DigitByDigitChinese(const std::string& digits);

// 数量读法（口语习惯，含"两"的用法）：2025 -> "两千零二十五"；0 -> "零"；-5 -> "负五"。
// 只支持整数（小数部分由 UppercaseCurrency 或调用方另行处理）。
std::string PlaceValueChinese(std::int64_t n);

// 大写金额（参照 GB/T 15835 数字用法规范）：amount_cents 是"分"为单位的整数金额
// （如 123456 分 = 1234.56 元），避免浮点误差。示例：
//   123456 -> "壹仟贰佰叁拾肆圆伍角陆分"
//   10000  -> "壹佰圆整"
//   10005  -> "壹佰圆零伍分"
// 负数在前面加"负"。
std::string UppercaseCurrency(std::int64_t amount_cents);

// 解析形如 "1234.56" / "-5" / "100" 的十进制数字文本为"分"为单位的整数
// （必要时四舍五入到分）。非法输入（空串、多个小数点、超过 2 位小数之外的更多小数
// 仍按截断/四舍五入处理、非数字字符等）返回 std::nullopt。
std::optional<std::int64_t> ParseDecimalToCents(const std::string& text);

// 从同一段十进制文本解析出整数部分（用于 PlaceValueChinese/DigitByDigitChinese 的入参）。
// 只有整数（无小数点）时才返回值；含小数点返回 std::nullopt（调用方应改用 ParseDecimalToCents）。
std::optional<std::int64_t> ParseInteger(const std::string& text);

}  // namespace myabc::shared

#endif  // MYABC_SHARED_CN_NUMBER_HPP
