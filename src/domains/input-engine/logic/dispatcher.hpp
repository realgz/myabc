// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/dispatcher.hpp --- 请求 -> 响应 的纯逻辑
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.3
//       docs/plan/03-m2-engine-process-ipc-plan.md §3.2（协议 v2，setCaretRect/uiShow/uiHide）
//       docs/architecture/system-overview.md §4.1
//
// 纯逻辑层：不碰命名管道细节（UiBridge 除外——它是"推给另一条连接"，Dispatcher 只负责
// 决定何时推、推什么，实际写管道在 UiBridge 内部）。
//
// v2：processKey 系方法的响应只剩 {handled,preedit,composing,commit?}——候选明细不再
// 经这条连接回 TIP，而是 MaybePushToUi() 存一份"待推"结果，等 setCaretRect 到达后
// （TIP 应用完 ITfComposition、算出光标矩形之后才会调）配上矩形一起经 UiBridge 推给
// myabc-ui；composing=false 时立刻推 uiHide，不必等 setCaretRect。

#ifndef MYABC_ENGINE_DISPATCHER_HPP
#define MYABC_ENGINE_DISPATCHER_HPP

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "bihuoma_table.hpp"
#include "candidate/source_registry.hpp"
#include "extension_bridge.hpp"
#include "json_codec.hpp"
#include "learning_policy.hpp"
#include "libpinyin_wrapper.hpp"
#include "protocol.hpp"
#include "scheme_state_store.hpp"
#include "session/session_manager.hpp"
#include "ui_bridge.hpp"
#include "wubi_table.hpp"

namespace myabc::engine {

class Dispatcher {
public:
    // engine 即使 Init() 失败（!engine.ready()）也必须是个有效对象——LibPinyinEngine 的
    // 各方法在未就绪时全部安全地空操作/返回空结果，因此 processKey 等会自然退化为
    // handled:false，不崩溃，等价 M0 占位行为。
    // ui_bridge 可空（测试/--selftest 不需要候选窗）。extension_bridge 同样可空
    // （测试/--selftest 不需要外部候选源，见 extension_bridge.hpp）。
    // scheme_state_path：setConfig 切换输入方案成功后落盘的小状态文件路径（见
    // scheme_state_store.hpp）。空 = 不持久化（测试/--selftest 默认，同其它可选路径
    // 参数的既有惯例）。
    Dispatcher(LibPinyinEngine& engine, SessionOptions opts, UiBridge* ui_bridge = nullptr,
              ExtensionBridge* extension_bridge = nullptr, std::string scheme_state_path = {});

    ipc::Response Handle(const ipc::Request& req);

    bool should_shutdown() const noexcept { return should_shutdown_; }

    // 引擎退出前（空闲自退出 / shutdown 方法 / 主循环自然结束）调用一次，尽力落盘。
    void SaveBeforeExit() { engine_.Save(); }

    static constexpr const char* kEngineVersion = "0.2.0-m2";

private:
    ipc::Response HandleHello(const ipc::Request& req);
    ipc::Response HandleInitSession(const ipc::Request& req);
    ipc::Response HandleProcessKey(const ipc::Request& req);
    ipc::Response HandleSelectCandidate(const ipc::Request& req);
    ipc::Response HandlePageCandidates(const ipc::Request& req);
    ipc::Response HandleCommitComposition(const ipc::Request& req);
    ipc::Response HandleCancelComposition(const ipc::Request& req);
    ipc::Response HandleFocusOut(const ipc::Request& req);
    ipc::Response HandleSetCaretRect(const ipc::Request& req);
    ipc::Response HandleSetFieldHint(const ipc::Request& req);
    ipc::Response HandleSetConfig(const ipc::Request& req);
    ipc::Response HandleUserDictExport(const ipc::Request& req);
    ipc::Response HandleUserDictImport(const ipc::Request& req);
    ipc::Response HandleUserDictClear(const ipc::Request& req);

    ipc::Response SessionResultToResponse(std::uint32_t msg_id, std::uint32_t session_id,
                                          const SessionResult& r);
    void MaybePushToUi(std::uint32_t session_id, const SessionResult& r);
    void MaybeAutosave();   // M5：每 N 次 commit 主动 pinyin_save，见 .cpp DECISION

    LibPinyinEngine& engine_;
    BihuoTable bihuo_table_;   // registry_ 持有它的引用；先于 registry_ 内容确定而声明
    WubiTable wubi_table_;    // 同上，五笔编码表，见 wubi_table.hpp
    SourceRegistry registry_;  // 默认空构造，真正内容在构造函数体里赋值（见 .cpp 说明）
    SessionManager sessions_;
    UiBridge* ui_bridge_;
    bool should_shutdown_ = false;

    // M5（plan 06 §3.2）：每 N 次成功 commit 主动 pinyin_save 一次，防 taskkill/崩溃丢失
    // 学习结果（idle-exit/shutdown 的落盘之外的额外保险）。见 learning_policy.hpp。
    AutosaveCounter autosave_counter_;

    // sessionId -> 最近一次算好、composing=true 的结果，等 setCaretRect 来了再推 UI。
    std::unordered_map<std::uint32_t, SessionResult> pending_ui_;

    // 2026-09-13：见构造函数参数注释与 scheme_state_store.hpp。
    std::string scheme_state_path_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_DISPATCHER_HPP
