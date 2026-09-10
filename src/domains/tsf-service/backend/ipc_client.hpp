// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/ipc_client.hpp --- TIP 侧命名管道客户端
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（ipc_client）
//       docs/architecture/system-overview.md §4 / §7 不变量 1（不链 libpinyin）、3、6
//
// M0：Activate 时 Connect 一次 + 发一次 hello，日志记录响应；不进按键热路径。
// 只依赖 myabc::ipc（标准库）+ Win32 管道 API。

#ifndef MYABC_TSF_IPC_CLIENT_HPP
#define MYABC_TSF_IPC_CLIENT_HPP

#include <windows.h>

#include <cstdint>
#include <string>

#include "json_codec.hpp"

namespace myabc::tsf {

struct IpcClientConfig {
    std::wstring pipe_name;          // 已展开 {sid}
    std::wstring engine_exe_path;    // 连接失败时用于拉起引擎
    std::uint32_t connect_timeout_ms = 2000;
    std::uint32_t request_timeout_ms = 50;   // M0 hello 用；按键路径 M2 才接
};

class IpcClient {
public:
    explicit IpcClient(IpcClientConfig cfg);
    ~IpcClient();

    IpcClient(const IpcClient&) = delete;
    IpcClient& operator=(const IpcClient&) = delete;

    // 连接（超时内重试；失败则尝试 CreateProcess 引擎再重试一次）。
    bool Connect();
    void Disconnect();
    bool connected() const noexcept { return pipe_ != INVALID_HANDLE_VALUE; }

    // 同步一问一答。失败返回 false（连接可能已断）。
    bool Call(const ipc::Request& req, ipc::Response& resp);

    // 便捷：发 hello，返回引擎版本串（失败为空）。
    std::string Hello();

private:
    bool TryOpenPipe();
    bool LaunchEngine();

    IpcClientConfig cfg_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    std::uint32_t next_id_ = 1;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_IPC_CLIENT_HPP
