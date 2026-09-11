// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/session/session_manager.hpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2
//
// DECISION: M1 所有 session 共享同一个 LibPinyinEngine/SourceRegistry（单一全局拼音实例）。
// 多窗口同时组字会互相干扰——真正的 per-session pinyin_instance_t 隔离留 M2 引擎生命周期
// 加固时处理。见 docs/decisions/_debt-log.md 2026-09-11。M1 的记事本单窗口验收场景不受影响。

#ifndef MYABC_ENGINE_SESSION_MANAGER_HPP
#define MYABC_ENGINE_SESSION_MANAGER_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "session.hpp"

namespace myabc::engine {

class SessionManager {
public:
    SessionManager(LibPinyinEngine& engine, SourceRegistry& registry, SessionOptions opts);

    // 不存在则创建。
    Session& GetOrCreate(std::uint32_t session_id);
    void Remove(std::uint32_t session_id);

private:
    LibPinyinEngine& engine_;
    SourceRegistry& registry_;
    SessionOptions opts_;
    std::unordered_map<std::uint32_t, std::unique_ptr<Session>> sessions_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_SESSION_MANAGER_HPP
