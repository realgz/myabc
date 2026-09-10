// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/ipc-protocol/frame.hpp --- 长度前缀帧读写（uint32 LE 长度 + UTF-8 body）
//
// 依据：docs/architecture/system-overview.md §4（长度前缀 + JSON）
//       docs/plan/01-m0-tsf-skeleton-plan.md §3.1
//
// 传输被抽象为读/写回调，本库不 include <windows.h> —— 具体的命名管道 HANDLE
// 由各域 backend 适配（tsf-service/backend/ipc_client、input-engine/backend/pipe_server）。
// 这样保证两套工具链都能编、可脱离 Windows 单测。

#ifndef MYABC_IPC_FRAME_HPP
#define MYABC_IPC_FRAME_HPP

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace myabc::ipc {

// 阻塞式读满 / 写满语义由实现保证；返回值：
//   > 0 实际处理字节数（可小于 n，调用方会循环）
//   ==0 对端关闭
//   < 0 IO 错误
using ReadFn  = std::function<std::ptrdiff_t(void* buf, std::size_t n)>;
using WriteFn = std::function<std::ptrdiff_t(const void* buf, std::size_t n)>;

enum class FrameStatus {
    kOk = 0,
    kClosed,     // 对端正常关闭
    kIoError,    // 读写回调报错
    kTooLarge,   // 长度前缀超过 kMaxFrameBytes
};

// 读一帧：先读 4 字节小端长度，再读该长度的 body 到 out_body（覆盖写）。
FrameStatus ReadFrame(const ReadFn& read, std::string& out_body);

// 写一帧：4 字节小端长度 + body。body 超过 kMaxFrameBytes 返回 kTooLarge 且不写出。
FrameStatus WriteFrame(const WriteFn& write, std::string_view body);

}  // namespace myabc::ipc

#endif  // MYABC_IPC_FRAME_HPP
