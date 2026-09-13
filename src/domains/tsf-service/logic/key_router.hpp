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
    // scheme_hotkey_vk：方案切换热键的字母部分 VK 码（如 'W'），0 = 不识别任何热键
    // （测试/未配置时的安全默认）。见 scheme_hotkey_detector.hpp。
    KeyRouter(const myabc::config::CandidatesConfig& candidates_cfg, int scheme_hotkey_vk = 0)
        : candidates_cfg_(candidates_cfg), scheme_hotkey_vk_(scheme_hotkey_vk) {}

    // 2026-09-13：Ctrl+Shift+<方案热键字母> 无论当前 mode/composing 状态如何都优先
    // 命中——否则宿主应用会在 TIP 之前吃掉这次按键，热键永远传不到 OnKeyDown
    // （见 docs/plan/08-wubi-input-scheme-plan.md §5.13）。
    // mode==English：从不吃键（M1-9，直接放行，引擎完全不参与）。
    // mode==Chinese 且未组字：吃字母 a-z/A-Z，或有标点映射的字符（起组字/直接上屏标点）。
    // mode==Chinese 且组字中：吃字母、选字键、翻页键、空格、Backspace、Escape。
    bool IsInterestedKey(int vk, wchar_t ch, bool composing, InputMode mode, bool ctrl,
                        bool shift) const noexcept;

    // 允许构造后重新设置（TIP 构造函数早于 config_ 真正 Load() 完成，见
    // myabc_text_service.cpp ActivateEx）。
    void SetSchemeHotkeyVk(int vk) noexcept { scheme_hotkey_vk_ = vk; }

    // 与 input-engine/logic/candidate/punctuation_source.cpp 的映射表保持同一套 ASCII 字符
    // 集（不含全角映射本身——TIP 不需要知道映射到什么，只需要知道"这个字符引擎认识"）。
    // 两处各自维护是跨工具链边界的必然代价（不变量 6：不能共享业务代码），改动需同步两边。
    static bool IsMappedPunctuationAscii(wchar_t ch) noexcept;

private:
    const myabc::config::CandidatesConfig& candidates_cfg_;
    int scheme_hotkey_vk_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_KEY_ROUTER_HPP
