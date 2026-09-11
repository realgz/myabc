// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/dispatcher.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.3

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

Dispatcher::Dispatcher(LibPinyinEngine& engine, SessionOptions opts)
    : engine_(engine),
      registry_(BuildDefaultSourceRegistry(engine)),
      sessions_(engine, registry_, std::move(opts)) {}

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
            return Response::Ok(req.id, Json::object());   // M1：无需动作
        case Method::kShutdown:
            should_shutdown_ = true;
            return Response::Ok(req.id, Json::object());
        case Method::kUnknown:
            return Response::Err(req.id, ipc::errc::kUnknownMethod,
                                 "unknown method: " + req.method_raw);
        default:
            return Response::Err(req.id, ipc::errc::kUnknownMethod,
                                 std::string("method not implemented in M1: ") +
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
    return SessionResultToResponse(req.id, s.ProcessKey(vk, ch));
}

Response Dispatcher::HandleSelectCandidate(const Request& req) {
    Session& s = sessions_.GetOrCreate(SessionIdOf(req.params));
    const int index = req.params.value("index", 0);
    return SessionResultToResponse(req.id, s.SelectCandidate(index));
}

Response Dispatcher::HandlePageCandidates(const Request& req) {
    Session& s = sessions_.GetOrCreate(SessionIdOf(req.params));
    const int delta = req.params.value("delta", 0);
    return SessionResultToResponse(req.id, s.PageCandidates(delta));
}

Response Dispatcher::HandleCommitComposition(const Request& req) {
    Session& s = sessions_.GetOrCreate(SessionIdOf(req.params));
    return SessionResultToResponse(req.id, s.CommitComposition());
}

Response Dispatcher::HandleCancelComposition(const Request& req) {
    Session& s = sessions_.GetOrCreate(SessionIdOf(req.params));
    return SessionResultToResponse(req.id, s.CancelComposition());
}

Response Dispatcher::HandleFocusOut(const Request& req) {
    const auto id = SessionIdOf(req.params);
    Session& s = sessions_.GetOrCreate(id);
    s.FocusOut();
    sessions_.Remove(id);
    return Response::Ok(req.id, Json::object());
}

Response Dispatcher::SessionResultToResponse(std::uint32_t id, const SessionResult& r) const {
    Json candidates = Json::array();
    for (const auto& c : r.candidates) {
        candidates.push_back(Json{{"text", c.text}, {"comment", ""}});
    }

    Json result{
        {"handled", r.handled},
        {"preedit", r.preedit},
        {"rawInput", r.raw_input},
        {"candidates", candidates},
        {"page", Json{{"index", r.page_index}, {"size", r.page_size}, {"total", r.page_total}}},
    };
    if (r.has_commit) result["commit"] = r.commit;

    return Response::Ok(id, std::move(result));
}

}  // namespace myabc::engine
