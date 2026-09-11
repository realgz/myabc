// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/charset/charset_filter.hpp --- 输出字符集过滤适配器
//
// 依据：docs/architecture/system-overview.md §5（output.charset）/ §6（CharsetFilter 适配器）
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2
//
// 用途：候选/commit 文本外发前按 config.output.charset 校验是否可表示；不可表示时
// 该做什么由调用方决定（M1：不过滤候选，只在需要时提供判定——GB2312/GBK 精确区分
// 与生僻字替代策略是 M4 范围，见 docs/decisions/_debt-log.md 2026-09-11）。

#ifndef MYABC_ENGINE_CHARSET_FILTER_HPP
#define MYABC_ENGINE_CHARSET_FILTER_HPP

#include <memory>
#include <string>

namespace myabc::engine {

class CharsetFilter {
public:
    virtual ~CharsetFilter() = default;
    // text（UTF-8）在本字符集下是否可完整表示。
    virtual bool CanRepresent(const std::string& text) const = 0;
};

// 按 config.output.charset（"unicode" | "gb2312" | "gbk"）取实现；未知值回退 UnicodeFilter。
std::unique_ptr<CharsetFilter> ResolveCharsetFilter(const std::string& charset_name);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_CHARSET_FILTER_HPP
