// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/dll_main.cpp --- DLL 入口 + COM 导出
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//       docs/architecture/system-overview.md §7 不变量 1（不链 libpinyin/glib/db）、4
//
// 导出（tsf-service.def）：DllGetClassObject / DllCanUnloadNow /
//                          DllRegisterServer / DllUnregisterServer

#include <windows.h>

#include <cstdint>
#include <iterator>

#include "class_factory.hpp"
#include "config_defaults.hpp"
#include "dll_refcount.hpp"
#include "guids.hpp"

// deployment 域的注册实现被 TIP DLL 复用（同一套 COM/TSF 注册逻辑）。
#include "com_register.hpp"
#include "tsf_register.hpp"

using namespace myabc::tsf;

namespace {
constexpr std::uint16_t kLangIdZhCN = myabc::config::kDefaultLangId;
constexpr wchar_t kDisplayName[] = L"智能ABC (myabc)";
}  // namespace

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID /*reserved*/) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            SetDllInstanceHandle(hinst);
            ::DisableThreadLibraryCalls(hinst);
            break;
        default:
            break;
    }
    return TRUE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;
    if (!::IsEqualCLSID(rclsid, CLSID_MyabcTextService)) return CLASS_E_CLASSNOTAVAILABLE;
    return CClassFactory::Instance().QueryInterface(riid, ppv);
}

STDAPI DllCanUnloadNow() { return DllCanUnload() ? S_OK : S_FALSE; }

STDAPI DllRegisterServer() {
    // InprocServer32 指向本 DLL 自身。
    wchar_t self[MAX_PATH] = {};
    ::GetModuleFileNameW(static_cast<HMODULE>(DllInstanceHandle()), self,
                         static_cast<DWORD>(std::size(self)));

    HRESULT hr = myabc::deploy::RegisterComServer(self);
    if (SUCCEEDED(hr)) hr = myabc::deploy::RegisterProfiles(kLangIdZhCN, kDisplayName, self);
    if (SUCCEEDED(hr)) hr = myabc::deploy::RegisterCategories();
    if (FAILED(hr)) {
        myabc::deploy::UnregisterCategories();
        myabc::deploy::UnregisterProfiles(kLangIdZhCN);
        myabc::deploy::UnregisterComServer();
    }
    return hr;
}

STDAPI DllUnregisterServer() {
    myabc::deploy::UnregisterCategories();
    myabc::deploy::UnregisterProfiles(kLangIdZhCN);
    myabc::deploy::UnregisterComServer();
    return S_OK;
}
