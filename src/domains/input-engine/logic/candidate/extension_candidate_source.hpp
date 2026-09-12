// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/extension_candidate_source.hpp
// 依据：docs/decisions/input-engine/20260912-extension-candidate-provider.md
//
// 接管条件：ctx.field_hint 非空，ExtensionBridge 有已注册这个 tag 的 provider，
// 且这次查询**真的返回了非空候选**——三者都满足才 Handles()==true，接管；否则
// 让出优先权，正常拼音候选流程完全不受影响（fail-open，见 extension_bridge.hpp
// 头注释）。
//
// DECISION（实现时发现的真问题，未落地前修复）：如果 Handles() 只检查"有没有
// provider 注册了这个 tag"（不管这次查询有没有结果），会导致一个真实回归——
// SourceRegistry::Resolve() 是"第一个 Handles()==true 的来源独占胜出"，如果
// provider 注册了 tag 但对当前这几个字符还查不到匹配（比如姓名只打了一半），
// Produce() 返回空列表，候选窗就会整个清空，用户连正常拼音候选都看不到了——
// 比"没有这个功能"还糟。修法：把实际查询挪到 Handles() 里做（用 mutable 缓存，
// 避免 Resolve()->Produce() 这组固定调用顺序里重复查一次），Handles() 返回值
// 本身就等于"这次真的有候选可给"，查不到就老老实实返回 false，交回给后面的
// PinyinCandidateSource。
//
// V1 范围收窄（见需求文档已知未决问题）：这样做到的是"有匹配结果时接管，没有
// 时完全让路"，还不是"跟拼音候选混排展示"（那需要 SourceRegistry::Resolve 从
// "单来源胜出"改成"可叠加多来源"，是更大的架构改动，留后续视真实使用反馈决定
// 要不要做）。

#ifndef MYABC_ENGINE_EXTENSION_CANDIDATE_SOURCE_HPP
#define MYABC_ENGINE_EXTENSION_CANDIDATE_SOURCE_HPP

#include <cstdint>
#include <string>

#include "candidate_source.hpp"
#include "extension_bridge.hpp"

namespace myabc::engine {

class ExtensionCandidateSource final : public CandidateSource {
public:
    // bridge 可空（测试/--selftest 不需要扩展协议）——为空时 Handles() 恒 false。
    explicit ExtensionCandidateSource(ExtensionBridge* bridge, std::uint32_t query_timeout_ms = 150)
        : bridge_(bridge), query_timeout_ms_(query_timeout_ms) {}

    bool Handles(const InputContext& ctx) const override;
    std::vector<CandidateItem> Produce(const InputContext& ctx) override;
    // 默认 AutoCommit()=false、UsesEngineChoose()=false（基类默认值，如
    // number_currency_source 一样是独立的原子候选来源，不查 libpinyin）。

private:
    ExtensionBridge* bridge_;
    std::uint32_t query_timeout_ms_;

    // Handles() 里查询的结果缓存，Produce() 直接取用——见上面 DECISION，避免
    // Resolve()->Produce() 这组固定调用顺序里对同一个 ctx 重复查询一次。用
    // raw+field_hint 拼接串当"这份缓存是不是配这次 ctx"的判据，足够简单可靠
    // （不需要真的比较结构体）。
    mutable std::vector<CandidateItem> cached_;
    mutable std::string cached_key_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_EXTENSION_CANDIDATE_SOURCE_HPP
