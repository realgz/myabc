// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/single_instance.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5

#include "single_instance.hpp"

#include <windows.h>

namespace myabc::engine {

SingleInstanceGuard::SingleInstanceGuard(const std::string& mutex_name) {
    // 名字里可能含 '\\'（Local\...），CreateMutexA 接受。
    HANDLE h = ::CreateMutexA(nullptr, TRUE, mutex_name.c_str());
    const DWORD err = ::GetLastError();
    handle_ = h;
    // 拿到句柄且不是 "already exists" 才算获得所有权。
    acquired_ = (h != nullptr) && (err != ERROR_ALREADY_EXISTS);
}

SingleInstanceGuard::~SingleInstanceGuard() {
    if (handle_ != nullptr) {
        if (acquired_) {
            ::ReleaseMutex(static_cast<HANDLE>(handle_));
        }
        ::CloseHandle(static_cast<HANDLE>(handle_));
    }
}

}  // namespace myabc::engine
