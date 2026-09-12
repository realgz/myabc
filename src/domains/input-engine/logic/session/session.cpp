// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/session/session.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.2/§3.3

#include "session.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>

#include "candidate/punctuation_source.hpp"

namespace myabc::engine {

namespace {
bool IsAsciiLetter(unsigned ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}
char ToLowerAscii(unsigned ch) { return static_cast<char>(std::tolower(static_cast<int>(ch))); }
}  // namespace

Session::Session(std::uint32_t id, LibPinyinEngine& engine, SourceRegistry& registry,
                 SessionOptions opts)
    : id_(id), engine_(engine), registry_(registry), opts_(std::move(opts)) {}

SessionResult Session::ProcessKey(int vk, unsigned ch, bool ctrl) {
    if (composing_) {
        // DECISION（用户 2026-09-12 明确要求，见 docs/decisions/_debt-log.md）：
        // Ctrl+数字任何时候都直接当 select_keys 处理，不用先按空格、不管是不是在
        // 数字模式/笔形模式——按住 Ctrl 是用户主动明确表达"我现在就要选第几个"的
        // 信号，优先级高于其它数字含义判断，所以放在组字态分支的最前面，抢在
        // VK_SPACE/数字模式/笔形码这些判断之前生效。
        if (ctrl && ch != 0) {
            const char c = static_cast<char>(ch);
            const auto pos = opts_.select_keys.find(c);
            if (pos != std::string::npos) {
                return SelectCandidate(static_cast<int>(pos));
            }
        }

        if (vk == VK_BACK) {
            if (!last_partial_sentence_.empty()) {
                // 有部分确认约束：先撤销约束，回到基于 raw_ 的全新猜测，不吃字母。
                return Recompute();
            }
            if (raw_.empty()) return BuildViewResult(false);
            raw_.pop_back();
            if (raw_.empty()) {
                ResetToIdle();
                return BuildViewResult(true);
            }
            return Recompute();
        }
        if (vk == VK_ESCAPE) return CancelComposition();
        // DECISION（智能ABC 风格空格两段式确认，用户 2026-09-11 明确要求，见
        // docs/decisions/_debt-log.md）：候选只有 0/1 个时没有歧义，空格直接选中上屏
        // （原有行为）。候选 >=2 个时空格不再直接吞下候选[0]——第一次空格只是把
        // 候选[0]"架"上（UI 高亮，不上屏，组字继续），逼用户看一眼是不是真的要这个；
        // 第二次空格（或直接按数字键，数字键不受这套两段式影响）才真正选中上屏。
        // raw_ 只要一变化（追加字母/退格/翻页/换段）就通过 Recompute()/相关分支清掉
        // space_armed_，防止"架住的是上一份候选"这种错位。
        if (vk == VK_SPACE) {
            if (candidates_.size() <= 1) return SelectCandidate(0);
            if (!space_armed_) {
                space_armed_ = true;
                return BuildViewResult(true);
            }
            space_armed_ = false;
            return SelectCandidate(0);
        }

        // DECISION（用户 2026-09-11 进一步明确的完整规格，取代 M4 时"独立触发键"的
        // 旧决策——见 docs/decisions/_debt-log.md 2026-09-11「笔形辅助码触发键」条目
        // 的废弃说明）：数字键在按过一次空格（space_armed_）之前统一表示"继续拼数字/
        // 笔形码"，按过一次空格之后（进入"数字选择状态"）才表示 select_keys 那种
        // "选第几个候选"。这样"拼音/数字直接接数字"（"i2025"、"wo31"）和"数字选字"
        // 就不再冲突，不需要额外的触发键——拼音后数字要么被下面的模式判断吃掉当输入
        // 本身的一部分，要么（没有匹配的模式时）落到 select_keys 分支，但 select_keys
        // 分支现在只在 space_armed_ 时生效。
        const bool in_number_mode = !raw_.empty() && raw_.front() == opts_.number_lead_key;
        if (!space_armed_ && in_number_mode && ch != 0) {
            const char c = static_cast<char>(ch);
            if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
                raw_ += c;
                return Recompute();
            }
        }

        // 笔形辅助码：数字 1-5 直接追加为笔形码后缀（"wo" -> "wo3" -> "wo31"），不需要
        // 触发键，只要还没进入数字选择状态、也不是数字模式（两者用同一批字符但语义
        // 互斥，由 raw_ 首字符已经区分）。
        if (!space_armed_ && !in_number_mode && opts_.bihuo_enabled && ch != 0) {
            const char c = static_cast<char>(ch);
            if (c >= '1' && c <= '5') {
                raw_ += c;
                return Recompute();
            }
        }

        // 数字选择状态（已按过一次空格）：数字键才表示"选第几个候选"。
        if (space_armed_ && ch != 0) {
            const char c = static_cast<char>(ch);
            if (opts_.select_keys.find(c) != std::string::npos) {
                return SelectCandidate(static_cast<int>(opts_.select_keys.find(c)));
            }
        }

        if (ch != 0) {
            const char c = static_cast<char>(ch);
            if (opts_.page_prev_keys.find(c) != std::string::npos) return PageCandidates(-1);
            if (opts_.page_next_keys.find(c) != std::string::npos) return PageCandidates(+1);
            if (IsAsciiLetter(ch)) {
                raw_ += ToLowerAscii(ch);
                return Recompute();
            }
        }
        return BuildViewResult(false);   // 组字态下不认识的键：不吃，交还宿主
    }

