// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/edit_session.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//
// 参考微软 SampleIME（MIT）EditSession.cpp / 各 *EditSession 的 SetText 模式。
// DECISION: docs/decisions/_debt-log.md（来源登记）。

#include "edit_session.hpp"

#include <wil/com.h>

#include "dll_refcount.hpp"

namespace myabc::tsf {

CInsertTextEditSession::CInsertTextEditSession(ITfContext* context, TfClientId tid, std::wstring text)
    : context_(context), tid_(tid), text_(std::move(text)) {
    if (context_) context_->AddRef();
    DllAddRef();
}

CInsertTextEditSession::~CInsertTextEditSession() {
    if (context_) context_->Release();
    DllRelease();
}

STDMETHODIMP CInsertTextEditSession::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;
    if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_ITfEditSession)) {
        *ppv = static_cast<ITfEditSession*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CInsertTextEditSession::AddRef() { return ::InterlockedIncrement(&ref_); }

STDMETHODIMP_(ULONG) CInsertTextEditSession::Release() {
    const LONG c = ::InterlockedDecrement(&ref_);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP CInsertTextEditSession::DoEditSession(TfEditCookie ec) {
    if (context_ == nullptr) return E_UNEXPECTED;

    wil::com_ptr_nothrow<ITfInsertAtSelection> insert;
    HRESULT hr = context_->QueryInterface(IID_PPV_ARGS(&insert));
    if (FAILED(hr)) return hr;

    wil::com_ptr_nothrow<ITfRange> range;
    hr = insert->InsertTextAtSelection(ec, 0, text_.c_str(), static_cast<LONG>(text_.size()), &range);
    if (FAILED(hr)) return hr;

    // 收拢 selection 到插入文本尾部。
    TF_SELECTION sel{};
    sel.range = range.get();
    sel.style.ase = TF_AE_NONE;
    sel.style.fInterimChar = FALSE;
    range->Collapse(ec, TF_ANCHOR_END);
    context_->SetSelection(ec, 1, &sel);
    return S_OK;
}

}  // namespace myabc::tsf
