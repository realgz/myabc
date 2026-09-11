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

SessionResult Session::ProcessKey(int vk, unsigned ch) {
    if (composing_) {
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
        if (vk == VK_SPACE) return SelectCandidate(0);

        // M4：一旦 raw_ 以 number_lead_key 开头（如 "i"），数字/小数点/负号必须能
        // 继续拼数字本身（"i2025" 的 '2'..'5'），不能被 select_keys（"123456789"）
        // 截胡当成选字——数字模式下只能用空格选候选[0]或 Esc 取消，见
        // docs/decisions/_debt-log.md 2026-09-11。
        const bool in_number_mode = !raw_.empty() && raw_.front() == opts_.number_lead_key;
        if (in_number_mode && ch != 0) {
            const char c = static_cast<char>(ch);
            if ((c >= '0' && c <= '9') || c == '.' || c == '-') {
                raw_ += c;
                return Recompute();
            }
        }

        // M4：笔形辅助码（DECISION: docs/decisions/_debt-log.md 2026-09-11「笔形辅助码
        // 触发键」）。拼音后直接接数字（"wo3"）跟 select_keys（默认全体数字，M1 起的
        // 既有行为）无法共存，因此用独立触发键 bihuo_lead_key（默认反引号）：按一次
        // 进入笔形输入态（"wo" -> "wo`"），之后 1-5 才追加为笔形码本身，不再落入
        // select_keys/letter 分支。不在数字模式下才生效——数字模式的数字另有含义。
        if (!in_number_mode && opts_.bihuo_enabled && ch != 0) {
            const char c = static_cast<char>(ch);
            const bool in_bihuo_mode = raw_.find(opts_.bihuo_lead_key) != std::string::npos;
            if (in_bihuo_mode && c >= '1' && c <= '5') {
                raw_ += c;
                return Recompute();
            }
            if (!in_bihuo_mode && c == opts_.bihuo_lead_key) {
                raw_ += c;
                return Recompute();
            }
        }

        if (ch != 0) {
            const char c = static_cast<char>(ch);
            if (opts_.select_keys.find(c) != std::string::npos) {
                return SelectCandidate(static_cast<int>(opts_.select_keys.find(c)));
            }
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
        ResetToIdle();
        return BuildViewResult(true, true, sentence_text);
    }

    candidates_ = engine_.candidates();
    current_source_uses_engine_choose_ = true;   // engine_.candidates() 天然 1:1，无需过滤映射
    page_index_ = 0;
    last_partial_sentence_ = sentence_text;
    return BuildViewResult(true);
}

SessionResult Session::PageCandidates(int delta) {
    if (!composing_ || candidates_.empty()) return BuildViewResult(false);

    const int total = static_cast<int>(
        (candidates_.size() + opts_.page_size - 1) / std::max<unsigned>(1, opts_.page_size));
    int idx = page_index_ + delta;
    idx = std::clamp(idx, 0, std::max(0, total - 1));
    page_index_ = idx;
    return BuildViewResult(true);
}

SessionResult Session::CommitComposition() {
    if (!composing_) return BuildViewResult(false);
    std::string text;
    if (current_source_uses_engine_choose_) {
        text = last_partial_sentence_.empty() ? engine_.CurrentSentence() : last_partial_sentence_;
        if (text.empty()) text = raw_;   // 兜底：解析失败也不吞用户输入
    } else {
        // 原子候选来源（如 number_currency）：engine_.CurrentSentence() 跟它无关
        // （libpinyin 从没解析过这个 raw_），直接取候选[0]，跟 Recompute() 的
        // AutoCommit 分支同一套兜底逻辑（M5 前修复真 bug，见 _debt-log.md）。
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
    return r;
}

void Session::ResetToIdle() {
    composing_ = false;
    raw_.clear();
    last_partial_sentence_.clear();
    candidates_.clear();
    page_index_ = 0;
    engine_.Reset();
}

}  // namespace myabc::engine
