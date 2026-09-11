// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/pipe_server.hpp --- 命名管道服务端
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/plan/03-m2-engine-process-ipc-plan.md §3.4（overlapped connect + 空闲自退出）
//       docs/architecture/system-overview.md §4
//
// M2：overlapped connect + idle_exit_minutes 空闲自退出（accept 等待用 WaitForSingleObject
// 加超时，不是无界阻塞）。单线程服务单个已连接客户端；多连接并发仍是后续里程碑范围。

#ifndef MYABC_ENGINE_PIPE_SERVER_HPP
#define MYABC_ENGINE_PIPE_SERVER_HPP

#include <string>

#include "dispatcher.hpp"

namespace myabc::engine {

struct PipeServerOptions {
    std::string pipe_name;          // 已展开 {sid} 的完整管道名
    unsigned idle_exit_minutes = 10;  // 连续空闲这么久无新连接则自退出（0 = 永不）
    // 测试用：> 0 时覆盖 idle_exit_minutes，以秒为单位（M2-4 验收要求极小值如 6 秒，
    // 不必等分钟级默认值）。见 --idle-exit-seconds。
    unsigned idle_exit_seconds_override = 0;
};

// 阻塞运行直到 dispatcher 收到 shutdown 或达到空闲超时。返回进程退出码。
int RunPipeServer(const PipeServerOptions& opts, Dispatcher& dispatcher);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_PIPE_SERVER_HPP
