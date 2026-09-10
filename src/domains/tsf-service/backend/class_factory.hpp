// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/class_factory.hpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3

#ifndef MYABC_TSF_CLASS_FACTORY_HPP
#define MYABC_TSF_CLASS_FACTORY_HPP

#include <windows.h>
#include <unknwn.h>

namespace myabc::tsf {

class CClassFactory final : public IClassFactory {
public:
    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override;
    STDMETHODIMP LockServer(BOOL fLock) override;

    // 进程内单例（无状态）。
    static CClassFactory& Instance() noexcept;

private:
    LONG ref_ = 1;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_CLASS_FACTORY_HPP
