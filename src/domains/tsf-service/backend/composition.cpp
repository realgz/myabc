// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/composition.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//
// 参考微软 TSF 官方样例（SampleIME/TableTextService，MIT）EditSession.cpp 里
// StartComposition/EndComposition 的标准调用序列（InsertTextAtSelection(QUERYONLY) 取
// range -> ITfContextComposition::StartComposition -> range->SetText）。
// DECISION: docs/decisions/_debt-log.md（来源说明，与 M0 R3 的 TSF/COM 代码来源登记同类）。

#include "composition.hpp"

#include <wil/result.h>

namespace myabc::tsf {

namespace {

// 把 range 收拢到结尾并设为唯一 selection，光标落在刚写入文本之后。
void CollapseSelectionToEnd(ITfContext* context, TfEditCookie ec, ITfRange* range) {
    range->Collapse(ec, TF_ANCHOR_END);
    TF_SELECTION sel{};
    sel.range = range;
    sel.style.ase = TF_AE_NONE;
    sel.style.fInterimChar = FALSE;
    context->SetSelection(ec, 1, &sel);
}

// 单个一次性 EditSession：按 Action 做 起/改/结束 三选一。
class CCompositionEditSession final : public ITfEditSession {
public:
    enum class Action { kStartOrUpdate, kEndWithText, kCancel };

    CCompositionEditSession(CompositionController& owner, ITfContext* context, TfClientId tid,
                           ITfCompositionSink* sink, Action action, std::wstring text,
                           RECT* out_caret_rect = nullptr)
        : owner_(owner), context_(context), tid_(tid), sink_(sink), action_(action),
          text_(std::move(text)), out_caret_rect_(out_caret_rect) {
        context_->AddRef();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (ppv == nullptr) return E_INVALIDARG;
        if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_ITfEditSession)) {
            *ppv = static_cast<ITfEditSession*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ::InterlockedIncrement(&ref_); }
    STDMETHODIMP_(ULONG) Release() override {
        const LONG c = ::InterlockedDecrement(&ref_);
        if (c == 0) delete this;
        return c;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        switch (action_) {
            case Action::kStartOrUpdate: return DoStartOrUpdate(ec);
            case Action::kEndWithText: return DoEnd(ec, /*commit=*/true);
            case Action::kCancel: return DoEnd(ec, /*commit=*/false);
        }
        return E_UNEXPECTED;
    }

private:
    ~CCompositionEditSession() { context_->Release(); }

    HRESULT DoStartOrUpdate(TfEditCookie ec) {
        wil::com_ptr_nothrow<ITfRange> range;

        if (ITfComposition* existing = owner_.raw_composition()) {
            RETURN_IF_FAILED(existing->GetRange(&range));
        } else {
            wil::com_ptr_nothrow<ITfInsertAtSelection> insert;
            RETURN_IF_FAILED(context_->QueryInterface(IID_PPV_ARGS(&insert)));
            RETURN_IF_FAILED(insert->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, nullptr, 0, &range));

            wil::com_ptr_nothrow<ITfContextComposition> cc;
            RETURN_IF_FAILED(context_->QueryInterface(IID_PPV_ARGS(&cc)));
            wil::com_ptr_nothrow<ITfComposition> created;
            RETURN_IF_FAILED(cc->StartComposition(ec, range.get(), sink_, &created));
            owner_.set_composition(std::move(created));
        }

        RETURN_IF_FAILED(range->SetText(ec, 0, text_.c_str(), static_cast<LONG>(text_.size())));
        CollapseSelectionToEnd(context_, ec, range.get());

        if (out_caret_rect_ != nullptr) {
            wil::com_ptr_nothrow<ITfContextView> view;
            if (SUCCEEDED(context_->GetActiveView(&view)) && view) {
                BOOL clipped = FALSE;
                view->GetTextExt(ec, range.get(), out_caret_rect_, &clipped);
            }
        }
        return S_OK;
    }

    HRESULT DoEnd(TfEditCookie ec, bool commit) {
        ITfComposition* existing = owner_.raw_composition();
        if (existing == nullptr) return S_OK;   // 已经结束/从未开始，幂等

        wil::com_ptr_nothrow<ITfRange> range;
        RETURN_IF_FAILED(existing->GetRange(&range));
        const std::wstring final_text = commit ? text_ : L"";
        RETURN_IF_FAILED(
            range->SetText(ec, 0, final_text.c_str(), static_cast<LONG>(final_text.size())));
        CollapseSelectionToEnd(context_, ec, range.get());

        const HRESULT hr = existing->EndComposition(ec);
        owner_.clear_composition();
        return hr;
    }

    LONG ref_ = 1;
    CompositionController& owner_;
    ITfContext* context_;
    TfClientId tid_;
    ITfCompositionSink* sink_;
    Action action_;
    std::wstring text_;
    RECT* out_caret_rect_;
};

HRESULT RunEditSession(ITfContext* context, TfClientId tid, CCompositionEditSession::Action action,
                       CompositionController& owner, ITfCompositionSink* sink, std::wstring text,
                       RECT* out_caret_rect = nullptr) {
    auto* session = new (std::nothrow)
        CCompositionEditSession(owner, context, tid, sink, action, std::move(text), out_caret_rect);
    if (session == nullptr) return E_OUTOFMEMORY;

    HRESULT hr_session = E_FAIL;
    const HRESULT hr =
        context->RequestEditSession(tid, session, TF_ES_SYNC | TF_ES_READWRITE, &hr_session);
    session->Release();
    return FAILED(hr) ? hr : hr_session;
}

}  // namespace

HRESULT CompositionController::StartOrUpdate(ITfContext* context, TfClientId tid,
                                             ITfCompositionSink* sink,
                                             const std::wstring& preedit_text,
                                             RECT* out_caret_rect) {
    return RunEditSession(context, tid, CCompositionEditSession::Action::kStartOrUpdate, *this,
                          sink, preedit_text, out_caret_rect);
}

HRESULT CompositionController::EndWithText(ITfContext* context, TfClientId tid,
                                           const std::wstring& final_text) {
    return RunEditSession(context, tid, CCompositionEditSession::Action::kEndWithText, *this,
                          nullptr, final_text);
}

HRESULT CompositionController::Cancel(ITfContext* context, TfClientId tid) {
    return RunEditSession(context, tid, CCompositionEditSession::Action::kCancel, *this, nullptr,
                          L"");
}

}  // namespace myabc::tsf
