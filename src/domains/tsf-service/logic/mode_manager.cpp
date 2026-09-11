// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/mode_manager.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5

#include "mode_manager.hpp"

#include <windows.h>

namespace myabc::tsf {

void ModeManager::OnKeyDown(int vk) {
    if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) {
        shift_only_ = true;
    } else {
        // 任何其它键插入 -> 这不是一次单击，是组合键的一部分。
        shift_only_ = false;
    }
}

bool ModeManager::OnKeyUp(int vk) {
    if (vk != VK_SHIFT && vk != VK_LSHIFT && vk != VK_RSHIFT) return false;

    const bool toggled = shift_only_;
    if (toggled) {
        mode_ = (mode_ == InputMode::kChinese) ? InputMode::kEnglish : InputMode::kChinese;
    }
    shift_only_ = false;
    return toggled;
}

}  // namespace myabc::tsf
