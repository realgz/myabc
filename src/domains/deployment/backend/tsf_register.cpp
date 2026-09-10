// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/backend/tsf_register.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4
//
// 参考微软 SampleIME（MIT）：Profile.cpp 的 RegisterProfiles / UnregisterProfiles，
// Register.cpp 的 RegisterCategories / UnregisterCategories（TIPCAP 集合）。
// DECISION: docs/decisions/_debt-log.md（SampleIME 片段来源登记）。

#include "tsf_register.hpp"

#include <msctf.h>
#include <olectl.h>

#include <array>
#include <cwchar>
#include <iterator>

#include <wil/com.h>
#include <wil/result.h>

#include "guids.hpp"

namespace myabc::deploy {

namespace {

std::wstring SelfDllPath() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring p(path);
    const auto slash = p.find_last_of(L"\\/");
    return (slash == std::wstring::npos ? L"." : p.substr(0, slash)) + L"\\myabc-tip.dll";
}

// Win8+ TIP 能力类别（plan 01 §3.4）：注册后系统才在现代宿主（UWP / 沙箱 / 无 UI 模式）
// 里启用本 TIP。DISPLAYATTRIBUTEPROVIDER 等 M0 未实现的能力不在此声明。
constexpr std::array<const GUID*, 6> kTipCaps = {
    &GUID_TFCAT_TIPCAP_SECUREMODE,
    &GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    &GUID_TFCAT_TIPCAP_COMLESS,
    &GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    &GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    &GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

}  // namespace

HRESULT RegisterProfiles(std::uint16_t langid, const std::wstring& display_name,
                         const std::wstring& icon_path) {
    wil::com_ptr<ITfInputProcessorProfiles> profiles;
    RETURN_IF_FAILED(::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&profiles)));

    RETURN_IF_FAILED(profiles->Register(CLSID_MyabcTextService));

    const std::wstring icon = icon_path.empty() ? SelfDllPath() : icon_path;
    const HRESULT hr = profiles->AddLanguageProfile(
        CLSID_MyabcTextService, static_cast<LANGID>(langid), GUID_MyabcProfile,
        display_name.c_str(), static_cast<ULONG>(display_name.size()), icon.c_str(),
        static_cast<ULONG>(icon.size()), /*uIconIndex=*/0);
    return hr;
}

HRESULT UnregisterProfiles(std::uint16_t langid) {
    wil::com_ptr<ITfInputProcessorProfiles> profiles;
    RETURN_IF_FAILED(::CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&profiles)));
    // 先删语言 profile，再注销整个 TIP。
    profiles->RemoveLanguageProfile(CLSID_MyabcTextService, static_cast<LANGID>(langid),
                                    GUID_MyabcProfile);
    return profiles->Unregister(CLSID_MyabcTextService);
}

HRESULT RegisterCategories() {
    wil::com_ptr<ITfCategoryMgr> cat;
    RETURN_IF_FAILED(::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&cat)));

    RETURN_IF_FAILED(cat->RegisterCategory(CLSID_MyabcTextService, GUID_TFCAT_TIP_KEYBOARD,
                                           CLSID_MyabcTextService));
    for (const GUID* g : kTipCaps) {
        RETURN_IF_FAILED(cat->RegisterCategory(CLSID_MyabcTextService, *g, CLSID_MyabcTextService));
    }
    return S_OK;
}

HRESULT UnregisterCategories() {
    wil::com_ptr<ITfCategoryMgr> cat;
    RETURN_IF_FAILED(::CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS(&cat)));
    cat->UnregisterCategory(CLSID_MyabcTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_MyabcTextService);
    for (const GUID* g : kTipCaps) {
        cat->UnregisterCategory(CLSID_MyabcTextService, *g, CLSID_MyabcTextService);
    }
    return S_OK;
}

bool IsProfileRegistered(std::uint16_t langid) {
    // 权威来源是注册表：TSF 把 profile 写在
    //   HKLM\SOFTWARE\Microsoft\CTF\TIP\{clsid}\LanguageProfile\0x{langid:08x}\{profileGuid}
    // （ITfInputProcessorProfiles::EnumLanguageProfiles 对"刚注册"的 profile 有进程内缓存，
    //  不可靠——见 docs/decisions/_debt-log.md 2026-09-11）。
    wchar_t clsid_s[64] = {};
    wchar_t prof_s[64] = {};
    ::StringFromGUID2(CLSID_MyabcTextService, clsid_s, static_cast<int>(std::size(clsid_s)));
    ::StringFromGUID2(GUID_MyabcProfile, prof_s, static_cast<int>(std::size(prof_s)));

    wchar_t sub[256] = {};
    ::swprintf(sub, std::size(sub),
               L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x%08x\\%s", clsid_s,
               static_cast<unsigned>(langid), prof_s);

    HKEY key = nullptr;
    // 64 位进程读 64 位视图即可；TSF API 注册时已同时写 WOW6432Node。
    if (::RegOpenKeyExW(HKEY_LOCAL_MACHINE, sub, 0, KEY_READ | KEY_WOW64_64KEY, &key) !=
        ERROR_SUCCESS) {
        return false;
    }
    const LSTATUS have_desc = ::RegQueryValueExW(key, L"Description", nullptr, nullptr, nullptr, nullptr);
    ::RegCloseKey(key);
    return have_desc == ERROR_SUCCESS;
}

}  // namespace myabc::deploy
