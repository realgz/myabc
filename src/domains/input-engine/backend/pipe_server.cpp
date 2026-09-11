// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/pipe_server.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/plan/03-m2-engine-process-ipc-plan.md §3.4（idle_exit_minutes 空闲自退出）

#include "pipe_server.hpp"

#include <windows.h>

#include <cstdio>
#include <string>

#include "frame.hpp"
#include "json_codec.hpp"

namespace myabc::engine {

namespace {

constexpr DWORD kPipeBufBytes = 64 * 1024;

// 把 HANDLE 包成 myabc::ipc 的读/写回调（同步、阻塞语义——一旦已连接，逐帧收发不设超时；
// 有界等待是 TIP 侧 ipc_client 的职责，见 docs/plan/02-...md §3.5，引擎侧信任已连接的
// 对端会及时收发）。
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
    // overlapped connect：等客户端连接时用 WaitForSingleObject 加超时，超时且已到
    // idle_exit_minutes 就退出；一旦真的连上，交回 ServeConnection 做同步收发
    // （不变量 3 是 TIP 侧的责任，引擎侧只需不无限期占着不退出）。
    const DWORD idle_ms = opts.idle_exit_seconds_override > 0
                             ? opts.idle_exit_seconds_override * 1000u
                             : (opts.idle_exit_minutes == 0 ? INFINITE
                                                            : opts.idle_exit_minutes * 60u * 1000u);

    HANDLE connect_event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

    while (!dispatcher.should_shutdown()) {
        HANDLE pipe = ::CreateNamedPipeA(
            opts.pipe_name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS,
            PIPE_UNLIMITED_INSTANCES, kPipeBufBytes, kPipeBufBytes, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            std::fprintf(stderr, "CreateNamedPipe 失败: %lu\n", ::GetLastError());
            return 2;
        }

        ::ResetEvent(connect_event);
        OVERLAPPED ov{};
        ov.hEvent = connect_event;

        bool connected = false;
        if (::ConnectNamedPipe(pipe, &ov)) {
            connected = true;   // 罕见：同步立刻完成
        } else {
            const DWORD err = ::GetLastError();
            if (err == ERROR_PIPE_CONNECTED) {
                connected = true;
            } else if (err == ERROR_IO_PENDING) {
                const DWORD w = ::WaitForSingleObject(connect_event, idle_ms);
                if (w == WAIT_OBJECT_0) {
                    DWORD dummy = 0;
                    connected = ::GetOverlappedResult(pipe, &ov, &dummy, FALSE) != FALSE;
                } else {
                    // 空闲超时：没有新连接，收尾退出（plan 03 §3.4：pinyin_save 尽力）。
                    ::CancelIoEx(pipe, &ov);
                    ::CloseHandle(pipe);
                    if (opts.idle_exit_seconds_override > 0) {
                        std::fprintf(stderr, "空闲 %u 秒无连接，引擎自退出。\n",
                                    opts.idle_exit_seconds_override);
                    } else {
                        std::fprintf(stderr, "空闲 %u 分钟无连接，引擎自退出。\n",
                                    opts.idle_exit_minutes);
                    }
                    dispatcher.SaveBeforeExit();
                    return 0;
                }
            }
        }

        if (connected) {
            ServeConnection(pipe, dispatcher);
            ::FlushFileBuffers(pipe);
            ::DisconnectNamedPipe(pipe);
        }
        ::CloseHandle(pipe);
    }

    dispatcher.SaveBeforeExit();
    return 0;
}

}  // namespace myabc::engine
