// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/composition.hpp --- ITfComposition 管理
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//       docs/architecture/system-overview.md §7 不变量 5（文档改动只在 EditSession 内）
//
// DECISION: ITfCompositionSink 由 CMyabcTextService 自己实现（沿用 M0 起"一个对象多接口"
// 的模式，见 myabc_text_service.hpp 的多重继承），本类只是它的一个纯逻辑帮手，不自己
// 实现 IUnknown——避免为一个生命周期完全跟随宿主对象的成员再造一份引用计数。
//
// 持有当前 ITfComposition（每个 CMyabcTextService 实例同一时刻只服务一个有焦点的
// context，见 docs/decisions/_debt-log.md 2026-09-11 的单一状态简化）。
// 所有操作（起组字/改预编辑文本/结束）各自起一次 ITfEditSession，满足不变量 5。

#ifndef MYABC_TSF_COMPOSITION_HPP
#define MYABC_TSF_COMPOSITION_HPP

#include <msctf.h>
#include <windows.h>

#include <string>

#include <wil/com.h>

namespace myabc::tsf {

class CompositionController {
public:
    bool active() const noexcept { return static_cast<bool>(composition_); }

    // sink：调用方（CMyabcTextService）的 this，需实现 ITfCompositionSink。
    // 已有 composition 时直接改预编辑文本；没有则新起一个。out_caret_rect 收到光标处的
    // 屏幕坐标矩形（ITfContextView::GetTextExt），候选窗据此定位（M1-11）；失败时不修改。
    HRESULT StartOrUpdate(ITfContext* context, TfClientId tid, ITfCompositionSink* sink,
                         const std::wstring& preedit_text, RECT* out_caret_rect);

    // 把 composition 的文本替换成 final_text 后结束组字（真正的"上屏"）。
    HRESULT EndWithText(ITfContext* context, TfClientId tid, const std::wstring& final_text);

    // 结束组字且不留任何文本（ESC 取消 / 引擎超时降级）。
    HRESULT Cancel(ITfContext* context, TfClientId tid);

    // ITfCompositionSink::OnCompositionTerminated 应转发到这里，清本地指针。
    void OnExternallyTerminated() { composition_.reset(); }

    // 供 composition.cpp 里的一次性 EditSession 实现用；不对外（不在 .cpp 之外调用）。
    ITfComposition* raw_composition() const noexcept { return composition_.get(); }
    void set_composition(wil::com_ptr_nothrow<ITfComposition> c) { composition_ = std::move(c); }
    void clear_composition() { composition_.reset(); }

private:
    wil::com_ptr_nothrow<ITfComposition> composition_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_COMPOSITION_HPP
