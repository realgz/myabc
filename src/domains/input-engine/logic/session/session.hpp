// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/session/session.hpp --- 单个 sessionId 的组字状态机
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2/§3.3
//
// 职责边界：TIP 侧只做"这个键值不值得问引擎"的本地快速判断（key_router），真正的
// 组字状态（是否在组字、raw 拼音、候选、翻页）全部由本类（引擎侧）持有——TIP 是哑终端。
//
// DECISION: M1 englishMode 由 TIP 本地处理（不经 IPC，直接透传，无候选窗），engine 侧
// InputMode::kEnglish / EnglishPassthroughSource 是为后续里程碑（如英文联想）预留的骨架，
// M1 实际不会被触发到。见 docs/decisions/_debt-log.md 2026-09-11。

#ifndef MYABC_ENGINE_SESSION_HPP
#define MYABC_ENGINE_SESSION_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "candidate/candidate_source.hpp"
#include "candidate/source_registry.hpp"
#include "libpinyin_wrapper.hpp"

namespace myabc::engine {

struct SessionOptions {
    unsigned page_size = 9;
    std::string page_prev_keys = "-,";   // 任一字符触发上一页
    std::string page_next_keys = "=.";   // 任一字符触发下一页（中文模式无逗号/句号歧义，
                                         // 因为标点已被 PunctuationSource 在"非组字态"拦下）
    std::string select_keys = "123456789";
    // M4：借道 SessionOptions 这个"配置包"把 Dispatcher 构造 SourceRegistry 需要的配置
    // 传下去（不是 Session 自己的行为，见 dispatcher.cpp）。
    char number_lead_key = 'i';
    bool bihuo_enabled = true;
    std::string bihuo_data_path;   // 空 = 不加载任何笔形数据（表为空，等价功能关闭但安全）
    // M5（plan 06 §3.1/§3.5）：拼音来源整句提交后是否调用 engine_.RememberUserInput +
    // Train（用户自学习）。只影响 UsesEngineChoose()==true 的来源（拼音）；number_currency
    // 这类原子候选来源永远不训练，见 session.cpp DECISION。
    bool learning_enabled = true;
    // 借道这个"配置包"传给 Dispatcher（不是 Session 自己的行为，跟 number_lead_key
    // 同样的路数，见 dispatcher.cpp）：每 N 次成功 commit 主动落盘一次学习结果。
    // 0 = 关闭自动保存（只在 idle-exit/shutdown 时存）。
    unsigned autosave_every_n_commits = 20;
};

struct CandidateView {
    std::string text;
};

struct SessionResult {
    bool handled = false;
    bool composing = false;   // v2: TIP 靠这个字段判断要不要维持 ITfComposition（不再靠
                              // candidates 是否为空推断——v2 起 candidates 不给 TIP 了）
    std::string preedit;
    std::string raw_input;
    std::vector<CandidateView> candidates;   // 当前页；v2 起只经 uiShow 推给 myabc-ui
    int page_index = 0;
    int page_size = 0;
    int page_total = 0;
    bool has_commit = false;
    std::string commit;
    // 智能ABC 风格空格两段式确认（见 session.cpp ProcessKey 的 VK_SPACE 分支 DECISION）：
    // -1 = 当前没有"架着待确认"的候选；>=0 = 当前页里这个下标的候选被架着，UI 应高亮
    // 它但还没有上屏。目前实现里只会是 0（架的永远是当前页第一项）。
    int armed_index = -1;
};

class Session {
public:
    Session(std::uint32_t id, LibPinyinEngine& engine, SourceRegistry& registry,
            SessionOptions opts);

    bool composing() const noexcept { return composing_; }

    // vk：Win32 虚拟键码（VK_BACK/VK_ESCAPE/VK_SPACE 等，字母数字直接用 ASCII 值）。
    // ch：若为可打印字符则是其 ASCII 值，否则 0。
    SessionResult ProcessKey(int vk, unsigned ch);
    SessionResult SelectCandidate(int index_in_page);
    SessionResult PageCandidates(int delta);
    SessionResult CommitComposition();   // 直接确认候选[0]（当前最佳整句）
    SessionResult CancelComposition();
    void FocusOut();   // M1：清空状态，不跨窗口保留（见 DECISION 注释）

private:
    SessionResult Recompute();     // 用 raw_ 通过 registry_ 重新产出候选，page_index 归零
    SessionResult BuildViewResult(bool handled, bool has_commit = false,
                                 std::string commit = {});
    void ResetToIdle();
    // M5：拼音来源整句提交前调用（必须在 ResetToIdle()/engine_.Reset() 之前——
    // RememberUserInput/Train 读取的是当前 matrix/nbest 状态）。非拼音来源、
    // learning_enabled=false 时是空操作。
    void MaybeTrain(const std::string& committed_text);

    std::uint32_t id_;
    LibPinyinEngine& engine_;
    SourceRegistry& registry_;
    SessionOptions opts_;

    bool composing_ = false;
    std::string raw_;                          // 原始按键（拼音字母），Choose 不修改它
    std::string last_partial_sentence_;        // 部分确认后的展示文本；追加/退格字母时清空
    std::vector<CandidateItem> candidates_;    // 全量（未分页）
    int page_index_ = 0;
    // 空格两段式确认的"架住"状态：候选数>1 时第一次按空格只把它置 true（高亮候选[0]，
    // 不上屏）；第二次按空格（或直接按数字键）才真正 SelectCandidate。raw_ 变化
    // （追加字母/退格/翻页/换到下一段）一律清掉，语义上"这次架的是哪份候选"已经过期。
    bool space_armed_ = false;
    // 当前 candidates_ 是哪个 CandidateSource 产出的（CandidateSource::UsesEngineChoose()），
    // 决定 SelectCandidate/CommitComposition 该不该查 libpinyin（见 session.cpp DECISION，
    // M5 前修复：数字/金额候选选不中的真 bug）。Recompute() 里跟 candidates_ 一起刷新。
    bool current_source_uses_engine_choose_ = false;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_SESSION_HPP
