// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/key_router.hpp --- 本地按键判定（不依赖引擎往返）
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（key_router）
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//       docs/architecture/system-overview.md §7 不变量 3（UI 线程不做无界等待）
//
// 纯逻辑：OnTestKeyDown 必须本地同步答复要不要吃键，真正的组字状态机在引擎侧
// （session.cpp）——本类只做"值不值得问引擎"的快速判断，不持有状态。

#ifndef MYABC_TSF_KEY_ROUTER_HPP
#define MYABC_TSF_KEY_ROUTER_HPP

#include "config_defaults.hpp"
#include "mode_manager.hpp"

namespace myabc::tsf {

class KeyRouter {
public:
    explicit KeyRouter(const myabc::config::CandidatesConfig& candidates_cfg)
        : candidates_cfg_(candidates_cfg) {}

    // mode==English：从不吃键（M1-9，直接放行，引擎完全不参与）。
    // mode==Chinese 且未组字：吃字母 a-z/A-Z，或有标点映射的字符（起组字/直接上屏标点）。
    // mode==Chinese 且组字中：吃字母、选字键、翻页键、空格、Backspace、Escape。
    bool IsInterestedKey(int vk, wchar_t ch, bool composing, InputMode mode) const noexcept;

    // 与 input-engine/logic/candidate/punctuation_source.cpp 的映射表保持同一套 ASCII 字符
    // 集（不含全角映射本身——TIP 不需要知道映射到什么，只需要知道"这个字符引擎认识"）。
    // 两处各自维护是跨工具链边界的必然代价（不变量 6：不能共享业务代码），改动需同步两边。
    static bool IsMappedPunctuationAscii(wchar_t ch) noexcept;

private:
    const myabc::config::CandidatesConfig& candidates_cfg_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_KEY_ROUTER_HPP
