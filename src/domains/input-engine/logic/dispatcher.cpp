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
      ui_bridge_(ui_bridge) {
    // bihuo_table_ 必须先加载好，registry_ 里的 PinyinCandidateSource 才拿到正确数据；
    // registry_ 是默认空构造的，这里赋值真正内容——sessions_ 持有的是 registry_ 这个
    // 对象的引用（不是内容快照），赋值后 sessions_ 看到的就是新内容。
    if (!opts.bihuo_data_path.empty()) bihuo_table_.LoadFromFile(opts.bihuo_data_path);
    registry_ = BuildDefaultSourceRegistry(engine, bihuo_table_, opts.bihuo_enabled,
                                           opts.bihuo_lead_key, opts.number_lead_key);
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
    return SessionResultToResponse(req.id, id, s.ProcessKey(vk, ch));
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

        std::vector<CandidateItem> items;
        items.reserve(it->second.candidates.size());
        for (const auto& c : it->second.candidates) items.push_back(CandidateItem{c.text, false});

        ui_bridge_->PushShow(id, rect, it->second.preedit, items, it->second.page_index,
                            it->second.page_size, it->second.page_total);
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
    if (r.has_commit) result["commit"] = r.commit;

    return Response::Ok(msg_id, std::move(result));
}

}  // namespace myabc::engine
