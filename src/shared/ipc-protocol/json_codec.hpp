// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/ipc-protocol/json_codec.hpp --- Request/Response <-> JSON 文本
//
// 依据：docs/architecture/system-overview.md §4.1
//       docs/plan/01-m0-tsf-skeleton-plan.md §3.1（M0 只实现 hello / processKey 编解码）
//
// 不变量 6：这些结构是"各侧本地"的，随源码在 MSVC / MinGW 各自编译，不共享二进制，
//           不跨边界传对象——跨边界的只有 frame.cpp 产出的字节。
//
// nlohmann/json 单头（third_party/json，MIT）在此暴露：params/result 是开放对象，
// 用通用 JSON 值承载最直接；两套工具链都仅需标准库即可编译该头。

#ifndef MYABC_IPC_JSON_CODEC_HPP
#define MYABC_IPC_JSON_CODEC_HPP

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "protocol.hpp"

namespace myabc::ipc {

using Json = nlohmann::json;

struct Request {
    std::uint32_t id = 0;
    Method        method = Method::kUnknown;
    std::string   method_raw;   // 线上原始 method 名（method==kUnknown 时用于诊断）
    Json          params = Json::object();
};

struct Response {
    std::uint32_t id = 0;
    bool          ok = true;
    Json          result = Json::object();
    int           error_code = errc::kOk;
    std::string   error_msg;

    static Response Ok(std::uint32_t id, Json result);
    static Response Err(std::uint32_t id, int code, std::string msg);
};

enum class DecodeStatus { kOk = 0, kBadJson, kProtocolMismatch, kMissingField };

// 编码：始终写出 v = kProtocolVersion。
std::string EncodeRequest(const Request& req);
std::string EncodeResponse(const Response& resp);

// 解码：校验 v 字段；不匹配返回 kProtocolMismatch。method 未知不算错误
// （req.method = kUnknown，method_raw 保留原名），由上层决定回 kUnknownMethod。
DecodeStatus DecodeRequest(const std::string& text, Request& out);
DecodeStatus DecodeResponse(const std::string& text, Response& out);

}  // namespace myabc::ipc

#endif  // MYABC_IPC_JSON_CODEC_HPP
