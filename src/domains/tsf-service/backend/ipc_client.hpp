// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/ipc_client.hpp --- TIP 侧命名管道客户端
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（ipc_client）
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5（有界等待、超时降级，M1-12）
//       docs/architecture/system-overview.md §4 / §7 不变量 1（不链 libpinyin）、3、6
//
// M1：全量请求方法（processKey 等）+ overlapped IO 有界等待。宿主 UI 线程调用 Call()
// 时最多阻塞 timeout_ms（默认 request_timeout_ms=50ms）——超时按"未处理"降级，
// 绝不无界等待（不变量 3）。只依赖 myabc::ipc（标准库）+ Win32 管道 API。

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
    std::uint32_t request_timeout_ms = 50;   // 按键热路径超时预算
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

    // 同步一问一答，最多等 timeout_ms（默认走 cfg_.request_timeout_ms）。
    // 超时或 IO 错误：断开连接并返回 false——调用方应把这次按键当"引擎没接住"处理，
    // 不得再重试同一次调用（避免宿主 UI 线程雪崩式累积等待）。
    bool Call(const ipc::Request& req, ipc::Response& resp);
    bool CallWithTimeout(const ipc::Request& req, ipc::Response& resp, std::uint32_t timeout_ms);

    // 便捷：发 hello，返回引擎版本串（失败为空）。用 connect_timeout_ms 预算。
    std::string Hello();

    // M1 全量请求（每个都是"编包 -> CallWithTimeout(request_timeout_ms) -> 解包"）。
    // 失败（含超时）时 out 不变，返回 false。
    bool InitSession(std::uint32_t session_id);
    bool ProcessKey(std::uint32_t session_id, int vk, unsigned ch, ipc::Response& out);
    bool SelectCandidate(std::uint32_t session_id, int index, ipc::Response& out);
    bool PageCandidates(std::uint32_t session_id, int delta, ipc::Response& out);
    bool CommitComposition(std::uint32_t session_id, ipc::Response& out);
    bool CancelComposition(std::uint32_t session_id, ipc::Response& out);
    bool FocusOut(std::uint32_t session_id);

private:
    bool TryOpenPipe();
    bool LaunchEngine();
    std::ptrdiff_t BoundedRead(void* buf, std::size_t n, DWORD timeout_ms);
    std::ptrdiff_t BoundedWrite(const void* buf, std::size_t n, DWORD timeout_ms);
    bool CallMethod(ipc::Method method, const ipc::Json& params, ipc::Response& out);

    IpcClientConfig cfg_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;
    HANDLE read_event_ = nullptr;
    HANDLE write_event_ = nullptr;
    std::uint32_t next_id_ = 1;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_IPC_CLIENT_HPP
