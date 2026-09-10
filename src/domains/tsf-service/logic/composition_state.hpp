// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/composition_state.hpp
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（composition_state）
//
// M0 只有 Idle。预留 Feed(key) 接口给 M1 的组字状态机。

#ifndef MYABC_TSF_COMPOSITION_STATE_HPP
#define MYABC_TSF_COMPOSITION_STATE_HPP

namespace myabc::tsf {

enum class CompositionState {
    Idle = 0,
    Composing,   // M1 起使用
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_COMPOSITION_STATE_HPP
