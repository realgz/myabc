// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/mode_manager.hpp --- 中/英文模式（Shift 单击切换）
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5（M1-9）
//
// 纯本地状态机，不经 IPC——英文模式下字母键由 TIP 直接放行，引擎完全不知情
// （见 docs/decisions/_debt-log.md 2026-09-11 "M1 englishMode 由 TIP 本地处理"）。
//
// 判定规则：记录"从 Shift 按下到弹起之间，是否还有其它键插入"；没有插入即视为
// 一次单击，弹起时切换模式。这是 Windows 上通用 IME 的标准手势，实现于
// key_event_sink 的 OnKeyDown/OnKeyUp（而非 OnTestKeyDown/OnTestKeyUp——
// 判定需要跨两次事件的状态，测试阶段不适合下结论）。

#ifndef MYABC_TSF_MODE_MANAGER_HPP
#define MYABC_TSF_MODE_MANAGER_HPP

namespace myabc::tsf {

enum class InputMode { kChinese, kEnglish };

class ModeManager {
public:
    InputMode mode() const noexcept { return mode_; }

    // 任意键按下时调用一次（含 Shift 自身）。
    void OnKeyDown(int vk);

    // 任意键弹起时调用一次。返回 true 表示"这是一次 Shift 单击，模式已切换"。
    bool OnKeyUp(int vk);

private:
    bool shift_only_ = false;
    InputMode mode_ = InputMode::kChinese;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_MODE_MANAGER_HPP
