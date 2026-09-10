// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/class_factory.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//
// 参考微软 SampleIME（MIT）SampleIMEBaseStructure / Globals 的类工厂实现。
// DECISION: docs/decisions/_debt-log.md（来源登记）。

#include "class_factory.hpp"

#include <new>

#include "dll_refcount.hpp"
#include "myabc_text_service.hpp"

namespace myabc::tsf {

CClassFactory& CClassFactory::Instance() noexcept {
    static CClassFactory s_instance;
    return s_instance;
}

STDMETHODIMP CClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_IClassFactory)) {
        *ppv = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    *ppv = nullptr;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CClassFactory::AddRef() { return ::InterlockedIncrement(&ref_); }

STDMETHODIMP_(ULONG) CClassFactory::Release() {
    // 进程内静态单例，不真正销毁；仅维持计数供诊断。
    return ::InterlockedDecrement(&ref_);
}

STDMETHODIMP CClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;
    if (outer != nullptr) return CLASS_E_NOAGGREGATION;

    auto* svc = new (std::nothrow) CMyabcTextService();
    if (svc == nullptr) return E_OUTOFMEMORY;

    const HRESULT hr = svc->QueryInterface(riid, ppv);
    svc->Release();
    return hr;
}

STDMETHODIMP CClassFactory::LockServer(BOOL fLock) {
    if (fLock) {
        DllLockServer();
    } else {
        DllUnlockServer();
    }
    return S_OK;
}

}  // namespace myabc::tsf
