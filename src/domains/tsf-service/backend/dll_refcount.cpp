// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/dll_refcount.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3

#include "dll_refcount.hpp"

#include <windows.h>

namespace myabc::tsf {

namespace {
LONG g_objects = 0;
LONG g_locks = 0;
void* g_hinst = nullptr;
}  // namespace

void DllAddRef() noexcept { ::InterlockedIncrement(&g_objects); }
void DllRelease() noexcept { ::InterlockedDecrement(&g_objects); }
void DllLockServer() noexcept { ::InterlockedIncrement(&g_locks); }
void DllUnlockServer() noexcept { ::InterlockedDecrement(&g_locks); }

bool DllCanUnload() noexcept {
    return ::InterlockedCompareExchange(&g_objects, 0, 0) == 0 &&
           ::InterlockedCompareExchange(&g_locks, 0, 0) == 0;
}

void* DllInstanceHandle() noexcept { return g_hinst; }
void SetDllInstanceHandle(void* hinst) noexcept { g_hinst = hinst; }

}  // namespace myabc::tsf
