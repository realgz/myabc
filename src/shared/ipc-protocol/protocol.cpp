// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/ipc-protocol/protocol.cpp --- Method <-> 线格式名 映射
// 依据：docs/architecture/system-overview.md §4.1

#include "protocol.hpp"

#include <array>
#include <string_view>
#include <utility>

namespace myabc::ipc {

namespace {
// 单一映射表，MethodName / MethodFromName 都走它，避免两处漂移。
constexpr std::array<std::pair<Method, std::string_view>, 11> kTable{{
    {Method::kHello, "hello"},
    {Method::kInitSession, "initSession"},
    {Method::kProcessKey, "processKey"},
    {Method::kSelectCandidate, "selectCandidate"},
    {Method::kPageCandidates, "pageCandidates"},
    {Method::kCommitComposition, "commitComposition"},
    {Method::kCancelComposition, "cancelComposition"},
    {Method::kFocusIn, "focusIn"},
    {Method::kFocusOut, "focusOut"},
    {Method::kSetConfig, "setConfig"},
    {Method::kShutdown, "shutdown"},
}};
}  // namespace

const char* MethodName(Method m) noexcept {
    for (const auto& [method, name] : kTable) {
        if (method == m) return name.data();
    }
    return "";
}

Method MethodFromName(const std::string& name) noexcept {
    for (const auto& [method, n] : kTable) {
        if (n == name) return method;
    }
    return Method::kUnknown;
}

}  // namespace myabc::ipc
