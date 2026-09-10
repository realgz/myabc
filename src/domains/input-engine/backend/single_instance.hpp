// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/single_instance.hpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/architecture/system-overview.md §4（引擎用命名互斥量保证单实例）

#ifndef MYABC_ENGINE_SINGLE_INSTANCE_HPP
#define MYABC_ENGINE_SINGLE_INSTANCE_HPP

#include <string>

namespace myabc::engine {

// 持有命名互斥量 Local\myabc-engine-<sid>。若已有实例，acquired() 返回 false。
class SingleInstanceGuard {
public:
    explicit SingleInstanceGuard(const std::string& mutex_name);
    ~SingleInstanceGuard();

    SingleInstanceGuard(const SingleInstanceGuard&) = delete;
    SingleInstanceGuard& operator=(const SingleInstanceGuard&) = delete;

    bool acquired() const noexcept { return acquired_; }

private:
    void* handle_ = nullptr;   // HANDLE
    bool acquired_ = false;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_SINGLE_INSTANCE_HPP
