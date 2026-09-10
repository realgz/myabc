// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/dll_refcount.hpp --- 进程内全局对象计数
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（DllCanUnloadNow 依对象计数）
//       docs/architecture/system-overview.md §7 不变量 4

#ifndef MYABC_TSF_DLL_REFCOUNT_HPP
#define MYABC_TSF_DLL_REFCOUNT_HPP

namespace myabc::tsf {

void DllAddRef() noexcept;     // 每个存活的 COM 对象 +1
void DllRelease() noexcept;    // 对象析构 -1
void DllLockServer() noexcept;   // IClassFactory::LockServer(TRUE)
void DllUnlockServer() noexcept; // IClassFactory::LockServer(FALSE)
bool DllCanUnload() noexcept;    // 对象计数 == 0 且 lock 计数 == 0

void* DllInstanceHandle() noexcept;         // HINSTANCE（DllMain 记录）
void  SetDllInstanceHandle(void* hinst) noexcept;

}  // namespace myabc::tsf

#endif  // MYABC_TSF_DLL_REFCOUNT_HPP
