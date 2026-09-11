// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/libpinyin_wrapper.hpp --- libpinyin 封装
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.1
//       docs/decisions/pinyin-engine/20260909-adopt-libpinyin-mingw-engine-process.md
//
// 只有本文件（+ .cpp）可以 include <pinyin.h> / glib 类型。对外只暴露 std::string /
// std::vector，不泄漏 gchar*、pinyin_context_t 等 —— 上层（session/dispatcher）不碰 libpinyin。
//
// DECISION: 候选模型 —— libpinyin 的 pinyin_guess_candidates(offset) 在 offset 前调用过
// pinyin_guess_sentence 后，会把"整句猜测"（NBEST_MATCH_CANDIDATE）连同该 offset 上的逐词候选
// 一起塞进同一个列表（见 third_party/libpinyin/src/pinyin.cpp:_prepend_sentence_candidates）。
// 所以候选[0] 天然就是当前最佳整句，候选[1..] 是给定位置的备选词——不需要额外拼一份"整句候选"逻辑。
// 详见 docs/decisions/_debt-log.md 2026-09-11。

#ifndef MYABC_ENGINE_LIBPINYIN_WRAPPER_HPP
#define MYABC_ENGINE_LIBPINYIN_WRAPPER_HPP

#include <cstddef>
#include <string>
#include <vector>

// 前向声明，避免把 <pinyin.h>（连带 glib.h）泄漏进包含本头的其它文件。
struct _pinyin_context_t;
struct _pinyin_instance_t;
struct _lookup_candidate_t;

namespace myabc::engine {

struct CandidateItem {
    std::string text;
    bool is_sentence = false;   // true = 整句候选（NBEST_MATCH_CANDIDATE）
    // -1（默认）= 这一项在候选列表里的下标就是 libpinyin 内部候选数组下标（未被任何
    // 过滤/重排改变，如 Choose() 直接返回的原始 candidates()）。>=0 = 显式指定内部
    // 下标——某个 CandidateSource（如笔形过滤后的 PinyinCandidateSource）产出的列表
    // 是原始候选的子集/重排时必须设置，否则 Session::SelectCandidate 传给 libpinyin
    // 的下标会跟用户看到的候选错位（M5 前修复的真 bug，见 _debt-log.md）。只有
    // CandidateSource::UsesEngineChoose()==true 的来源需要关心这个字段。
    int engine_index = -1;
};

class LibPinyinEngine {
public:
    LibPinyinEngine();
    ~LibPinyinEngine();

    LibPinyinEngine(const LibPinyinEngine&) = delete;
    LibPinyinEngine& operator=(const LibPinyinEngine&) = delete;

    // model_dir：系统词库/模型目录（含 table.conf + *.bin + bigram.db）。
    // user_dir：用户可写目录（不存在会自动创建）；本会话不调用 pinyin_save，不跨重启持久化
    // （DECISION: docs/plan/02-...md §4 不改动清单 —— M1 不承诺跨重启自学习）。
    bool Init(const std::string& model_dir, const std::string& user_dir);
    bool ready() const noexcept { return instance_ != nullptr; }

    // M3（plan 04 §3.5）：按 config.input.scheme / config.input.fuzzy 调整解析选项。
    // incomplete_enabled：quanpin=false（严格全拼，不接受简拼/混拼）；
    //                     jianpin/hunpin=true（libpinyin 统一用 PINYIN_INCOMPLETE 处理
    //                     简拼/混拼——两者对 libpinyin 而言是同一个开关，见
    //                     docs/decisions/_debt-log.md 2026-09-11 的简化说明）。
    // fuzzy_names：模糊音开关名集合，取值见 libpinyin_wrapper.cpp 里的映射表
    // （c_ch/s_sh/z_zh/f_h/g_k/l_n/l_r/an_ang/en_eng/in_ing/all）；未识别的名字忽略。
    void ApplyInputOptions(bool incomplete_enabled, const std::vector<std::string>& fuzzy_names);

    // 用完整原始拼音串重新猜测（丢弃此前任何 Choose 产生的分段约束）。
    // M1 简化：追加字母 / 退格都整串重算，不做增量约束保留——见 _debt-log 2026-09-11。
    void ParseAndGuess(const std::string& raw_pinyin);

    // 当前候选页（未分页，全量列表；分页由上层 session 按 page_size 切片）。
    const std::vector<CandidateItem>& candidates() const noexcept { return candidates_; }

    // 选择第 index 个候选（0-based，对应 candidates() 的下标）。
    // 返回 true：整句已完全确定，out_sentence 是最终上屏文本，调用方应 Reset()。
    // 返回 false：部分确定，candidates() 已刷新为剩余部分的候选，组字继续。
    bool Choose(std::size_t index, std::string& out_sentence);

    // 当前"最佳整句"文本（已确定前缀 + 猜测的剩余部分），供预编辑显示。
    std::string CurrentSentence() const;

    void Reset();      // 清约束 + 矩阵，回到空白态（cursor=0，parsed_len=0）
    void Train();      // pinyin_train(instance, 0) —— 本次会话内自适应，不 save

    // M2（plan 03 §3.4）：引擎空闲自退出前调用一次，尽力落盘用户词库。
    // M1 起故意不在每次 commit 后调用（不承诺跨重启持久化）；这里是唯一的显式落盘点。
    void Save();

private:
    void RecomputeCandidates();

    _pinyin_context_t* context_ = nullptr;
    _pinyin_instance_t* instance_ = nullptr;
    std::size_t cursor_ = 0;
    std::size_t parsed_len_ = 0;

    std::vector<CandidateItem> candidates_;
    std::vector<_lookup_candidate_t*> raw_candidates_;   // 与 candidates_ 一一对应，指针归 libpinyin 所有
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_LIBPINYIN_WRAPPER_HPP
