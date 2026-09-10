// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/backend/com_register.hpp --- COM 自注册（InprocServer32）
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4
//       docs/architecture/system-overview.md §7 不变量 4

#ifndef MYABC_DEPLOY_COM_REGISTER_HPP
#define MYABC_DEPLOY_COM_REGISTER_HPP

#include <windows.h>

#include <string>

namespace myabc::deploy {

// 写 HKEY_CLASSES_ROOT\CLSID\{CLSID_MyabcTextService}\InprocServer32
//   (默认) = dll_path，ThreadingModel = Apartment
// dll_path 为空则用当前模块（deployer 所在目录）下的 myabc-tip.dll。
HRESULT RegisterComServer(const std::wstring& dll_path);

// 删除 HKCR\CLSID\{CLSID_MyabcTextService} 整个键（含 InprocServer32）。
HRESULT UnregisterComServer();

// 查询：InprocServer32 默认值是否存在且指向一个真实文件。out_path 回填当前登记路径。
bool QueryComServer(std::wstring& out_path);

}  // namespace myabc::deploy

#endif  // MYABC_DEPLOY_COM_REGISTER_HPP
