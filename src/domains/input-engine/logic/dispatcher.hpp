// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/dispatcher.hpp --- 请求 -> 响应 的纯逻辑
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.3
//       docs/architecture/system-overview.md §4.1
//
// 纯逻辑层：不碰管道 / windows.h，输入 ipc::Request，输出 ipc::Response。
// M1：initSession/processKey/selectCandidate/pageCandidates/commitComposition/
//     cancelComposition/focusIn/focusOut 全部实现；setConfig 仍是 M1 未实现方法。

#ifndef MYABC_ENGINE_DISPATCHER_HPP
#define MYABC_ENGINE_DISPATCHER_HPP

#include <cstdint>
#include <memory>

#include "candidate/source_registry.hpp"
#include "json_codec.hpp"
#include "libpinyin_wrapper.hpp"
#include "protocol.hpp"
#include "session/session_manager.hpp"

namespace myabc::engine {

class Dispatcher {
public:
    // engine 即使 Init() 失败（!engine.ready()）也必须是个有效对象——LibPinyinEngine 的
    // 各方法在未就绪时全部安全地空操作/返回空结果，因此 processKey 等会自然退化为
    // handled:false，不崩溃，等价 M0 占位行为。
    Dispatcher(LibPinyinEngine& engine, SessionOptions opts);

    ipc::Response Handle(const ipc::Request& req);

    bool should_shutdown() const noexcept { return should_shutdown_; }

    static constexpr const char* kEngineVersion = "0.1.0-m1";

private:
    ipc::Response HandleHello(const ipc::Request& req);
    ipc::Response HandleInitSession(const ipc::Request& req);
    ipc::Response HandleProcessKey(const ipc::Request& req);
    ipc::Response HandleSelectCandidate(const ipc::Request& req);
    ipc::Response HandlePageCandidates(const ipc::Request& req);
    ipc::Response HandleCommitComposition(const ipc::Request& req);
    ipc::Response HandleCancelComposition(const ipc::Request& req);
    ipc::Response HandleFocusOut(const ipc::Request& req);

    ipc::Response SessionResultToResponse(std::uint32_t id, const SessionResult& r) const;

    LibPinyinEngine& engine_;
    SourceRegistry registry_;
    SessionManager sessions_;
    bool should_shutdown_ = false;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_DISPATCHER_HPP
