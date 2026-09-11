// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/text_convert.cpp

#include "text_convert.hpp"

#include <windows.h>

namespace myabc::tsf {

std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()),
                                        nullptr, 0);
    if (n <= 0) return {};
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return w;
}

std::string Narrow(const std::wstring& utf16) {
    if (utf16.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()),
                                        nullptr, 0, nullptr, nullptr);
    if (n <= 0) return {};
    std::string s(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, utf16.data(), static_cast<int>(utf16.size()), s.data(), n,
                          nullptr, nullptr);
    return s;
}

}  // namespace myabc::tsf
