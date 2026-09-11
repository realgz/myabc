// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/text_convert.hpp --- UTF-8 <-> UTF-16 小工具
//
// 依据：code-quality-standards.md（纯技术、无业务逻辑的工具函数）
//
// IPC 线上一律 UTF-8（system-overview §4.1 JSON body）；TSF/Win32 API 一律 UTF-16。
// 本文件是这两者之间唯一的转换点，避免同样的 WideCharToMultiByte/MultiByteToWideChar
// 样板代码在 ipc_client / myabc_text_service / composition_state 里各写一份。

#ifndef MYABC_TSF_TEXT_CONVERT_HPP
#define MYABC_TSF_TEXT_CONVERT_HPP

#include <string>

namespace myabc::tsf {

std::wstring Widen(const std::string& utf8);
std::string Narrow(const std::wstring& utf16);

}  // namespace myabc::tsf

#endif  // MYABC_TSF_TEXT_CONVERT_HPP
