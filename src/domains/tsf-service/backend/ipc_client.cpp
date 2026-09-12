// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/ipc_client.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5（有界等待，M1-12）

#include "ipc_client.hpp"

#include <string>

#include "frame.hpp"
#include "protocol.hpp"
#include "text_convert.hpp"

namespace myabc::tsf {

IpcClient::IpcClient(IpcClientConfig cfg) : cfg_(std::move(cfg)) {
    read_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    write_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ::InitializeCriticalSection(&pending_lock_);
}

IpcClient::~IpcClient() {
    // 后台连接线程可能还在跑（真机反馈的冷启动场景，见 EnsureConnectedAsync）；
    // 先示意它尽快退出再等它结束，避免它退出后访问已经开始析构的 this。
    // cancel_requested_ 让它在下一次循环检查点（≤200ms 粒度）就退出，不用真等满
    // connect_timeout_ms。
    cancel_requested_.store(true);
    if (connect_thread_ != nullptr) {
        ::WaitForSingleObject(connect_thread_, INFINITE);
        ::CloseHandle(connect_thread_);
    }
    if (pending_pipe_ != INVALID_HANDLE_VALUE) ::CloseHandle(pending_pipe_);
    ::DeleteCriticalSection(&pending_lock_);

    Disconnect();
    if (read_event_) ::CloseHandle(read_event_);
    if (write_event_) ::CloseHandle(write_event_);
}

bool IpcClient::TryOpenPipe() {
    // FILE_FLAG_OVERLAPPED：Call() 才能对单次 ReadFile/WriteFile 做有界等待（不变量 3）。
    const HANDLE h =
        ::CreateFileW(cfg_.pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                     OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = PIPE_READMODE_BYTE;
    ::SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
    pipe_ = h;
    return true;
}

bool IpcClient::LaunchEngine() {
    if (cfg_.engine_exe_path.empty()) return false;

    // 工作目录必须是引擎所在目录，否则它按相对路径找的 model_dir/user_dir
    // （config engine.model_dir 默认 "data"）会相对宿主进程（如 notepad.exe）的 CWD
    // 解析，几乎总是找不到——engine_exe_path 已是绝对路径（见 SelfDir() 拼接）。
    const auto slash = cfg_.engine_exe_path.find_last_of(L"\\/");
    const std::wstring engine_dir =
        slash == std::wstring::npos ? L"." : cfg_.engine_exe_path.substr(0, slash);

    std::wstring cmd = L"\"" + cfg_.engine_exe_path + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok =
        ::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, engine_dir.c_str(), &si, &pi);
    if (!ok) return false;
    ::CloseHandle(pi.hThread);
    ::CloseHandle(pi.hProcess);
    return true;
}

bool IpcClient::Connect() {
    if (connected()) return true;

    const DWORD deadline = ::GetTickCount() + cfg_.connect_timeout_ms;
    bool launched = false;
    for (;;) {
        if (TryOpenPipe()) return true;

        if (::GetLastError() == ERROR_PIPE_BUSY) {
            ::WaitNamedPipeW(cfg_.pipe_name.c_str(), 200);
        } else if (!launched) {
            launched = LaunchEngine();  // 引擎可能还没起来
            ::Sleep(100);
        } else {
            ::Sleep(100);
        }

        if (static_cast<LONG>(::GetTickCount() - deadline) >= 0) return false;
    }
}

bool IpcClient::PickUpPendingConnection() {
    HANDLE h = INVALID_HANDLE_VALUE;
    ::EnterCriticalSection(&pending_lock_);
    h = pending_pipe_;
    pending_pipe_ = INVALID_HANDLE_VALUE;
    ::LeaveCriticalSection(&pending_lock_);

    if (h == INVALID_HANDLE_VALUE) return false;
    pipe_ = h;   // 从这一刻起这个 HANDLE 只由主线程碰，安全（后台线程已经放手）。
    return true;
}

DWORD WINAPI IpcClient::ConnectThreadProc(LPVOID param) {
    static_cast<IpcClient*>(param)->RunConnectInBackground();
    return 0;
}

void IpcClient::RunConnectInBackground() {
    const DWORD deadline = ::GetTickCount() + cfg_.connect_timeout_ms;
    bool launched = false;
    for (;;) {
        if (cancel_requested_.load()) break;   // 对象正在析构，尽快退出，不再碰 cfg_/pending_*

        if (TryOpenPipe()) {
            // TryOpenPipe() 写的是 pipe_（原本给同步 Connect() 用的），但这里是后台
            // 线程调用，绝不能让它被主线程当作"已连接"直接使用——先转移到
            // pending_pipe_，再把 pipe_ 还原，保持"pipe_ 只由主线程碰"这个不变量。
            HANDLE h = pipe_;
            pipe_ = INVALID_HANDLE_VALUE;
            ::EnterCriticalSection(&pending_lock_);
            pending_pipe_ = h;
            ::LeaveCriticalSection(&pending_lock_);
            break;
        }

        if (::GetLastError() == ERROR_PIPE_BUSY) {
            ::WaitNamedPipeW(cfg_.pipe_name.c_str(), 200);
        } else if (!launched) {
            launched = LaunchEngine();
            ::Sleep(100);
        } else {
            ::Sleep(100);
        }

        if (static_cast<LONG>(::GetTickCount() - deadline) >= 0) break;   // 放弃，下次按键再试
    }
    connect_thread_running_.store(false);
}

bool IpcClient::EnsureConnectedAsync() {
    if (connected()) return true;
    if (PickUpPendingConnection()) return true;   // 后台线程刚好连上了，直接采用

    if (!connect_thread_running_.load()) {
        // 没有后台线程在跑（第一次尝试，或者上一次已经跑完/放弃了）：起一个新的，
        // 立即返回——这次按键降级为"引擎还没接住"（调用方原样插入字符），不阻塞。
        if (connect_thread_ != nullptr) {
            ::CloseHandle(connect_thread_);
            connect_thread_ = nullptr;
        }
        connect_thread_running_.store(true);
        connect_thread_ = ::CreateThread(nullptr, 0, &IpcClient::ConnectThreadProc, this, 0, nullptr);
        if (connect_thread_ == nullptr) connect_thread_running_.store(false);   // 开线程失败，下次再试
    }
    return false;   // 已经在后台跑（或刚起的），这次按键不等，不阻塞 UI 线程。
}

void IpcClient::Disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        ::CancelIoEx(pipe_, nullptr);
        ::CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

std::ptrdiff_t IpcClient::BoundedRead(void* buf, std::size_t n, DWORD timeout_ms) {
    ::ResetEvent(read_event_);
    OVERLAPPED ov{};
    ov.hEvent = read_event_;

    DWORD got = 0;
    if (!::ReadFile(pipe_, buf, static_cast<DWORD>(n), &got, &ov)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_IO_PENDING) {
            if (::WaitForSingleObject(read_event_, timeout_ms) == WAIT_TIMEOUT) {
                ::CancelIoEx(pipe_, &ov);
                ::GetOverlappedResult(pipe_, &ov, &got, TRUE);   // 等取消真正完成
                return -1;
            }
            if (!::GetOverlappedResult(pipe_, &ov, &got, FALSE)) {
                const DWORD e2 = ::GetLastError();
                return (e2 == ERROR_BROKEN_PIPE || e2 == ERROR_PIPE_NOT_CONNECTED) ? 0 : -1;
            }
        } else if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
            return 0;
        } else {
            return -1;
        }
    }
    return static_cast<std::ptrdiff_t>(got);
}

