// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/extension_bridge.hpp --- 外部候选源扩展协议（引擎侧）
//
// 依据：docs/decisions/input-engine/20260912-extension-candidate-provider.md
//       docs/architecture/system-overview.md §7 不变量 3（不阻塞按键热路径）
//
// 第三方候选提供者（如闭源的 inputserver，不属于本项目）连接到这条独立命名管道
// （myabc-extension-{sid}，跟 TIP<->引擎、引擎->myabc-ui 两条既有管道都不一样），
// 先发一条 registerExtension 声明自己能处理哪些 tag（如 "phone"），之后引擎在
// 组字时如果当前 field_hint（见 candidate_source.hpp::InputContext）命中某个
// 已注册的 tag，就在同一条连接上反向发起 queryCandidates，问它要候选。
//
// 有界超时、fail-open：任何失败（没有 provider、tag 不匹配、连接断开、超时、
// provider 返回格式不对）都当"没有扩展候选"处理，绝不影响正常拼音候选、绝不
// 阻塞按键热路径——这是跟项目其它外部依赖（引擎连接、UI 推送）一致的既有原则。
//
// 线程模型：接受连接在后台线程里跑（跟 UiBridge 同款）；Query() 从 Dispatcher
// 所在线程调用（pipe_server.cpp 单线程服务模型，见其头注释——不会有第二个线程
// 并发调 Query()）。lock_ 保护 current_pipe_/tags_，Query() 整个过程（含阻塞的
// 有界读写）都持锁，跟接受线程"替换连接"互斥——简化的权衡：接受新连接可能被
// 一次进行中的 Query() 短暂延后（最多 timeout_ms），换来不用处理"连接正在被
// Query() 使用时被另一线程关闭"这种句柄生命周期竞态，见 .cpp 说明。

#ifndef MYABC_ENGINE_EXTENSION_BRIDGE_HPP
#define MYABC_ENGINE_EXTENSION_BRIDGE_HPP

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "libpinyin_wrapper.hpp"   // CandidateItem

namespace myabc::engine {

class ExtensionBridge {
public:
    explicit ExtensionBridge(std::string pipe_name);
    ~ExtensionBridge();

    ExtensionBridge(const ExtensionBridge&) = delete;
    ExtensionBridge& operator=(const ExtensionBridge&) = delete;

    // tag 有没有已注册的 provider 覆盖——ExtensionCandidateSource::Handles() 用它
    // 快速判断"值不值得发起一次 Query"，避免每次按键都去写管道（哪怕当前没连着
    // provider，或连着但没声明这个 tag）。
    bool HasProviderFor(const std::string& tag) const;

    // 有界查询：provider 有响应且给了至少一项候选才返回 true 并填 out；
    // 没有匹配的 provider / 连接断开 / 超时 / 响应格式不对，一律返回 false
    // （调用方按"没有扩展候选"处理，不是错误）。
    bool Query(const std::string& tag, const std::string& raw, std::uint32_t timeout_ms,
              std::vector<CandidateItem>& out);

private:
    static DWORD WINAPI AcceptThreadProc(LPVOID param);
    void RunAcceptLoop();
    // 新连接读第一帧当 registerExtension 处理；成功才发布为 current_pipe_，失败
    // （握手超时/格式不对/不是这个 method）直接关掉这个连接，不影响引擎本身。
    void HandleNewConnection(HANDLE pipe);

    // 有界读/写（overlapped IO + CancelIoEx，超时即取消），跟 TIP 侧 IpcClient 的
    // BoundedRead/BoundedWrite 同一套手法（不能共享代码——MinGW/MSVC 两侧工具链
    // 独立编译，见协议头不变量 6），返回值语义一致：<0=出错/超时，0=连接已断，
    // >0=实际读写字节数。
    std::ptrdiff_t BoundedRead(HANDLE pipe, HANDLE event, void* buf, std::size_t n,
                               DWORD timeout_ms);
    std::ptrdiff_t BoundedWrite(HANDLE pipe, HANDLE event, const void* buf, std::size_t n,
                                DWORD timeout_ms);

    std::string pipe_name_;
    HANDLE accept_thread_ = nullptr;
    std::atomic<bool> stop_{false};

    CRITICAL_SECTION lock_{};
    HANDLE current_pipe_ = INVALID_HANDLE_VALUE;   // 受 lock_ 保护
    std::vector<std::string> tags_;                // 受 lock_ 保护

    // 分开给接受线程（握手）和 Query()（查询）各自专用，避免两个线程共用同一个
    // event 对象导致的 overlapped IO 竞态（见文件头 DECISION）。
    HANDLE handshake_event_ = nullptr;   // 只在接受线程用
    HANDLE query_write_event_ = nullptr;   // 只在调 Query() 的线程用
    HANDLE query_read_event_ = nullptr;    // 同上
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_EXTENSION_BRIDGE_HPP
