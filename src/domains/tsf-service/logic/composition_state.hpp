// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/composition_state.hpp --- 组字显示状态（纯数据）
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//       docs/plan/03-m2-engine-process-ipc-plan.md §3.2（v2：候选明细不再经这条通道）
//
// 只负责"引擎 IPC 结果 -> 该怎么显示预编辑文字"的判断，不碰任何 COM/TSF 接口。
// v2 起候选列表本身不在这个结果里——那是 myabc-ui 直接从引擎的 uiShow 收的，
// TIP 只关心 preedit（要写进 ITfComposition 的内联文字）和 composing/commit。

#ifndef MYABC_TSF_COMPOSITION_STATE_HPP
#define MYABC_TSF_COMPOSITION_STATE_HPP

#include <string>

#include "json_codec.hpp"

namespace myabc::tsf {

// 从 processKey/selectCandidate/pageCandidates/commitComposition/cancelComposition 的
// Response.result 解出的展示态（形状见 system-overview §4.1，v2）。
struct CompositionState {
    bool composing = false;
    std::wstring preedit;
    bool has_commit = false;
    std::wstring commit_text;

    void ApplyResult(const ipc::Json& result);
    void Reset();
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_COMPOSITION_STATE_HPP
