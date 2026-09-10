// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/dispatcher.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5

#include "dispatcher.hpp"

namespace myabc::engine {

using ipc::Json;
using ipc::Method;
using ipc::Request;
using ipc::Response;

Response Dispatcher::Handle(const Request& req) {
    switch (req.method) {
        case Method::kHello:
            return HandleHello(req);
        case Method::kProcessKey:
            return HandleProcessKey(req);
        case Method::kShutdown:
            should_shutdown_ = true;
            return Response::Ok(req.id, Json::object());
        case Method::kUnknown:
            return Response::Err(req.id, ipc::errc::kUnknownMethod,
                                 "unknown method: " + req.method_raw);
        default:
            // 已定义但 M0 未实现的方法（initSession/selectCandidate/...）。
            return Response::Err(req.id, ipc::errc::kUnknownMethod,
                                 std::string("method not implemented in M0: ") +
                                     ipc::MethodName(req.method));
    }
}

Response Dispatcher::HandleHello(const Request& req) {
    return Response::Ok(req.id, Json{
                                    {"engineVersion", kEngineVersion},
                                    {"protocol", ipc::kProtocolVersion},
                                });
}

Response Dispatcher::HandleProcessKey(const Request& req) {
    // M0 占位：不接入 libpinyin，一律不处理，让 TIP 侧透传原键。
    return Response::Ok(req.id, Json{
                                    {"handled", false},
                                    {"preedit", ""},
                                    {"candidates", Json::array()},
                                });
}

}  // namespace myabc::engine
