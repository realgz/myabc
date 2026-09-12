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
// v2（M2，docs/plan/03-m2-engine-process-ipc-plan.md §3.2）：processKey 系方法的响应不
// 再带候选明细/翻页信息（{handled,preedit,composing,commit?} 即可）——候选窗搬到独立的
// myabc-ui.exe，候选明细改由引擎经 uiShow/uiHide 单向推给它，不再经 TIP 转发。
// 新增 setCaretRect（TIP -> 引擎）与 uiShow/uiHide（引擎 -> myabc-ui，同一套编解码复用，
// 只是用在不同的一条命名管道连接上）。
// v3-4：M3/M4 均未改协议结构（见 docs/decisions/_debt-log.md 2026-09-11 各自条目），版本
// 一直停在 2。v5（M5，docs/plan/06-m5-user-dict-learning-plan.md §3.3）：新增管理类方法
// userDictExport/userDictImport/userDictClear（非热路径，deployer CLI 用，见 dispatcher.cpp）。
// v6（用户 2026-09-11 明确要求的智能ABC 风格空格两段式确认，见 session.cpp DECISION）：
// uiShow 新增 armedIndex 字段——候选数>1 时空格先"架住"候选[0]（高亮不上屏），
// 再按一次空格/数字键才真正选中；-1 表示当前没有被架住的候选。
// v7（用户 2026-09-12 要求：不按空格也能用 Ctrl+数字直接选字，见 session.cpp
// DECISION）：processKey 新增 ctrl 字段（bool，默认 false）——Ctrl 按住时数字键
// 无条件当 select_keys 处理，跳过"按空格前数字是笔形码/数字模式"这层判断。
// v8（用户 2026-09-12 要求"开始实现企业能力"，见
// docs/decisions/input-engine/20260912-extension-candidate-provider.md）：
// 新增外部候选源扩展协议——setFieldHint（TIP -> 引擎，告诉引擎当前输入框大概是
// 什么类型字段，供第三方候选提供者判断要不要接管）；registerExtension（第三方
// 提供者 -> 引擎，声明自己能处理哪些 tag，走一条独立的命名管道
// myabc-extension-{sid}，不复用 TIP<->引擎那条）；queryCandidates（引擎 ->
// 第三方提供者，在同一条 extension 管道上反向发起，问它要候选）。
inline constexpr std::uint32_t kProtocolVersion = 8;

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

// 方法集（system-overview §4.1）。编解码遇到未知 method 归为 kUnknown。
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
    // v2（M2）
    kSetCaretRect,   // TIP -> 引擎：本次组字的光标屏幕矩形
    kUiShow,         // 引擎 -> myabc-ui：候选窗内容 + 定位
    kUiHide,         // 引擎 -> myabc-ui：隐藏
    // v5（M5，plan 06 §3.3）：用户词库管理，deployer CLI -> 引擎，非热路径。
    kUserDictExport,   // params: {path}
    kUserDictImport,   // params: {path}
    kUserDictClear,    // params: {}
    // v8（外部候选源扩展协议，见上方版本历史注释）
    kSetFieldHint,       // TIP -> 引擎：params: {sessionId, hint}
    kRegisterExtension,  // 第三方提供者 -> 引擎（myabc-extension-{sid} 管道）：
                        // params: {tags: [string,...]}
    kQueryCandidates,    // 引擎 -> 第三方提供者（同一条管道反向发起）：
                        // params: {tag, raw, sessionId}；
                        // 响应 result: {candidates: [{text, commitText?}, ...]}
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
// 正数留给引擎业务层（不变量见上方注释）。M5：userDictExport/Import/Clear 失败
// （文件读写失败、libpinyin 未就绪等），见 dispatcher.cpp。
inline constexpr int kOperationFailed = 1;
}  // namespace errc

}  // namespace myabc::ipc

#endif  // MYABC_IPC_PROTOCOL_HPP
