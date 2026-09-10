// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/ipc-protocol/json_codec.cpp
// 依据：docs/architecture/system-overview.md §4.1

#include "json_codec.hpp"

namespace myabc::ipc {

Response Response::Ok(std::uint32_t id, Json result) {
    Response r;
    r.id = id;
    r.ok = true;
    r.result = std::move(result);
    return r;
}

Response Response::Err(std::uint32_t id, int code, std::string msg) {
    Response r;
    r.id = id;
    r.ok = false;
    r.error_code = code;
    r.error_msg = std::move(msg);
    return r;
}

std::string EncodeRequest(const Request& req) {
    Json j;
    j[key::kVersion] = kProtocolVersion;
    j[key::kId] = req.id;
    j[key::kMethod] = req.method == Method::kUnknown ? req.method_raw
                                                     : std::string(MethodName(req.method));
    j[key::kParams] = req.params.is_null() ? Json::object() : req.params;
    return j.dump();
}

std::string EncodeResponse(const Response& resp) {
    Json j;
    j[key::kVersion] = kProtocolVersion;
    j[key::kId] = resp.id;
    j[key::kOk] = resp.ok;
    if (resp.ok) {
        j[key::kResult] = resp.result.is_null() ? Json::object() : resp.result;
    } else {
        j[key::kError] = Json{{key::kErrCode, resp.error_code}, {key::kErrMsg, resp.error_msg}};
    }
    return j.dump();
}

namespace {

// 解析 + 版本校验的公共前半段。
DecodeStatus ParseAndCheck(const std::string& text, Json& j) {
    j = Json::parse(text, /*cb=*/nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded() || !j.is_object()) return DecodeStatus::kBadJson;
    const auto v = j.find(key::kVersion);
    if (v == j.end() || !v->is_number_unsigned()) return DecodeStatus::kMissingField;
    if (v->get<std::uint32_t>() != kProtocolVersion) return DecodeStatus::kProtocolMismatch;
    return DecodeStatus::kOk;
}

}  // namespace

DecodeStatus DecodeRequest(const std::string& text, Request& out) {
    Json j;
    if (const auto s = ParseAndCheck(text, j); s != DecodeStatus::kOk) return s;

    const auto id = j.find(key::kId);
    const auto method = j.find(key::kMethod);
    if (id == j.end() || !id->is_number_unsigned()) return DecodeStatus::kMissingField;
    if (method == j.end() || !method->is_string()) return DecodeStatus::kMissingField;

    out = Request{};
    out.id = id->get<std::uint32_t>();
    out.method_raw = method->get<std::string>();
    out.method = MethodFromName(out.method_raw);
    if (const auto p = j.find(key::kParams); p != j.end() && p->is_object()) {
        out.params = *p;
    }
    return DecodeStatus::kOk;
}

DecodeStatus DecodeResponse(const std::string& text, Response& out) {
    Json j;
    if (const auto s = ParseAndCheck(text, j); s != DecodeStatus::kOk) return s;

    const auto id = j.find(key::kId);
    const auto ok = j.find(key::kOk);
    if (id == j.end() || !id->is_number_unsigned()) return DecodeStatus::kMissingField;
    if (ok == j.end() || !ok->is_boolean()) return DecodeStatus::kMissingField;

    out = Response{};
    out.id = id->get<std::uint32_t>();
    out.ok = ok->get<bool>();
    if (out.ok) {
        if (const auto r = j.find(key::kResult); r != j.end() && r->is_object()) out.result = *r;
    } else {
        const auto err = j.find(key::kError);
        if (err == j.end() || !err->is_object()) return DecodeStatus::kMissingField;
        if (const auto c = err->find(key::kErrCode); c != err->end() && c->is_number_integer()) {
            out.error_code = c->get<int>();
        }
        if (const auto m = err->find(key::kErrMsg); m != err->end() && m->is_string()) {
            out.error_msg = m->get<std::string>();
        }
    }
    return DecodeStatus::kOk;
}

}  // namespace myabc::ipc
