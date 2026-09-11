// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/ui_bridge.cpp
// 依据：docs/plan/03-m2-engine-process-ipc-plan.md §3.1/§3.2

#include "ui_bridge.hpp"

#include <cstdio>

#include "frame.hpp"
#include "json_codec.hpp"
#include "protocol.hpp"

namespace myabc::engine {

namespace {
constexpr DWORD kPipeBufBytes = 64 * 1024;
}  // namespace

UiBridge::UiBridge(std::string pipe_name, std::string ui_exe_path)
    : pipe_name_(std::move(pipe_name)), ui_exe_path_(std::move(ui_exe_path)) {
    ::InitializeCriticalSection(&lock_);
    accept_thread_ = ::CreateThread(nullptr, 0, &UiBridge::AcceptThreadProc, this, 0, nullptr);
}

UiBridge::~UiBridge() {
    stop_.store(true);
    // 唤醒可能卡在 ConnectNamedPipe 里的接受线程：连一下自己的管道触发它返回。
    HANDLE h = ::CreateFileA(pipe_name_.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) ::CloseHandle(h);

    if (accept_thread_ != nullptr) {
        ::WaitForSingleObject(accept_thread_, 1000);
        ::CloseHandle(accept_thread_);
    }
    ::EnterCriticalSection(&lock_);
    if (current_pipe_ != INVALID_HANDLE_VALUE) ::CloseHandle(current_pipe_);
    ::LeaveCriticalSection(&lock_);
    ::DeleteCriticalSection(&lock_);
}

DWORD WINAPI UiBridge::AcceptThreadProc(LPVOID param) {
    static_cast<UiBridge*>(param)->RunAcceptLoop();
    return 0;
}

void UiBridge::RunAcceptLoop() {
    while (!stop_.load()) {
        HANDLE pipe = ::CreateNamedPipeA(
            pipe_name_.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES, kPipeBufBytes, kPipeBufBytes, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) return;

        const BOOL ok = ::ConnectNamedPipe(pipe, nullptr);
        if (stop_.load()) {
            ::CloseHandle(pipe);
            return;
        }
        if (!ok && ::GetLastError() != ERROR_PIPE_CONNECTED) {
            ::CloseHandle(pipe);
            continue;
        }

        ::EnterCriticalSection(&lock_);
        if (current_pipe_ != INVALID_HANDLE_VALUE) ::CloseHandle(current_pipe_);
        current_pipe_ = pipe;
        ::LeaveCriticalSection(&lock_);
    }
}

void UiBridge::EnsureUiSpawned() {
    if (ui_exe_path_.empty()) return;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // myabc-ui.exe 自己的命名互斥量保证单实例；这里重复拉起无害（多余实例会自退出）。
    std::string cmd = "\"" + ui_exe_path_ + "\"";
    if (::CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi)) {
        ::CloseHandle(pi.hThread);
        ::CloseHandle(pi.hProcess);
    }
}

bool UiBridge::WriteJson(const std::string& utf8_json) {
    HANDLE pipe;
    ::EnterCriticalSection(&lock_);
    pipe = current_pipe_;
    ::LeaveCriticalSection(&lock_);

    if (pipe == INVALID_HANDLE_VALUE) {
        EnsureUiSpawned();
        return false;   // 这次推送放弃；等 myabc-ui 连上来后下次推送会成功
    }

    const ipc::WriteFn writer = [pipe](const void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD put = 0;
        if (!::WriteFile(pipe, buf, static_cast<DWORD>(n), &put, nullptr)) return -1;
        return static_cast<std::ptrdiff_t>(put);
    };

    if (ipc::WriteFrame(writer, utf8_json) == ipc::FrameStatus::kOk) return true;

    // 写失败：这条连接坏了，清掉；接受线程会在下次 myabc-ui 重连时更新 current_pipe_。
    ::EnterCriticalSection(&lock_);
    if (current_pipe_ == pipe) {
        ::CloseHandle(current_pipe_);
        current_pipe_ = INVALID_HANDLE_VALUE;
    }
    ::LeaveCriticalSection(&lock_);
    EnsureUiSpawned();
    return false;
}

void UiBridge::PushShow(std::uint32_t session_id, const CaretRect& rect, const std::string& preedit,
                        const std::vector<CandidateItem>& candidates, int page_index, int page_size,
                        int page_total) {
    ipc::Json items = ipc::Json::array();
    for (const auto& c : candidates) items.push_back(ipc::Json{{"text", c.text}});

    ipc::Request req;
    req.id = 0;   // 单向推送，不需要匹配响应
    req.method = ipc::Method::kUiShow;
    req.params = ipc::Json{
        {"sessionId", session_id},
        {"caretRect", ipc::Json{{"x", rect.x}, {"y", rect.y}, {"w", rect.w}, {"h", rect.h}}},
        {"preedit", preedit},
        {"candidates", items},
        {"page", ipc::Json{{"index", page_index}, {"size", page_size}, {"total", page_total}}},
    };
    WriteJson(ipc::EncodeRequest(req));
}

void UiBridge::PushHide(std::uint32_t session_id) {
    ipc::Request req;
    req.id = 0;
    req.method = ipc::Method::kUiHide;
    req.params = ipc::Json{{"sessionId", session_id}};
    WriteJson(ipc::EncodeRequest(req));
}

}  // namespace myabc::engine
