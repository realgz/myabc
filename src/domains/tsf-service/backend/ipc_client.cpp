// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/ipc_client.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3

#include "ipc_client.hpp"

#include <string>

#include "frame.hpp"
#include "protocol.hpp"

namespace myabc::tsf {

namespace {

std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0,
                                        nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr,
                          nullptr);
    return s;
}

}  // namespace

IpcClient::IpcClient(IpcClientConfig cfg) : cfg_(std::move(cfg)) {}

IpcClient::~IpcClient() { Disconnect(); }

bool IpcClient::TryOpenPipe() {
    const HANDLE h = ::CreateFileW(cfg_.pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                   OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = PIPE_READMODE_BYTE;
    ::SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
    pipe_ = h;
    return true;
}

bool IpcClient::LaunchEngine() {
    if (cfg_.engine_exe_path.empty()) return false;

    std::wstring cmd = L"\"" + cfg_.engine_exe_path + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    const BOOL ok = ::CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                                     CREATE_NO_WINDOW | DETACHED_PROCESS, nullptr, nullptr, &si, &pi);
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

void IpcClient::Disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool IpcClient::Call(const ipc::Request& req, ipc::Response& resp) {
    if (!connected()) return false;

    const auto writer = [this](const void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD put = 0;
        if (!::WriteFile(pipe_, buf, static_cast<DWORD>(n), &put, nullptr)) return -1;
        return static_cast<std::ptrdiff_t>(put);
    };
    const auto reader = [this](void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD got = 0;
        if (!::ReadFile(pipe_, buf, static_cast<DWORD>(n), &got, nullptr)) {
            const DWORD e = ::GetLastError();
            return (e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED) ? 0 : -1;
        }
        return static_cast<std::ptrdiff_t>(got);
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

std::string IpcClient::Hello() {
    ipc::Request req;
    req.id = next_id_++;
    req.method = ipc::Method::kHello;
    req.params = ipc::Json{
        {"clientVersion", "0.0.0-m0"},
        {"pid", static_cast<std::uint32_t>(::GetCurrentProcessId())},
        {"arch", sizeof(void*) == 8 ? "x64" : "x86"},
    };
    ipc::Response resp;
    if (!Call(req, resp) || !resp.ok) return {};
    return resp.result.value("engineVersion", std::string{});
}

}  // namespace myabc::tsf
