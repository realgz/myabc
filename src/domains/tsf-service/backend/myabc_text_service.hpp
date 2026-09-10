// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/myabc_text_service.hpp --- TSF TIP 主对象
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//       docs/architecture/system-overview.md §7 不变量 1/3/4/5
//
// M0 实现：ITfTextInputProcessorEx（Activate/ActivateEx/Deactivate）、
//          ITfThreadMgrEventSink（占位）、ITfKeyEventSink（'A' -> 上屏"啊"）。

#ifndef MYABC_TSF_TEXT_SERVICE_HPP
#define MYABC_TSF_TEXT_SERVICE_HPP

#include <msctf.h>
#include <windows.h>

#include <memory>

#include "ipc_client.hpp"
#include "key_router.hpp"

namespace myabc::tsf {

class CMyabcTextService final : public ITfTextInputProcessorEx,
                                public ITfThreadMgrEventSink,
                                public ITfKeyEventSink {
public:
    CMyabcTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor / Ex
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
    STDMETHODIMP Deactivate() override;
    STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) override;

    // ITfThreadMgrEventSink（M0 全部返回 S_OK）
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) override;
    STDMETHODIMP OnPushContext(ITfContext*) override;
    STDMETHODIMP OnPopContext(ITfContext*) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) override;

private:
    ~CMyabcTextService();

    HRESULT InitSinks();
    void    UninitSinks();
    void    CommitText(ITfContext* context, const wchar_t* text);
    void    ConnectEngineAndHello();

    LONG ref_ = 1;
    ITfThreadMgr* thread_mgr_ = nullptr;
    TfClientId tid_ = TF_CLIENTID_NULL;
    DWORD thread_mgr_cookie_ = TF_INVALID_COOKIE;

    KeyRouter key_router_;
    std::unique_ptr<IpcClient> ipc_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_TEXT_SERVICE_HPP
