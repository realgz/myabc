// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/config/guids.hpp --- myabc 设计常量 GUID（非配置项）
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.2 / §3.4
//
// DECISION: 这些 GUID 是设计常量，不是配置——一旦发布不可更改（改了等于换一个输入法，
//           旧注册残留无法清理）。
//   CLSID_MyabcTextService     {8BA238DD-B6B4-42B4-95C1-8062D25B3652}（生成于 2026-09-10）
//   GUID_MyabcProfile          {C4FBBA94-26BC-4C7A-B9F1-4F91A723C60A}（生成于 2026-09-10）
//   GUID_MyabcSchemeLangBar    {68B73434-2D30-4DB5-B479-C4B7A5E96B12}（生成于 2026-09-13，
//     语言栏输入方案状态指示按钮，见 docs/decisions/input-engine/20260913-wubi-input-scheme.md）
//
// 定义在 guids.cpp（唯一 translation unit），tsf-service DLL 与 deployer 各自链接同一 .cpp。

#ifndef MYABC_CONFIG_GUIDS_HPP
#define MYABC_CONFIG_GUIDS_HPP

#include <guiddef.h>

extern "C" {
extern const CLSID CLSID_MyabcTextService;
extern const GUID  GUID_MyabcProfile;
extern const GUID  GUID_MyabcSchemeLangBar;
}

#endif  // MYABC_CONFIG_GUIDS_HPP
