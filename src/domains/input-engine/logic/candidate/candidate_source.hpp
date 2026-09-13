// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/candidate/candidate_source.hpp --- 候选来源适配器接口
//
// 依据：docs/architecture/system-overview.md §6（CandidateSource 适配器点位）
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2
//
// DECISION: 3 个以上实现（pinyin / english_passthrough / punctuation，M4 再加 number）
// 触发 code-quality-standards.md §3.2 的适配器强制线，禁止用 if/elif 分发——
// 新增来源 = 新增一个类 + 在 source_registry 里登记，不改已有分发逻辑。

#ifndef MYABC_ENGINE_CANDIDATE_SOURCE_HPP
#define MYABC_ENGINE_CANDIDATE_SOURCE_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "libpinyin_wrapper.hpp"

namespace myabc::engine {

enum class InputMode { kChinese, kEnglish };

// 2026-09-13：输入方案（跟 InputMode 是正交维度，见
// docs/decisions/input-engine/20260913-wubi-input-scheme.md）。
// - kSmartAbc：原有默认行为，libpinyin 候选 + 空格两段式确认 + 笔形辅助码/i 数字金额，
//   一行不改（docs/decisions/input-engine/20260911-space-key-two-step-confirm.md）。
// - kPlainPinyin：跟 kSmartAbc 用同一个 libpinyin 引擎/候选来源，但按键路由不同——
//   数字键任何时候直接选字、空格任何时候立即选中候选[0]，不架住。
// - kWubi：候选来源换成 WubiCandidateSource，按键路由跟 kPlainPinyin 共用同一套
//   "数字直选、空格直选"逻辑（Session::RouteDigitKeyDirect）。
enum class InputScheme { kSmartAbc, kPlainPinyin, kWubi };

// 线上/配置文件用的字符串形式（"smartabc"/"pinyin"/"wubi"）<-> InputScheme 互转。
// 集中放这里，避免 engine_main.cpp / dispatcher.cpp 各自散落一份重复的 if/else
// 映射表（三处以上重复即应收敛，见 code-quality-standards.md §3.1）。
inline const char* InputSchemeToMethodString(InputScheme scheme) noexcept {
    switch (scheme) {
        case InputScheme::kPlainPinyin: return "pinyin";
        case InputScheme::kWubi: return "wubi";
        case InputScheme::kSmartAbc: default: return "smartabc";
    }
}
inline std::optional<InputScheme> MethodStringToInputScheme(const std::string& method) {
    if (method == "smartabc") return InputScheme::kSmartAbc;
    if (method == "pinyin") return InputScheme::kPlainPinyin;
    if (method == "wubi") return InputScheme::kWubi;
    return std::nullopt;
}

// 判定 + 产出候选所需的最小上下文。M1 字段够用；M2+ 按需增字段。
struct InputContext {
    InputMode mode = InputMode::kChinese;
    std::string raw;         // 本次组字已累积的原始按键（拼音字母 / 待判定的单个标点字符）
    bool composing = false;  // 是否已处于组字态（影响标点是否走"取消组字直接标点"路径）
    // 2026-09-12：外部候选源（ExtensionCandidateSource）用来判断"当前输入框是什么类型
    // 字段"（如 "phone"），空 = 无提示。由 TIP 经 setFieldHint 告诉引擎（目前没有真实
    // 自动检测机制——UIA 检测是 TIP 侧未来工作，这里先只搭好"提示怎么流转"这条通路，
    // 见 docs/decisions/input-engine/20260912-extension-candidate-provider.md）。
    std::string field_hint;
    // 2026-09-13：当前会话使用的输入方案，见 InputScheme 注释。
    InputScheme scheme = InputScheme::kSmartAbc;
};

// 与 Session 交互的最小接口：Handles 判定这次输入该不该由自己接管；
// Produce 只在 Handles 为 true 后调用，产出候选列表（可能只有 1 项，如英文透传/标点）。
class CandidateSource {
public:
    virtual ~CandidateSource() = default;
    virtual bool Handles(const InputContext& ctx) const = 0;
    virtual std::vector<CandidateItem> Produce(const InputContext& ctx) = 0;

    // true：Produce() 的结果不需要用户翻页/选字确认，Session 应立即取候选[0]上屏
    // 并结束组字（标点、英文透传）。默认 false（pinyin：需要用户选字/翻页）。
    virtual bool AutoCommit() const { return false; }

    // true：candidates_ 的下标跟 LibPinyinEngine 内部候选数组一一对应，SelectCandidate/
    // CommitComposition 要走 engine_.Choose()/CurrentSentence()（分段确认、整句语境）。
    // false（默认）：candidates_ 是这个来源自己攒的、跟 libpinyin 无关的原子候选列表
    // （如 number_currency），选中即直接把 candidates_[index].text 当整句提交，不查
    // libpinyin。见 session.cpp DECISION（M5 前修复：数字/金额候选选不中的真 bug）。
    virtual bool UsesEngineChoose() const { return false; }
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_CANDIDATE_SOURCE_HPP
