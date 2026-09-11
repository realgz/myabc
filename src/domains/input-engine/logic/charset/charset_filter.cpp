// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/charset/charset_filter.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2

#include "charset_filter.hpp"

#include <windows.h>

namespace myabc::engine {

namespace {

// 透传：M1/M2 默认路径，永远可表示。
class UnicodeFilter final : public CharsetFilter {
public:
    bool CanRepresent(const std::string&) const override { return true; }
};

// 用 Win32 codepage 936（GBK）做可表示性判定：WideCharToMultiByte 遇到映射不到的字符会
// 用默认替代符且置位 usedDefaultChar。
//
// DECISION: GB2312 是 GBK 的严格子集，Windows 没有独立的 GB2312 代码页；精确区分
// （M4"GBK 生僻字"范围）需要查表而非代码页转换。M1 用同一实现近似两者，Gb2312Filter
// 的"过严"留 M4 与生僻字候选策略一起解决。见 docs/decisions/_debt-log.md 2026-09-11。
class GbkFilter final : public CharsetFilter {
public:
    bool CanRepresent(const std::string& text) const override {
        if (text.empty()) return true;

        const int wlen =
            ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (wlen <= 0) return false;
        std::wstring wide(static_cast<std::size_t>(wlen), L'\0');
        ::MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(),
                              wlen);

        BOOL used_default = FALSE;
        const int mlen = ::WideCharToMultiByte(936, WC_NO_BEST_FIT_CHARS, wide.data(), wlen, nullptr,
                                               0, "?", &used_default);
        return mlen > 0 && !used_default;
    }
};

}  // namespace

std::unique_ptr<CharsetFilter> ResolveCharsetFilter(const std::string& charset_name) {
    if (charset_name == "gbk" || charset_name == "gb2312") {
        return std::make_unique<GbkFilter>();
    }
    return std::make_unique<UnicodeFilter>();
}

}  // namespace myabc::engine