    // 非组字态：只有字母（起组字）或有映射的标点（直接上屏）值得处理。
    if (ch == 0) return BuildViewResult(false);
    const char c = static_cast<char>(ch);
    if (IsAsciiLetter(ch)) {
        composing_ = true;
        raw_ = std::string(1, ToLowerAscii(ch));
        return Recompute();
    }
    if (PunctuationSource::IsMappedPunctuation(c)) {
        composing_ = true;   // Recompute 里 AutoCommit 会立刻结束
        raw_ = std::string(1, c);
        return Recompute();
    }
    return BuildViewResult(false);
}

SessionResult Session::SelectCandidate(int index_in_page) {
    if (!composing_ || index_in_page < 0) return BuildViewResult(false);

    const std::size_t global_index =
        static_cast<std::size_t>(page_index_) * opts_.page_size + static_cast<std::size_t>(index_in_page);
    if (global_index >= candidates_.size()) return BuildViewResult(false);

    // DECISION（M5 前修复真 bug，见 docs/decisions/_debt-log.md）：只有
    // UsesEngineChoose()==true 的来源（拼音）的 candidates_ 下标才对应 libpinyin 内部
    // 候选数组；number_currency 这类"原子候选"来源的 candidates_ 是自己攒的、跟
    // libpinyin 无关的列表，选中即直接把这一项整句提交，绝不能传给 engine_.Choose()——
    // 之前统一走 Choose() 导致这类候选要么选不中（下标查不到，静默卡在组字态），要么
    // （笔形过滤后）选中的字跟显示的对不上（下标错位）。
    if (!current_source_uses_engine_choose_) {
        std::string text = candidates_[global_index].text;
        ResetToIdle();
        return BuildViewResult(true, true, std::move(text));
    }

    // 笔形过滤会让显示下标跟 libpinyin 内部下标错位；engine_index>=0 时用它，否则
    // （未过滤）显示下标本身就是内部下标。见 CandidateItem::engine_index 注释。
    const CandidateItem& item = candidates_[global_index];
    const std::size_t engine_idx = item.engine_index >= 0
                                       ? static_cast<std::size_t>(item.engine_index)
                                       : global_index;

    std::string sentence_text;
    const bool done = engine_.Choose(engine_idx, sentence_text);
    if (done) {
        MaybeTrain(sentence_text);   // 必须在 ResetToIdle() 之前（见头文件注释）
        ResetToIdle();
        return BuildViewResult(true, true, sentence_text);
    }

    candidates_ = engine_.candidates();
    current_source_uses_engine_choose_ = true;   // engine_.candidates() 天然 1:1，无需过滤映射
    page_index_ = 0;
    space_armed_ = false;   // 换到剩余段的新候选列表，之前架的那份已经过期
    last_partial_sentence_ = sentence_text;
    // 跟 Recompute() 同一条"只剩一个候选就自动选中"规则，见那边的 DECISION 注释。
    if (candidates_.size() == 1) return SelectCandidate(0);
    return BuildViewResult(true);
}

SessionResult Session::PageCandidates(int delta) {
    if (!composing_ || candidates_.empty()) return BuildViewResult(false);

    const int total = static_cast<int>(
        (candidates_.size() + opts_.page_size - 1) / std::max<unsigned>(1, opts_.page_size));
    int idx = page_index_ + delta;
    idx = std::clamp(idx, 0, std::max(0, total - 1));
    page_index_ = idx;
    space_armed_ = false;   // 翻页后"候选[0]"已经是另一页的另一项，之前架的状态作废
    return BuildViewResult(true);
}

