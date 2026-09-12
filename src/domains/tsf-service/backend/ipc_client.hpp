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

#include <atomic>
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

    // 连接（超时内重试；失败则尝试 CreateProcess 引擎再重试一次）。会阻塞调用线程
    // 最长 cfg_.connect_timeout_ms——DECISION（真机反馈"启动/打字冻结"，见
    // docs/decisions/_debt-log.md 2026-09-12）：这个方法本身没有删，但热路径
    // （OnKeyDown/激活）不应该再直接调它，改用下面的 EnsureConnectedAsync()。
    bool Connect();
    void Disconnect();
    bool connected() const noexcept { return pipe_ != INVALID_HANDLE_VALUE; }

    // 非阻塞版本，取代热路径里原来的 Connect()：
    // - 已连接：立即返回 true。
    // - 未连接、后台没有连接线程在跑：启动一个后台线程做"CreateProcess 拉起引擎 +
    //   循环重试打开管道"（原 Connect() 的逻辑搬过去跑），立即返回 false——调用方
    //   应该把这次按键当"引擎还没接住"降级处理（原样插入字符），不阻塞 UI 线程。
    // - 未连接、后台线程正在跑：看它是否刚好连上了（PickUpPendingConnection），
    //   有就采用并返回 true，没有立即返回 false，既不阻塞也不重复开线程。
    bool EnsureConnectedAsync();

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
    // ctrl：Ctrl 键是否按住（用户 2026-09-12 要求：Ctrl+数字不用先按空格就能直接
    // 选字，见 src/domains/input-engine/logic/session/session.cpp DECISION）。
    bool ProcessKey(std::uint32_t session_id, int vk, unsigned ch, bool ctrl, ipc::Response& out);
    bool SelectCandidate(std::uint32_t session_id, int index, ipc::Response& out);
    bool PageCandidates(std::uint32_t session_id, int delta, ipc::Response& out);
    bool CommitComposition(std::uint32_t session_id, ipc::Response& out);
    bool CancelComposition(std::uint32_t session_id, ipc::Response& out);
    bool FocusOut(std::uint32_t session_id);

    // M2（plan 03 §3.2）：composing=true 时，TIP 应用完 ITfComposition 后把光标屏幕矩形
    // 告诉引擎——引擎据此把候选明细 + 这个矩形一起推给 myabc-ui（见 uiShow）。
    // 单向语义强，失败无需特殊处理（下次按键会再报一次新矩形）。
    bool SetCaretRect(std::uint32_t session_id, int x, int y, int w, int h);

private:
    bool TryOpenPipe();
    bool LaunchEngine();
    std::ptrdiff_t BoundedRead(void* buf, std::size_t n, DWORD timeout_ms);
    std::ptrdiff_t BoundedWrite(const void* buf, std::size_t n, DWORD timeout_ms);
    bool CallMethod(ipc::Method method, const ipc::Json& params, ipc::Response& out);

    // EnsureConnectedAsync() 的后台线程实现。
    static DWORD WINAPI ConnectThreadProc(LPVOID param);
    void RunConnectInBackground();   // 后台线程体：LaunchEngine + 循环 TryOpenPipe，
                                     // 成功把 HANDLE 存进 pending_pipe_（加锁），
                                     // 全程不碰 pipe_（那是主线程独占的）。
    bool PickUpPendingConnection();  // 主线程调用：加锁取走 pending_pipe_（如果有）
                                     // 赋给 pipe_，从此这个 HANDLE 只由主线程碰。

    IpcClientConfig cfg_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE;   // 只由主线程读写（EnsureConnectedAsync 的
                                           // 调用线程），后台连接线程不碰它。
    HANDLE read_event_ = nullptr;
    HANDLE write_event_ = nullptr;
    std::uint32_t next_id_ = 1;

    // 异步连接状态（见 EnsureConnectedAsync 头注释）。pending_pipe_/pending_lock_
    // 是后台线程和主线程之间唯一的共享数据，其它成员各自独占，不需要额外加锁。
    CRITICAL_SECTION pending_lock_{};
    HANDLE pending_pipe_ = INVALID_HANDLE_VALUE;
    HANDLE connect_thread_ = nullptr;
    std::atomic<bool> connect_thread_running_{false};
    std::atomic<bool> cancel_requested_{false};   // 析构时置位，让后台线程尽快退出
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_IPC_CLIENT_HPP
