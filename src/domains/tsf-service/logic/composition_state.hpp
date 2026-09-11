// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/logic/composition_state.hpp --- 组字显示状态（纯数据）
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//
// 只负责"引擎 IPC 结果 -> 该怎么显示"的判断，不碰任何 COM/TSF 接口——真正去改文档、
// 显示候选窗的操作在 backend/composition.cpp、backend/candidate_window 里。

#ifndef MYABC_TSF_COMPOSITION_STATE_HPP
#define MYABC_TSF_COMPOSITION_STATE_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "json_codec.hpp"

namespace myabc::tsf {

struct CandidateLine {
    std::wstring text;
};

// 从 processKey/selectCandidate/pageCandidates 等的 Response.result 解出的展示态。
struct CompositionState {
    bool composing = false;
    std::wstring preedit;
    std::vector<CandidateLine> candidates;   // 当前页
    int page_index = 0;
    int page_size = 0;
    int page_total = 0;
    bool has_commit = false;
    std::wstring commit_text;

    // 用引擎响应的 result 对象刷新本状态（result 形状见 system-overview §4.1）。
    // 解析失败（缺字段）时保守地把 composing 置 false，避免带着脏状态继续走。
    void ApplyResult(const ipc::Json& result);

    void Reset();
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_COMPOSITION_STATE_HPP
