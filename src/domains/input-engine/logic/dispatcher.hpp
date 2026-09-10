// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/dispatcher.hpp --- 请求 -> 响应 的纯逻辑
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/architecture/system-overview.md §4.1
//
// 纯逻辑层：不碰管道 / windows.h，输入 ipc::Request，输出 ipc::Response，便于单测。
// M0：hello -> {engineVersion, protocol}；processKey -> {handled:false}（占位）；
//     shutdown -> 置 should_shutdown 标志。

#ifndef MYABC_ENGINE_DISPATCHER_HPP
#define MYABC_ENGINE_DISPATCHER_HPP

#include "json_codec.hpp"
#include "protocol.hpp"

namespace myabc::engine {

class Dispatcher {
public:
    // 处理一个请求，产出响应。未知/未实现方法回结构化错误。
    ipc::Response Handle(const ipc::Request& req);

    // shutdown 收到后置位；主循环据此优雅退出。
    bool should_shutdown() const noexcept { return should_shutdown_; }

    static constexpr const char* kEngineVersion = "0.0.0-m0";

private:
    ipc::Response HandleHello(const ipc::Request& req);
    ipc::Response HandleProcessKey(const ipc::Request& req);

    bool should_shutdown_ = false;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_DISPATCHER_HPP
