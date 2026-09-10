// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/ipc-protocol/protocol.hpp --- TIP <-> 引擎 命名管道协议定义
//
// 依据：docs/architecture/system-overview.md §4.1（消息协议草案）+ §7 不变量 6/7
//       docs/plan/01-m0-tsf-skeleton-plan.md §3.1
//
// 不变量 6：跨 MinGW/MSVC 边界只传字节级管道协议，绝不传 C++ 对象 / STL 容器 / 异常。
//           => 本头只定义"线格式"的常量与轻量 POD 视图；两侧各自编译，不共享二进制。
// 不变量 7：协议结构变更必须递增 PROTOCOL_VERSION 并同步 §4.1 与两侧编解码。

#ifndef MYABC_IPC_PROTOCOL_HPP
#define MYABC_IPC_PROTOCOL_HPP

#include <cstdint>
#include <string>

namespace myabc::ipc {

// DECISION: docs/architecture/system-overview.md §4.1 —— 协议版本，改结构必须 +1。
inline constexpr std::uint32_t kProtocolVersion = 1;

// 长度前缀帧：uint32 小端长度 + 该长度的 UTF-8 JSON 字节。
inline constexpr std::uint32_t kMaxFrameBytes = 1u << 20;  // 1 MiB 上限，防御坏帧

// 线格式字段名（集中定义，避免散落裸字符串 —— 不变量 8）。
namespace key {
inline constexpr const char* kVersion = "v";
inline constexpr const char* kId      = "id";
inline constexpr const char* kMethod  = "method";
inline constexpr const char* kParams  = "params";
inline constexpr const char* kOk      = "ok";
inline constexpr const char* kResult  = "result";
inline constexpr const char* kError   = "error";
inline constexpr const char* kErrCode = "code";
inline constexpr const char* kErrMsg  = "msg";
}  // namespace key

// 方法集（system-overview §4.1）。M0 只实现 hello / processKey 的编解码，
// 其余保留枚举位，编解码遇到未知 method 归为 kUnknown。
enum class Method : std::uint8_t {
    kUnknown = 0,
    kHello,
    kInitSession,
    kProcessKey,
    kSelectCandidate,
    kPageCandidates,
    kCommitComposition,
    kCancelComposition,
    kFocusIn,
    kFocusOut,
    kSetConfig,
    kShutdown,
};

const char* MethodName(Method m) noexcept;   // 线格式名；kUnknown -> ""
Method      MethodFromName(const std::string& name) noexcept;

// 错误码（响应 error.code）。负数留给传输/协议层，正数留给引擎业务层。
namespace errc {
inline constexpr int kOk              = 0;
inline constexpr int kBadFrame        = -1;
inline constexpr int kBadJson         = -2;
inline constexpr int kUnknownMethod   = -3;
inline constexpr int kProtocolMismatch = -4;
}  // namespace errc

}  // namespace myabc::ipc

#endif  // MYABC_IPC_PROTOCOL_HPP