SessionResult Session::CommitComposition() {
    if (!composing_) return BuildViewResult(false);
    std::string text;
    if (current_source_uses_engine_choose_) {
        text = last_partial_sentence_.empty() ? engine_.CurrentSentence() : last_partial_sentence_;
        if (text.empty()) {
            text = raw_;   // 兜底：解析失败也不吞用户输入，此时没有可训练的真实句子
        } else {
            MaybeTrain(text);   // 必须在 ResetToIdle() 之前（见头文件注释）
        }
    } else {
        // 原子候选来源（如 number_currency）：engine_.CurrentSentence() 跟它无关
        // （libpinyin 从没解析过这个 raw_），直接取候选[0]，跟 Recompute() 的
        // AutoCommit 分支同一套兜底逻辑（M5 前修复真 bug，见 _debt-log.md）。不训练——
        // 这类候选不是拼音句子，pinyin_remember_user_input 无从谈起。
        text = candidates_.empty() ? raw_ : candidates_.front().text;
    }
    ResetToIdle();
    return BuildViewResult(true, true, text);
}

SessionResult Session::CancelComposition() {
    if (!composing_) return BuildViewResult(false);
    ResetToIdle();
    return BuildViewResult(true);
}

void Session::FocusOut() { ResetToIdle(); }

SessionResult Session::Recompute() {
    last_partial_sentence_.clear();
    space_armed_ = false;   // raw_ 变了，之前架着的候选（如果有）已经过期
    const InputContext ctx{InputMode::kChinese, raw_, composing_};
    CandidateSource* src = registry_.Resolve(ctx);
    if (src == nullptr) {
        ResetToIdle();
        return BuildViewResult(false);
    }

    current_source_uses_engine_choose_ = src->UsesEngineChoose();
    candidates_ = src->Produce(ctx);
    page_index_ = 0;

    if (src->AutoCommit()) {
        std::string text = candidates_.empty() ? raw_ : candidates_.front().text;
        ResetToIdle();
        return BuildViewResult(true, true, text);
    }

    // DECISION（用户 2026-09-11 明确要求，见 docs/decisions/_debt-log.md）：候选收窄到
    // 只剩一个（通常是笔形码筛掉了所有歧义）时不用等用户按空格，直接自动选中——
    // "只剩一个候选才自动选中"跟 VK_SPACE 分支"候选<=1 直接选中"是同一条规则，只是
    // 这里在候选一出现就立刻应用，不用等按键。SelectCandidate(0) 走一致的
    // engine-choose/原子候选分流逻辑，不重复实现。
    if (candidates_.size() == 1) return SelectCandidate(0);

    return BuildViewResult(true);
}

SessionResult Session::BuildViewResult(bool handled, bool has_commit, std::string commit) {
    SessionResult r;
    r.handled = handled;
    r.composing = composing_;
    r.raw_input = raw_;
    r.preedit = last_partial_sentence_.empty() ? raw_ : last_partial_sentence_;
    r.page_size = static_cast<int>(opts_.page_size);
    r.page_total = candidates_.empty()
                       ? 0
                       : static_cast<int>((candidates_.size() + opts_.page_size - 1) /
                                          std::max<unsigned>(1, opts_.page_size));
    r.page_index = page_index_;

    const std::size_t start = static_cast<std::size_t>(page_index_) * opts_.page_size;
    for (std::size_t i = start; i < candidates_.size() && i < start + opts_.page_size; ++i) {
        r.candidates.push_back(CandidateView{candidates_[i].text});
    }
    r.has_commit = has_commit;
    r.commit = std::move(commit);
    r.armed_index = space_armed_ ? 0 : -1;
    return r;
}

void Session::MaybeTrain(const std::string& committed_text) {
    if (!opts_.learning_enabled || committed_text.empty()) return;
    // 顺序：先 RememberUserInput（不管这句话本来存不存在于词库，都当一个词条记住/
    // 加计数——这是 libpinyin 真正的"造新词"入口，见 pinyin.h 文档），再 Train
    // （bigram/unigram 调频）。两者都读当前 matrix/nbest 状态，必须在调用方
    // ResetToIdle()（进而 engine_.Reset()）之前完成。
    engine_.RememberUserInput(committed_text);
    engine_.Train();
}

void Session::ResetToIdle() {
    composing_ = false;
    raw_.clear();
    last_partial_sentence_.clear();
    candidates_.clear();
    page_index_ = 0;
    space_armed_ = false;
    engine_.Reset();
}

}  // namespace myabc::engine
