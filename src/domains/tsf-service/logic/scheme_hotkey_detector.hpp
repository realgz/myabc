// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/scheme_hotkey_detector.hpp --- 输入方案切换热键检测
//
// 依据：docs/plan/08-wubi-input-scheme-plan.md §5.13
//       docs/decisions/input-engine/20260913-wubi-input-scheme.md
//
// 结构完全仿照 mode_manager.hpp（纯本地状态机，不经 IPC 判定是否命中手势）。跟
// ModeManager 的 Shift 单击检测不同，这里检测的是"修饰键 + 字母键"组合（默认
// Ctrl+Shift+W），命中后由调用方（myabc_text_service.cpp）经 IPC 通知引擎真正
// 切换方案——手势识别本身仍是纯本地判断，符合不变量 3。

#ifndef MYABC_TSF_SCHEME_HOTKEY_DETECTOR_HPP
#define MYABC_TSF_SCHEME_HOTKEY_DETECTOR_HPP

#include <string>

namespace myabc::tsf {

// 解析 "ctrl+shift+<字母>" 格式的热键描述串，返回字母部分的 Win32 VK 码
// （大写 ASCII 字母的 VK 值恰好等于其 ASCII 码，如 'W' = 0x57）。解析失败
// （格式不对、字母缺失/不是单个字母）返回 fallback_vk（调用方传入配置默认值
// 对应的 VK，保证永远有一个可用的热键，不会因为配置字符串写错就整个失效）。
// 不在 src/config 里做这个解析（config_defaults.hpp 的既有惯例：本字段只是
// 反硬编码落点本身，解析逻辑留在实际使用它的领域）。
int ParseHotkeyLetterVk(const std::string& spec, int fallback_vk);

class SchemeHotkeyDetector {
public:
    explicit SchemeHotkeyDetector(int letter_vk) : letter_vk_(letter_vk) {}

    int letter_vk() const noexcept { return letter_vk_; }

    // 允许构造后重新设置（TIP 构造函数早于 config_ 真正 Load() 完成，见
    // myabc_text_service.cpp ActivateEx 里 config_ 加载完之后的重新设置调用）。
    void SetLetterVk(int letter_vk) noexcept { letter_vk_ = letter_vk; }

    // 任意键按下时调用。返回 true = 命中一次热键（Ctrl+Shift+<letter> 按下瞬间），
    // 调用方应据此触发一次方案切换。
    // DECISION：按住不放触发的 WM_KEYDOWN 自动重复不应该连续触发多次切换——
    // already_fired_ 去抖，必须先松开字母键（OnKeyUp）才能再次触发，跟真实
    // 键盘快捷键的常规体验一致（对比 Alt+Tab 按住不放不会连续切多个窗口）。
    bool OnKeyDown(int vk, bool ctrl, bool shift);

    // 任意键弹起时调用，清除去抖状态。
    void OnKeyUp(int vk);

private:
    int letter_vk_;
    bool already_fired_ = false;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_SCHEME_HOTKEY_DETECTOR_HPP
