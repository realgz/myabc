// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/backend/engine_client.cpp
// 依据：docs/plan/06-m5-user-dict-learning-plan.md §3.4

#include "engine_client.hpp"

#include "frame.hpp"
#include "protocol.hpp"

namespace myabc::deploy {

bool EngineClient::TryOpenPipe() {
    // 非 overlapped：一次性 CLI 调用，阻塞 ReadFile/WriteFile 可接受（见头文件 DECISION，
    // 跟 tsf-service/backend/ipc_client 的"绝不无界等待"约束不是同一个场景）。
    const HANDLE h = ::CreateFileW(cfg_.pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                   nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD mode = PIPE_READMODE_BYTE;
    ::SetNamedPipeHandleState(h, &mode, nullptr, nullptr);
    pipe_ = h;
    return true;
}

bool EngineClient::LaunchEngine() {
    if (cfg_.engine_exe_path.empty()) return false;
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

bool EngineClient::Connect() {
    if (pipe_ != INVALID_HANDLE_VALUE) return true;

    const DWORD deadline = ::GetTickCount() + cfg_.connect_timeout_ms;
    bool launched = false;
    for (;;) {
        if (TryOpenPipe()) return true;

        if (::GetLastError() == ERROR_PIPE_BUSY) {
            ::WaitNamedPipeW(cfg_.pipe_name.c_str(), 200);
        } else if (!launched) {
            launched = LaunchEngine();   // 没有正在跑的引擎实例，先拉起一个
            ::Sleep(200);
        } else {
            ::Sleep(100);
        }

        if (static_cast<LONG>(::GetTickCount() - deadline) >= 0) return false;
    }
}

void EngineClient::Disconnect() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
        ::CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
}

bool EngineClient::CallMethod(ipc::Method method, const ipc::Json& params, ipc::Response& out,
                              std::string& out_error) {
    if (!Connect()) {
        out_error = "连不上引擎（也拉不起来），检查 myabc-engine.exe 是否存在";
        return false;
    }

    ipc::Request req;
    req.id = next_id_++;
    req.method = method;
    req.params = params;

    const auto writer = [this](const void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD put = 0;
        if (!::WriteFile(pipe_, buf, static_cast<DWORD>(n), &put, nullptr)) return -1;
        return static_cast<std::ptrdiff_t>(put);
    };
    const auto reader = [this](void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD got = 0;
        if (!::ReadFile(pipe_, buf, static_cast<DWORD>(n), &got, nullptr)) {
            const DWORD err = ::GetLastError();
            return (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) ? 0 : -1;
        }
        return static_cast<std::ptrdiff_t>(got);
    };

    if (ipc::WriteFrame(writer, ipc::EncodeRequest(req)) != ipc::FrameStatus::kOk) {
        Disconnect();
        out_error = "写请求失败（管道已断开）";
        return false;
    }
    std::string body;
    if (ipc::ReadFrame(reader, body) != ipc::FrameStatus::kOk) {
        Disconnect();
        out_error = "读响应失败（管道已断开或超时）";
        return false;
    }
    if (ipc::DecodeResponse(body, out) != ipc::DecodeStatus::kOk) {
        out_error = "响应解码失败（协议不匹配？）";
        return false;
    }
    if (!out.ok) {
        out_error = out.error_msg.empty() ? "引擎返回错误" : out.error_msg;
        return false;
    }
    return true;
}

bool EngineClient::UserDictExport(const std::string& path, std::string& out_error) {
    ipc::Response out;
    return CallMethod(ipc::Method::kUserDictExport, ipc::Json{{"path", path}}, out, out_error);
}

bool EngineClient::UserDictImport(const std::string& path, std::string& out_error) {
    ipc::Response out;
    return CallMethod(ipc::Method::kUserDictImport, ipc::Json{{"path", path}}, out, out_error);
}

bool EngineClient::UserDictClear(std::string& out_error) {
    ipc::Response out;
    return CallMethod(ipc::Method::kUserDictClear, ipc::Json::object(), out, out_error);
}

}  // namespace myabc::deploy
