// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/pipe_server.hpp --- 命名管道服务端
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/architecture/system-overview.md §4
//
// M0：单线程 accept 循环，每次服务一个连接（同步 read_frame -> dispatch -> write_frame），
// 连接断开后回到 accept。多连接并发留到 M2（§3 计划 03）。

#ifndef MYABC_ENGINE_PIPE_SERVER_HPP
#define MYABC_ENGINE_PIPE_SERVER_HPP

#include <string>

#include "dispatcher.hpp"

namespace myabc::engine {

struct PipeServerOptions {
    std::string pipe_name;          // 已展开 {sid} 的完整管道名
    unsigned idle_exit_minutes = 10;  // 连续空闲这么久无新连接则自退出（0 = 永不）
};

// 阻塞运行直到 dispatcher 收到 shutdown 或达到空闲超时。返回进程退出码。
int RunPipeServer(const PipeServerOptions& opts, Dispatcher& dispatcher);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_PIPE_SERVER_HPP
