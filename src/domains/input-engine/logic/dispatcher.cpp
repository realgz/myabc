// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/dispatcher.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.3
//       docs/plan/03-m2-engine-process-ipc-plan.md §3.2

#include "dispatcher.hpp"

namespace myabc::engine {

using ipc::Json;
using ipc::Method;
using ipc::Request;
using ipc::Response;

namespace {
std::uint32_t SessionIdOf(const Json& params) {
    return params.value("sessionId", static_cast<std::uint32_t>(0));
}
}  // namespace

Dispatcher::Dispatcher(LibPinyinEngine& engine, SessionOptions opts, UiBridge* ui_bridge)
    : engine_(engine),
      sessions_(engine, registry_, opts),   // 绑定 registry_ 的引用；内容随后在函数体里填
      ui_bridge_(ui_bridge),
      autosave_counter_(opts.autosave_every_n_commits) {
    // bihuo_table_ 必须先加载好，registry_ 里的 PinyinCandidateSource 才拿到正确数据；
    // registry_ 是默认空构造的，这里赋值真正内容——sessions_ 持有的是 registry_ 这个
    // 对象的引用（不是内容快照），赋值后 sessions_ 看到的就是新内容。
    if (!opts.bihuo_data_path.empty()) bihuo_table_.LoadFromFile(opts.bihuo_data_path);
    registry_ = BuildDefaultSourceRegistry(engine, bihuo_table_, opts.bihuo_enabled,
                                           opts.number_lead_key);
}

Response Dispatcher::Handle(const Request& req) {
    switch (req.method) {
        case Method::kHello:
            return HandleHello(req);
        case Method::kInitSession:
            return HandleInitSession(req);
        case Method::kProcessKey:
            return HandleProcessKey(req);
        case Method::kSelectCandidate:
            return HandleSelectCandidate(req);
        case Method::kPageCandidates:
            return HandlePageCandidates(req);
        case Method::kCommitComposition:
            return HandleCommitComposition(req);
        case Method::kCancelComposition:
            return HandleCancelComposition(req);
        case Method::kFocusOut:
            return HandleFocusOut(req);
        case Method::kFocusIn:
            return Response::Ok(req.id, Json::object());   // 无需动作
        case Method::kSetCaretRect:
            return HandleSetCaretRect(req);
        case Method::kUserDictExport:
            return HandleUserDictExport(req);
        case Method::kUserDictImport:
            return HandleUserDictImport(req);
        case Method::kUserDictClear:
            return HandleUserDictClear(req);
        case Method::kShutdown:
            should_shutdown_ = true;
            return Response::Ok(req.id, Json::object());
        case Method::kUnknown:
            return Response::Err(req.id, ipc::errc::kUnknownMethod,
                                 "unknown method: " + req.method_raw);
        default:
            return Response::Err(req.id, ipc::errc::kUnknownMethod,
                                 std::string("method not implemented: ") +
                                     ipc::MethodName(req.method));
    }
}

Response Dispatcher::HandleHello(const Request& req) {
    return Response::Ok(req.id, Json{
                                    {"engineVersion", kEngineVersion},
                                    {"protocol", ipc::kProtocolVersion},
                                    {"pinyinReady", engine_.ready()},
                                });
}

Response Dispatcher::HandleInitSession(const Request& req) {
    sessions_.GetOrCreate(SessionIdOf(req.params));
    return Response::Ok(req.id, Json::object());
}

Response Dispatcher::HandleProcessKey(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    const int vk = req.params.value("vk", 0);
    const unsigned ch = req.params.value("ch", 0u);
    const bool ctrl = req.params.value("ctrl", false);
    return SessionResultToResponse(req.id, id, s.ProcessKey(vk, ch, ctrl));
}

Response Dispatcher::HandleSelectCandidate(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    const int index = req.params.value("index", 0);
    return SessionResultToResponse(req.id, id, s.SelectCandidate(index));
}

Response Dispatcher::HandlePageCandidates(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    const int delta = req.params.value("delta", 0);
    return SessionResultToResponse(req.id, id, s.PageCandidates(delta));
}

Response Dispatcher::HandleCommitComposition(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    return SessionResultToResponse(req.id, id, s.CommitComposition());
}

Response Dispatcher::HandleCancelComposition(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    return SessionResultToResponse(req.id, id, s.CancelComposition());
}

Response Dispatcher::HandleFocusOut(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    s.FocusOut();
    sessions_.Remove(id);
    pending_ui_.erase(id);
    if (ui_bridge_ != nullptr) ui_bridge_->PushHide(id);
    return Response::Ok(req.id, Json::object());
}

Response Dispatcher::HandleSetCaretRect(const Request& req) {
    const auto id = SessionIdOf(req.params);
    const auto it = pending_ui_.find(id);
    if (it != pending_ui_.end() && ui_bridge_ != nullptr) {
        CaretRect rect;
        rect.x = req.params.value("x", 0);
        rect.y = req.params.value("y", 0);
        rect.w = req.params.value("w", 0);
        rect.h = req.params.value("h", 0);

        // DECISION（真机反馈"候选框偶尔出现在左上角"，见 docs/decisions/_debt-log.md
        // 2026-09-11）：根因是 TIP 侧 ITfContextView::GetTextExt 偶尔在组字刚开始、
        // 宿主布局还没稳定时失败或给出退化矩形，composition.cpp 没检查 HRESULT，
        // 于是把零初始化的 RECT{}（即 x=y=w=h 全部为 0）原样发过来。
        // 2026-09-12 修正（真机反馈"候选词不更新，但空格能正确上屏"——上一版判据
        // `w>0 && h>0` 太严格了：文本插入点（光标）天然是一条竖线，很多宿主对着一个
        // "空选区"（纯插入点，不是选中一段文字）调 GetTextExt 会合法地返回 w=0（没有
        // 宽度，只有高度/位置），这是每次按键后的常态，不是错误——旧判据把这些完全
        // 正常的后续帧全部当"退化"吞掉，候选窗停在第一帧的内容不再刷新，但引擎内部
        // 状态（raw_/candidates_）其实一直在正确推进，所以空格/上屏用的是最新状态，
        // candidates_UI 显示的却是旧的。只有 x/y/w/h 四个全是 0（GetTextExt 真正失败、
        // RECT{} 从没被写过的信号）才算退化——单独 w==0（或 h==0）都可能是合法插入点。
        if (!(rect.x == 0 && rect.y == 0 && rect.w == 0 && rect.h == 0)) {
            std::vector<CandidateItem> items;
            items.reserve(it->second.candidates.size());
            for (const auto& c : it->second.candidates) items.push_back(CandidateItem{c.text, false});

            ui_bridge_->PushShow(id, rect, it->second.preedit, items, it->second.page_index,
                                it->second.page_size, it->second.page_total, it->second.armed_index);
        }
    }
    return Response::Ok(req.id, Json::object());
}

Response Dispatcher::HandleUserDictExport(const Request& req) {
    const std::string path = req.params.value("path", std::string());
    if (path.empty() || !engine_.ExportUserDict(path)) {
        return Response::Err(req.id, ipc::errc::kOperationFailed, "userDictExport failed");
    }
    return Response::Ok(req.id, Json::object());
}

Response Dispatcher::HandleUserDictImport(const Request& req) {
    const std::string path = req.params.value("path", std::string());
    if (path.empty() || !engine_.ImportUserDict(path)) {
        return Response::Err(req.id, ipc::errc::kOperationFailed, "userDictImport failed");
    }
    return Response::Ok(req.id, Json::object());
}

Response Dispatcher::HandleUserDictClear(const Request& req) {
    if (!engine_.ClearUserDict()) {
        return Response::Err(req.id, ipc::errc::kOperationFailed, "userDictClear failed");
    }
    return Response::Ok(req.id, Json::object());
}

void Dispatcher::MaybePushToUi(std::uint32_t session_id, const SessionResult& r) {
    if (!r.composing) {
        pending_ui_.erase(session_id);
        if (ui_bridge_ != nullptr) ui_bridge_->PushHide(session_id);
        return;
    }
    // composing=true：留着等 setCaretRect（TIP 应用完 ITfComposition 后才知道矩形）。
    pending_ui_[session_id] = r;
}

Response Dispatcher::SessionResultToResponse(std::uint32_t msg_id, std::uint32_t session_id,
                                             const SessionResult& r) {
    MaybePushToUi(session_id, r);

    // v2：不再把 candidates/page 塞进 TIP 的响应——那是 uiShow 的活。
    Json result{
        {"handled", r.handled},
        {"preedit", r.preedit},
        {"composing", r.composing},
    };
    if (r.has_commit) {
        result["commit"] = r.commit;
        MaybeAutosave();   // M5：学习结果每 N 次 commit 落盘一次，见头文件 DECISION
    }

    return Response::Ok(msg_id, std::move(result));
}

void Dispatcher::MaybeAutosave() {
    if (autosave_counter_.OnCommit()) engine_.Save();
}

}  // namespace myabc::engine
