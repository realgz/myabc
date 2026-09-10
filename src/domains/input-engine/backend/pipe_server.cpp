// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/pipe_server.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5

#include "pipe_server.hpp"

#include <windows.h>

#include <cstdio>
#include <string>

#include "frame.hpp"
#include "json_codec.hpp"

namespace myabc::engine {

namespace {

constexpr DWORD kPipeBufBytes = 64 * 1024;

// 把 HANDLE 包成 myabc::ipc 的读/写回调（同步、阻塞语义）。
ipc::ReadFn MakeReader(HANDLE pipe) {
    return [pipe](void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD got = 0;
        if (!::ReadFile(pipe, buf, static_cast<DWORD>(n), &got, nullptr)) {
            const DWORD e = ::GetLastError();
            if (e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED) return 0;
            return -1;
        }
        return static_cast<std::ptrdiff_t>(got);
    };
}

ipc::WriteFn MakeWriter(HANDLE pipe) {
    return [pipe](const void* buf, std::size_t n) -> std::ptrdiff_t {
        DWORD put = 0;
        if (!::WriteFile(pipe, buf, static_cast<DWORD>(n), &put, nullptr)) return -1;
        return static_cast<std::ptrdiff_t>(put);
    };
}

// 服务单个已连接客户端，直到断开或 shutdown。
void ServeConnection(HANDLE pipe, Dispatcher& dispatcher) {
    const auto reader = MakeReader(pipe);
    const auto writer = MakeWriter(pipe);

    std::string body;
    for (;;) {
        if (ipc::ReadFrame(reader, body) != ipc::FrameStatus::kOk) break;

        ipc::Request req;
        const ipc::DecodeStatus ds = ipc::DecodeRequest(body, req);
        ipc::Response resp;
        if (ds == ipc::DecodeStatus::kProtocolMismatch) {
            resp = ipc::Response::Err(req.id, ipc::errc::kProtocolMismatch,
                                      "protocol version mismatch");
        } else if (ds != ipc::DecodeStatus::kOk) {
            resp = ipc::Response::Err(req.id, ipc::errc::kBadJson, "malformed request frame");
        } else {
            resp = dispatcher.Handle(req);
        }

        if (ipc::WriteFrame(writer, ipc::EncodeResponse(resp)) != ipc::FrameStatus::kOk) break;
        if (dispatcher.should_shutdown()) break;
    }
}

}  // namespace

int RunPipeServer(const PipeServerOptions& opts, Dispatcher& dispatcher) {
    // M0：同步 accept 循环。ConnectNamedPipe 阻塞至客户端连接；
    // TODO(M2, plan 03)：overlapped connect + idle_exit_minutes 空闲自退出 + 多连接并发。
    (void)opts.idle_exit_minutes;

    while (!dispatcher.should_shutdown()) {
        HANDLE pipe = ::CreateNamedPipeA(
            opts.pipe_name.c_str(), PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES, kPipeBufBytes, kPipeBufBytes, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            std::fprintf(stderr, "CreateNamedPipe 失败: %lu\n", ::GetLastError());
            return 2;
        }

        const BOOL ok = ::ConnectNamedPipe(pipe, nullptr);
        const bool connected = ok || ::GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected) {
            ServeConnection(pipe, dispatcher);
            ::FlushFileBuffers(pipe);
            ::DisconnectNamedPipe(pipe);
        }
        ::CloseHandle(pipe);
    }
    return 0;
}

}  // namespace myabc::engine