std::ptrdiff_t IpcClient::BoundedWrite(const void* buf, std::size_t n, DWORD timeout_ms) {
    ::ResetEvent(write_event_);
    OVERLAPPED ov{};
    ov.hEvent = write_event_;

    DWORD put = 0;
    if (!::WriteFile(pipe_, buf, static_cast<DWORD>(n), &put, &ov)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_IO_PENDING) {
            if (::WaitForSingleObject(write_event_, timeout_ms) == WAIT_TIMEOUT) {
                ::CancelIoEx(pipe_, &ov);
                ::GetOverlappedResult(pipe_, &ov, &put, TRUE);
                return -1;
            }
            if (!::GetOverlappedResult(pipe_, &ov, &put, FALSE)) return -1;
        } else {
            return -1;
        }
    }
    return static_cast<std::ptrdiff_t>(put);
}

bool IpcClient::CallWithTimeout(const ipc::Request& req, ipc::Response& resp,
                                std::uint32_t timeout_ms) {
    if (!connected()) return false;

    const auto writer = [this, timeout_ms](const void* buf, std::size_t n) {
        return BoundedWrite(buf, n, timeout_ms);
    };
    const auto reader = [this, timeout_ms](void* buf, std::size_t n) {
        return BoundedRead(buf, n, timeout_ms);
    };

    if (ipc::WriteFrame(writer, ipc::EncodeRequest(req)) != ipc::FrameStatus::kOk) {
        Disconnect();
        return false;
    }
    std::string body;
    if (ipc::ReadFrame(reader, body) != ipc::FrameStatus::kOk) {
        Disconnect();
        return false;
    }
    return ipc::DecodeResponse(body, resp) == ipc::DecodeStatus::kOk;
}

