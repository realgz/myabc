// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/edit_session.hpp --- 一次性 ITfEditSession：插入文本
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3（edit_session）
//       docs/architecture/system-overview.md §7 不变量 5（文档改动只在 EditSession 内）

#ifndef MYABC_TSF_EDIT_SESSION_HPP
#define MYABC_TSF_EDIT_SESSION_HPP

#include <msctf.h>
#include <windows.h>

#include <string>

namespace myabc::tsf {

// 用法：new CInsertTextEditSession(context, tid, L"啊")，交给
// context->RequestEditSession(tid, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr)。
class CInsertTextEditSession : public ITfEditSession {
public:
    CInsertTextEditSession(ITfContext* context, TfClientId tid, std::wstring text);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec) override;

private:
    ~CInsertTextEditSession();

    LONG ref_ = 1;
    ITfContext* context_ = nullptr;
    TfClientId tid_ = 0;
    std::wstring text_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_EDIT_SESSION_HPP
