// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/extension_candidate_source.cpp

#include "extension_candidate_source.hpp"

namespace myabc::engine {

namespace {
std::string CacheKeyFor(const InputContext& ctx) { return ctx.field_hint + '\x01' + ctx.raw; }
}  // namespace

bool ExtensionCandidateSource::Handles(const InputContext& ctx) const {
    if (bridge_ == nullptr || ctx.field_hint.empty() || ctx.raw.empty()) return false;
    if (!bridge_->HasProviderFor(ctx.field_hint)) return false;

    // 真查一次（见头文件 DECISION）：只有确实查到候选才接管，否则让路给拼音。
    // 缓存给紧随其后的 Produce() 复用，不重复查询同一个 ctx。
    cached_.clear();
    cached_key_ = CacheKeyFor(ctx);
    bridge_->Query(ctx.field_hint, ctx.raw, query_timeout_ms_, cached_);
    return !cached_.empty();
}

std::vector<CandidateItem> ExtensionCandidateSource::Produce(const InputContext& ctx) {
    // 正常调用顺序下这里应该命中缓存（Resolve() 刚调过 Handles(ctx) 用的就是
    // 同一个 ctx）；万一不是（防御性兜底，比如未来有人绕开 Resolve 直接调
    // Produce），退化成现查一次，不返回错的缓存内容。
    if (cached_key_ != CacheKeyFor(ctx)) {
        std::vector<CandidateItem> out;
        if (bridge_ != nullptr) bridge_->Query(ctx.field_hint, ctx.raw, query_timeout_ms_, out);
        return out;
    }
    return std::move(cached_);
}

}  // namespace myabc::engine
