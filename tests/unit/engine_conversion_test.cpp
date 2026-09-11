// SPDX-License-Identifier: GPL-3.0-or-later
//
// tests/unit/engine_conversion_test.cpp --- libpinyin 全拼整句转换 golden 测试
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §5（M1-2）
//
// 只在 MinGW 侧构建（需要 LibPinyin）。MYABC_TEST_MODEL_DIR 由 CMake 注入
// （third_party/libpinyin/build-data/data，见 cmake/FindLibPinyin.cmake）。
//
// 回归判据（历史真实坑，见 docs/plan/02-...md §5"回归判据"）：
// - libpinyin locale 敏感解析（上游 PR #187）：本测试在非 C locale 下也应通过。

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "libpinyin_wrapper.hpp"

#ifndef MYABC_TEST_MODEL_DIR
#error "MYABC_TEST_MODEL_DIR 未定义（应由 CMake 注入）"
#endif

namespace {

int g_failures = 0;

void CheckContains(const std::string& sentence, const char* expect, const char* what) {
    if (sentence.find(expect) == std::string::npos) {
        std::fprintf(stderr, "FAIL: %s —— 期望包含 \"%s\"，实际 \"%s\"\n", what, expect,
                    sentence.c_str());
        ++g_failures;
    } else {
        std::printf("OK: %s -> %s\n", what, sentence.c_str());
    }
}

}  // namespace

int main() {
    // 回归判据：非 C locale 下也要能正常加载模型并转换（上游 PR #187 修的坑）。
    std::setlocale(LC_ALL, "Chinese_China.936");

    myabc::engine::LibPinyinEngine engine;
    const std::string model_dir = MYABC_TEST_MODEL_DIR;
    const std::string user_dir = std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") +
                                 "\\myabc-engine-conversion-test";

    if (!engine.Init(model_dir, user_dir)) {
        std::fprintf(stderr, "FAIL: LibPinyinEngine::Init 失败（model_dir=%s）\n",
                    model_dir.c_str());
        return EXIT_FAILURE;
    }

    engine.ParseAndGuess("nihao");
    CheckContains(engine.CurrentSentence(), "你好", "nihao -> 你好 (top1)");

    engine.ParseAndGuess("woshizhongguoren");
    CheckContains(engine.CurrentSentence(), "我是中国人", "woshizhongguoren -> 我是中国人 (top1)");

    // beijing：只要求候选里命中"北京"，不强求 top1（"背景"等同音词也合理竞争第一）。
    engine.ParseAndGuess("beijing");
    bool hit = false;
    for (const auto& c : engine.candidates()) {
        if (c.text.find("北京") != std::string::npos) {
            hit = true;
            break;
        }
    }
    if (!hit) {
        std::fprintf(stderr, "FAIL: beijing -> 候选中未命中\"北京\"\n");
        ++g_failures;
    } else {
        std::printf("OK: beijing -> 候选命中 北京\n");
    }

    if (g_failures == 0) {
        std::puts("engine_conversion_test: all checks passed");
        return EXIT_SUCCESS;
    }
    std::fprintf(stderr, "engine_conversion_test: %d check(s) failed\n", g_failures);
    return EXIT_FAILURE;
}
