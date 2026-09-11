// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/libpinyin_wrapper.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.1

#include "libpinyin_wrapper.hpp"

#include <pinyin.h>

#include <cstdio>

namespace myabc::engine {

namespace {
// DECISION: docs/decisions/pinyin-engine/20260909-adopt-libpinyin-mingw-engine-process.md
// 取自 third_party/libpinyin/tests/test_pinyin.cpp 的规范用法：全拼 + 容错 + 动态调频。
constexpr pinyin_option_t kOptions = static_cast<pinyin_option_t>(
    PINYIN_INCOMPLETE | PINYIN_CORRECT_ALL | USE_DIVIDED_TABLE | USE_RESPLIT_TABLE |
    DYNAMIC_ADJUST);

constexpr guint kSortOption = SORT_BY_PHRASE_LENGTH | SORT_BY_FREQUENCY;
}  // namespace

LibPinyinEngine::LibPinyinEngine() = default;

LibPinyinEngine::~LibPinyinEngine() {
    if (instance_) ::pinyin_free_instance(reinterpret_cast<pinyin_instance_t*>(instance_));
    if (context_) ::pinyin_fini(reinterpret_cast<pinyin_context_t*>(context_));
}

bool LibPinyinEngine::Init(const std::string& model_dir, const std::string& user_dir) {
    ::g_mkdir_with_parents(user_dir.c_str(), 0755);

    auto* ctx = ::pinyin_init(model_dir.c_str(), user_dir.c_str());
    if (ctx == nullptr) return false;

    ::pinyin_set_options(ctx, kOptions);

    auto* inst = ::pinyin_alloc_instance(ctx);
    if (inst == nullptr) {
        ::pinyin_fini(ctx);
        return false;
    }

    context_ = reinterpret_cast<_pinyin_context_t*>(ctx);
    instance_ = reinterpret_cast<_pinyin_instance_t*>(inst);
    return true;
}

void LibPinyinEngine::ParseAndGuess(const std::string& raw_pinyin) {
    if (!ready()) return;
    auto* inst = reinterpret_cast<pinyin_instance_t*>(instance_);

    ::pinyin_reset(inst);   // 丢弃此前 Choose 产生的约束，见头文件 DECISION 注释。
    cursor_ = 0;
    parsed_len_ = ::pinyin_parse_more_full_pinyins(inst, raw_pinyin.c_str());
    ::pinyin_guess_sentence(inst);
    RecomputeCandidates();
}

void LibPinyinEngine::RecomputeCandidates() {
    candidates_.clear();
    raw_candidates_.clear();
    if (!ready()) return;
    auto* inst = reinterpret_cast<pinyin_instance_t*>(instance_);

    if (!::pinyin_guess_candidates(inst, cursor_, kSortOption)) return;

    guint n = 0;
    if (!::pinyin_get_n_candidate(inst, &n)) return;

    candidates_.reserve(n);
    raw_candidates_.reserve(n);
    for (guint i = 0; i < n; ++i) {
        lookup_candidate_t* c = nullptr;
        if (!::pinyin_get_candidate(inst, i, &c) || c == nullptr) continue;

        const gchar* s = nullptr;
        ::pinyin_get_candidate_string(inst, c, &s);
        lookup_candidate_type_t type = NORMAL_CANDIDATE;
        ::pinyin_get_candidate_type(inst, c, &type);

        candidates_.push_back(CandidateItem{s ? std::string(s) : std::string(),
                                            type == NBEST_MATCH_CANDIDATE});
        raw_candidates_.push_back(reinterpret_cast<_lookup_candidate_t*>(c));
    }
}

bool LibPinyinEngine::Choose(std::size_t index, std::string& out_sentence) {
    if (!ready() || index >= raw_candidates_.size()) return false;
    auto* inst = reinterpret_cast<pinyin_instance_t*>(instance_);
    auto* cand = reinterpret_cast<lookup_candidate_t*>(raw_candidates_[index]);

    const int new_cursor = ::pinyin_choose_candidate(inst, cursor_, cand);
    if (new_cursor < 0) return false;
    cursor_ = static_cast<std::size_t>(new_cursor);

    ::pinyin_guess_sentence(inst);
    out_sentence = CurrentSentence();

    if (cursor_ >= parsed_len_) {
        return true;   // 整句已完全确定
    }
    RecomputeCandidates();
    return false;
}

std::string LibPinyinEngine::CurrentSentence() const {
    if (!ready()) return {};
    auto* inst = reinterpret_cast<pinyin_instance_t*>(instance_);

    char* sentence = nullptr;
    if (!::pinyin_get_sentence(inst, 0, &sentence) || sentence == nullptr) return {};
    std::string result(sentence);
    ::g_free(sentence);
    return result;
}

void LibPinyinEngine::Reset() {
    if (!ready()) return;
    ::pinyin_reset(reinterpret_cast<pinyin_instance_t*>(instance_));
    cursor_ = 0;
    parsed_len_ = 0;
    candidates_.clear();
    raw_candidates_.clear();
}

void LibPinyinEngine::Train() {
    if (!ready()) return;
    // M1：只在本进程内存里调频（不 pinyin_save，不跨重启）——见头文件 DECISION 注释。
    ::pinyin_train(reinterpret_cast<pinyin_instance_t*>(instance_), 0);
}

void LibPinyinEngine::Save() {
    if (!ready()) return;
    ::pinyin_save(reinterpret_cast<pinyin_context_t*>(context_));
}

}  // namespace myabc::engine
