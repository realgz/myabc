// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/key_router.hpp --- 本地按键判定（不依赖引擎往返）
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（key_router）
//       docs/architecture/system-overview.md §7 不变量 3（UI 线程不做无界等待）
//
// 纯逻辑：OnTestKeyDown 必须本地同步答复要不要吃键。M0 只关心 'A'。
// 组字态下的字母/数字/翻页键判定留到 M1（predeclare 接口）。

#ifndef MYABC_TSF_KEY_ROUTER_HPP
#define MYABC_TSF_KEY_ROUTER_HPP

#include "composition_state.hpp"

namespace myabc::tsf {

class KeyRouter {
public:
    // M0：仅 vk == 'A'（0x41）时吃键。后续按 state 扩展（组字态吃字母/数字/翻页/选字键）。
    bool IsInterestedKey(int vk, CompositionState state) const noexcept;

    // M0：'A' -> 上屏的固定字符串。M1 起改由引擎候选驱动。
    static constexpr const wchar_t* kM0CommitForA = L"啊";  // 啊
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_KEY_ROUTER_HPP
