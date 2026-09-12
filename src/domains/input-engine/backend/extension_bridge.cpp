// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/extension_bridge.cpp
// 依据：docs/decisions/input-engine/20260912-extension-candidate-provider.md

#include "extension_bridge.hpp"

#include <algorithm>
#include <atomic>

#include "frame.hpp"
#include "json_codec.hpp"
#include "protocol.hpp"

namespace myabc::engine {

namespace {
constexpr DWORD kPipeBufBytes = 64 * 1024;
constexpr DWORD kHandshakeTimeoutMs = 5000;   // 握手是一次性的，给足时间不必省
std::atomic<std::uint32_t> g_next_query_id{1};
}  // namespace

ExtensionBridge::ExtensionBridge(std::string pipe_name) : pipe_name_(std::move(pipe_name)) {
    ::InitializeCriticalSection(&lock_);
    handshake_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    query_write_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    query_read_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
    accept_thread_ = ::CreateThread(nullptr, 0, &ExtensionBridge::AcceptThreadProc, this, 0, nullptr);
}

ExtensionBridge::~ExtensionBridge() {
    stop_.store(true);
    // 唤醒可能卡在 ConnectNamedPipe 里的接受线程：连一下自己的管道触发它返回
    // （跟 UiBridge 同款手法）。
    HANDLE h = ::CreateFileA(pipe_name_.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) ::CloseHandle(h);

    if (accept_thread_ != nullptr) {
        ::WaitForSingleObject(accept_thread_, 2000);
        ::CloseHandle(accept_thread_);
    }
    ::EnterCriticalSection(&lock_);
    if (current_pipe_ != INVALID_HANDLE_VALUE) ::CloseHandle(current_pipe_);
    ::LeaveCriticalSection(&lock_);
    ::DeleteCriticalSection(&lock_);

    if (handshake_event_) ::CloseHandle(handshake_event_);
    if (query_write_event_) ::CloseHandle(query_write_event_);
    if (query_read_event_) ::CloseHandle(query_read_event_);
}

std::ptrdiff_t ExtensionBridge::BoundedRead(HANDLE pipe, HANDLE event, void* buf, std::size_t n,
                                            DWORD timeout_ms) {
    ::ResetEvent(event);
    OVERLAPPED ov{};
    ov.hEvent = event;

    DWORD got = 0;
    if (!::ReadFile(pipe, buf, static_cast<DWORD>(n), &got, &ov)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_IO_PENDING) {
            if (::WaitForSingleObject(event, timeout_ms) == WAIT_TIMEOUT) {
                ::CancelIoEx(pipe, &ov);
                ::GetOverlappedResult(pipe, &ov, &got, TRUE);
                return -1;
            }
            if (!::GetOverlappedResult(pipe, &ov, &got, FALSE)) {
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

std::ptrdiff_t ExtensionBridge::BoundedWrite(HANDLE pipe, HANDLE event, const void* buf,
                                             std::size_t n, DWORD timeout_ms) {
    ::ResetEvent(event);
    OVERLAPPED ov{};
    ov.hEvent = event;

    DWORD put = 0;
    if (!::WriteFile(pipe, buf, static_cast<DWORD>(n), &put, &ov)) {
        const DWORD err = ::GetLastError();
        if (err == ERROR_IO_PENDING) {
            if (::WaitForSingleObject(event, timeout_ms) == WAIT_TIMEOUT) {
                ::CancelIoEx(pipe, &ov);
                ::GetOverlappedResult(pipe, &ov, &put, TRUE);
                return -1;
            }
            if (!::GetOverlappedResult(pipe, &ov, &put, FALSE)) return -1;
        } else {
            return -1;
        }
    }
    return static_cast<std::ptrdiff_t>(put);
}

DWORD WINAPI ExtensionBridge::AcceptThreadProc(LPVOID param) {
    static_cast<ExtensionBridge*>(param)->RunAcceptLoop();
    return 0;
}

void ExtensionBridge::RunAcceptLoop() {
    while (!stop_.load()) {
        HANDLE pipe = ::CreateNamedPipeA(
            pipe_name_.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES, kPipeBufBytes, kPipeBufBytes, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) return;

        ::ResetEvent(handshake_event_);
        OVERLAPPED ov{};
        ov.hEvent = handshake_event_;
        const BOOL ok = ::ConnectNamedPipe(pipe, &ov);
        if (!ok) {
            const DWORD err = ::GetLastError();
            if (err == ERROR_IO_PENDING) {
                ::WaitForSingleObject(handshake_event_, INFINITE);   // 等真连上，无界但可被
                                                                     // 析构的自连接手法唤醒
            } else if (err != ERROR_PIPE_CONNECTED) {
                ::CloseHandle(pipe);
                continue;
            }
        }
        if (stop_.load()) {
            ::CloseHandle(pipe);
            return;
        }

        HandleNewConnection(pipe);   // 成功则发布为 current_pipe_，失败自己关掉 pipe
    }
}

void ExtensionBridge::HandleNewConnection(HANDLE pipe) {
    const ipc::ReadFn reader = [this, pipe](void* buf, std::size_t n) -> std::ptrdiff_t {
        return BoundedRead(pipe, handshake_event_, buf, n, kHandshakeTimeoutMs);
    };
    std::string body;
    if (ipc::ReadFrame(reader, body) != ipc::FrameStatus::kOk) {
        ::CloseHandle(pipe);
        return;
    }

    ipc::Request req;
    if (ipc::DecodeRequest(body, req) != ipc::DecodeStatus::kOk ||
        req.method != ipc::Method::kRegisterExtension) {
        ::CloseHandle(pipe);   // 握手格式不对：不是我们认识的协议，静默拒绝
        return;
    }

    std::vector<std::string> tags;
    if (const auto it = req.params.find("tags"); it != req.params.end() && it->is_array()) {
        for (const auto& t : *it) {
            if (t.is_string()) tags.push_back(t.get<std::string>());
        }
    }

    const ipc::Response ack = ipc::Response::Ok(req.id, ipc::Json::object());
    const ipc::WriteFn writer = [this, pipe](const void* buf, std::size_t n) -> std::ptrdiff_t {
        return BoundedWrite(pipe, handshake_event_, buf, n, kHandshakeTimeoutMs);
    };
    if (ipc::WriteFrame(writer, ipc::EncodeResponse(ack)) != ipc::FrameStatus::kOk) {
        ::CloseHandle(pipe);
        return;
    }

    // 握手成功：发布为当前连接。跟 Query() 共用 lock_——见头文件 DECISION，简化的
    // 权衡是"新连接可能被一次进行中的 Query() 短暂延后"，换取不用处理句柄生命周期
    // 竞态。
    ::EnterCriticalSection(&lock_);
    if (current_pipe_ != INVALID_HANDLE_VALUE) ::CloseHandle(current_pipe_);
    current_pipe_ = pipe;
    tags_ = std::move(tags);
    ::LeaveCriticalSection(&lock_);
}

bool ExtensionBridge::HasProviderFor(const std::string& tag) const {
    if (tag.empty()) return false;
    ::EnterCriticalSection(&const_cast<ExtensionBridge*>(this)->lock_);
    const bool found = current_pipe_ != INVALID_HANDLE_VALUE &&
                       std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
    ::LeaveCriticalSection(&const_cast<ExtensionBridge*>(this)->lock_);
    return found;
}

bool ExtensionBridge::Query(const std::string& tag, const std::string& raw,
                           std::uint32_t timeout_ms, std::vector<CandidateItem>& out) {
    if (tag.empty()) return false;

    ::EnterCriticalSection(&lock_);
    if (current_pipe_ == INVALID_HANDLE_VALUE ||
        std::find(tags_.begin(), tags_.end(), tag) == tags_.end()) {
        ::LeaveCriticalSection(&lock_);
        return false;
    }
    const HANDLE pipe = current_pipe_;

    ipc::Request req;
    req.id = g_next_query_id.fetch_add(1);
    req.method = ipc::Method::kQueryCandidates;
    req.params = ipc::Json{{"tag", tag}, {"raw", raw}};

    const ipc::WriteFn writer = [this, pipe, timeout_ms](const void* buf,
                                                         std::size_t n) -> std::ptrdiff_t {
        return BoundedWrite(pipe, query_write_event_, buf, n, timeout_ms);
    };
    bool ok = ipc::WriteFrame(writer, ipc::EncodeRequest(req)) == ipc::FrameStatus::kOk;

    std::string body;
    if (ok) {
        const ipc::ReadFn reader = [this, pipe, timeout_ms](void* buf,
                                                            std::size_t n) -> std::ptrdiff_t {
            return BoundedRead(pipe, query_read_event_, buf, n, timeout_ms);
        };
        ok = ipc::ReadFrame(reader, body) == ipc::FrameStatus::kOk;
    }

    if (!ok) {
        // 写/读失败：这条连接坏了（超时/断线），清掉——下次握手会重新建立。
        ::CloseHandle(current_pipe_);
        current_pipe_ = INVALID_HANDLE_VALUE;
        tags_.clear();
        ::LeaveCriticalSection(&lock_);
        return false;
    }
    ::LeaveCriticalSection(&lock_);

    ipc::Response resp;
    if (ipc::DecodeResponse(body, resp) != ipc::DecodeStatus::kOk || !resp.ok) return false;

    const auto it = resp.result.find("candidates");
    if (it == resp.result.end() || !it->is_array()) return false;

    for (const auto& c : *it) {
        if (!c.is_object()) continue;
        const auto text_it = c.find("text");
        if (text_it == c.end() || !text_it->is_string()) continue;
        CandidateItem item;
        item.text = text_it->get<std::string>();
        if (const auto ct = c.find("commitText"); ct != c.end() && ct->is_string()) {
            item.commit_text = ct->get<std::string>();
        }
        out.push_back(std::move(item));
    }
    return !out.empty();
}

}  // namespace myabc::engine
