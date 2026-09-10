// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/backend/tsf_register.hpp --- TSF profile / category 注册
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4
//       docs/reference/windows-ime-framework-reference.md
//
// 参考微软 SampleIME（MIT）Profile.cpp / Register.cpp。
// DECISION: docs/decisions/_debt-log.md（来源登记）。

#ifndef MYABC_DEPLOY_TSF_REGISTER_HPP
#define MYABC_DEPLOY_TSF_REGISTER_HPP

#include <windows.h>

#include <cstdint>
#include <string>

namespace myabc::deploy {

// ITfInputProcessorProfiles::Register + AddLanguageProfile
//   langid：注册用语言（system-overview §5，默认 zh-CN，见 config kDefaultLangId）
//   display_name：输入法切换菜单里显示的名字
//   icon_path：可空
HRESULT RegisterProfiles(std::uint16_t langid, const std::wstring& display_name,
                         const std::wstring& icon_path);
HRESULT UnregisterProfiles(std::uint16_t langid);

// ITfCategoryMgr::RegisterCategory 到 TIP_KEYBOARD + Win8 TIPCAP 集合。
HRESULT RegisterCategories();
HRESULT UnregisterCategories();

// 重新枚举，确认 CLSID_MyabcTextService 的 profile 在 langid 下已存在。
bool IsProfileRegistered(std::uint16_t langid);

}  // namespace myabc::deploy

#endif  // MYABC_DEPLOY_TSF_REGISTER_HPP