bool IpcClient::Call(const ipc::Request& req, ipc::Response& resp) {
    return CallWithTimeout(req, resp, cfg_.request_timeout_ms);
}

bool IpcClient::CallMethod(ipc::Method method, const ipc::Json& params, ipc::Response& out) {
    ipc::Request req;
    req.id = next_id_++;
    req.method = method;
    req.params = params;
    return Call(req, out) && out.ok;
}

std::string IpcClient::Hello() {
    ipc::Request req;
    req.id = next_id_++;
    req.method = ipc::Method::kHello;
    req.params = ipc::Json{
        {"clientVersion", "0.1.0-m1"},
        {"pid", static_cast<std::uint32_t>(::GetCurrentProcessId())},
        {"arch", sizeof(void*) == 8 ? "x64" : "x86"},
    };
    ipc::Response resp;
    if (!CallWithTimeout(req, resp, cfg_.connect_timeout_ms) || !resp.ok) return {};
    return resp.result.value("engineVersion", std::string{});
}

bool IpcClient::InitSession(std::uint32_t session_id) {
    ipc::Response out;
    return CallMethod(ipc::Method::kInitSession, ipc::Json{{"sessionId", session_id}}, out);
}

bool IpcClient::ProcessKey(std::uint32_t session_id, int vk, unsigned ch, bool ctrl,
                          ipc::Response& out) {
    return CallMethod(
        ipc::Method::kProcessKey,
        ipc::Json{{"sessionId", session_id}, {"vk", vk}, {"ch", ch}, {"ctrl", ctrl}}, out);
}

bool IpcClient::SelectCandidate(std::uint32_t session_id, int index, ipc::Response& out) {
    return CallMethod(ipc::Method::kSelectCandidate,
                      ipc::Json{{"sessionId", session_id}, {"index", index}}, out);
}

bool IpcClient::PageCandidates(std::uint32_t session_id, int delta, ipc::Response& out) {
    return CallMethod(ipc::Method::kPageCandidates,
                      ipc::Json{{"sessionId", session_id}, {"delta", delta}}, out);
}

bool IpcClient::CommitComposition(std::uint32_t session_id, ipc::Response& out) {
    return CallMethod(ipc::Method::kCommitComposition, ipc::Json{{"sessionId", session_id}}, out);
}

bool IpcClient::CancelComposition(std::uint32_t session_id, ipc::Response& out) {
    return CallMethod(ipc::Method::kCancelComposition, ipc::Json{{"sessionId", session_id}}, out);
}

bool IpcClient::FocusOut(std::uint32_t session_id) {
    ipc::Response out;
    return CallMethod(ipc::Method::kFocusOut, ipc::Json{{"sessionId", session_id}}, out);
}

bool IpcClient::SetCaretRect(std::uint32_t session_id, int x, int y, int w, int h) {
    ipc::Response out;
    return CallMethod(ipc::Method::kSetCaretRect,
                      ipc::Json{{"sessionId", session_id}, {"x", x}, {"y", y}, {"w", w}, {"h", h}},
                      out);
}

}  // namespace myabc::tsf
