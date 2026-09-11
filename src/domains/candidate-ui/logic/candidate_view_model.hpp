// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/candidate-ui/logic/candidate_view_model.hpp --- 候选窗纯数据模型
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.4
//
// 由 TIP 从 IPC result 填充（见 tsf-service/logic/composition_state.hpp），本文件
// 不依赖任何 COM/Win32 类型，方便未来（M2）整体搬到独立 UI 进程时原样带走。

#ifndef MYABC_CANDIDATE_UI_VIEW_MODEL_HPP
#define MYABC_CANDIDATE_UI_VIEW_MODEL_HPP

#include <string>
#include <vector>

namespace myabc::ui {

struct CandidateViewModel {
    std::wstring preedit;
    std::vector<std::wstring> items;   // 当前页，序号 1..N 对应选字键
    int page_index = 0;
    int page_total = 0;
    // 智能ABC 风格空格两段式确认（protocol v6 armedIndex）：-1 = 无高亮；
    // >=0 = items 里这个下标被"架住"，还没真正选中，渲染层应画出高亮但不当作已上屏。
    int armed_index = -1;
};

}  // namespace myabc::ui

#endif  // MYABC_CANDIDATE_UI_VIEW_MODEL_HPP
