// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/unit/learning_policy_test.cpp --- AutosaveCounter 单测（M5-5）
//
// 依据：docs/plan/06-m5-user-dict-learning-plan.md §5 M5-5
//
// DECISION（见 src/domains/input-engine/logic/learning_policy.hpp 头注释、
// docs/decisions/_debt-log.md 2026-09-11）：plan 原判据写"晋升阈值、去重、衰减逻辑
// 断言"，对应 min_uses_to_promote/decay 那层未实现（libpinyin 自带的
// pinyin_remember_user_input 已经是按 count 累加的自学习模型，不需要重复造轮子）；
// 这里测的是实际写的那部分自建逻辑——多久落盘一次的计数器。

#include <cstdio>
#include <cstdlib>

#include "learning_policy.hpp"

namespace {
int g_failures = 0;

void Check(bool cond, const char* what) {
    if (cond) {
        std::printf("OK: %s\n", what);
    } else {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++g_failures;
    }
}
}  // namespace

int main() {
    using myabc::engine::AutosaveCounter;

    // ---- every_n_commits == 0：关闭，OnCommit 永远返回 false --------------
    {
        AutosaveCounter c(0);
        bool any_true = false;
        for (int i = 0; i < 100; ++i) any_true = any_true || c.OnCommit();
        Check(!any_true, "every_n_commits=0 时 OnCommit 永远不触发（关闭自动保存）");
    }

    // ---- every_n_commits == 1：每次 commit 都触发 --------------------------
    {
        AutosaveCounter c(1);
        Check(c.OnCommit(), "every_n_commits=1 时第 1 次 commit 就触发");
        Check(c.OnCommit(), "every_n_commits=1 时第 2 次 commit 也触发");
    }

    // ---- every_n_commits == 3：恰好第 3 次触发，之后计数器清零重新计 --------
    {
        AutosaveCounter c(3);
        Check(!c.OnCommit(), "every_n_commits=3 第 1 次不触发");
        Check(!c.OnCommit(), "every_n_commits=3 第 2 次不触发");
        Check(c.OnCommit(), "every_n_commits=3 第 3 次触发");
        Check(c.pending_count() == 0, "触发后计数器清零");
        Check(!c.OnCommit(), "清零后第 1 次（全局第 4 次）不触发");
        Check(!c.OnCommit(), "清零后第 2 次（全局第 5 次）不触发");
        Check(c.OnCommit(), "清零后第 3 次（全局第 6 次）再次触发（周期性）");
    }

    // ---- pending_count 反映还差几次（供 debug/上层观察，非必须但顺手测一下）---
    {
        AutosaveCounter c(5);
        c.OnCommit();
        c.OnCommit();
        Check(c.pending_count() == 2, "pending_count 正确累计未触发的 commit 数");
    }

    if (g_failures == 0) {
        std::puts("learning_policy_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "learning_policy_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
