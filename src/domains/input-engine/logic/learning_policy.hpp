// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/learning_policy.hpp --- 用户词库自学习：落盘节奏策略
//
// 依据：docs/plan/06-m5-user-dict-learning-plan.md §3.1/§3.2
//
// DECISION: docs/decisions/_debt-log.md 2026-09-11——plan 06 §3.1 原设想的
// learning_policy.cpp 承担"何时 commit 学习"（晋升阈值/去重/衰减）这类决策，但
// libpinyin 自带的 pinyin_remember_user_input 已经是一个按 count 累加的自学习模型
// （见 Session::MaybeTrain 调用点），不需要在它之上再叠一层阈值/去重/衰减判断——
// 真正需要的自建逻辑只剩"多久落盘一次"这一件事（防 taskkill/崩溃丢学习结果），
// 就是这个文件的全部内容。不依赖 libpinyin/引擎状态，纯计数器，可独立单测。

#ifndef MYABC_ENGINE_LEARNING_POLICY_HPP
#define MYABC_ENGINE_LEARNING_POLICY_HPP

namespace myabc::engine {

// 每 N 次成功 commit 触发一次落盘。every_n_commits==0 表示关闭（永远不通过这个计数器
// 触发，只依赖 idle-exit/shutdown 的落盘）。
class AutosaveCounter {
public:
    explicit AutosaveCounter(unsigned every_n_commits) : every_n_commits_(every_n_commits) {}

    // 每次成功 commit（有 commit 内容的 processKey/selectCandidate/commitComposition）
    // 调用一次。返回 true 表示这次该落盘了——调用方落盘后计数器已经自动清零，不需要
    // 调用方再手动 Reset()。
    bool OnCommit() {
        if (every_n_commits_ == 0) return false;
        if (++count_ < every_n_commits_) return false;
        count_ = 0;
        return true;
    }

    unsigned every_n_commits() const noexcept { return every_n_commits_; }
    unsigned pending_count() const noexcept { return count_; }   // 供测试断言用

private:
    unsigned every_n_commits_;
    unsigned count_ = 0;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_LEARNING_POLICY_HPP
