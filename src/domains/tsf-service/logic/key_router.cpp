// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/key_router.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3

#include "key_router.hpp"

namespace myabc::tsf {

bool KeyRouter::IsInterestedKey(int vk, CompositionState state) const noexcept {
    // M0：与组字态无关，仅吃 'A'。
    (void)state;
    return vk == 'A';
}

}  // namespace myabc::tsf
