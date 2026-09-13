// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/scheme_lang_bar_button.cpp

#include "scheme_lang_bar_button.hpp"

#include <oleauto.h>

#include "dll_refcount.hpp"
#include "guids.hpp"

namespace myabc::tsf {

SchemeLangBarButton::SchemeLangBarButton(std::function<void()> on_click)
    : on_click_(std::move(on_click)) {
    DllAddRef();
}

SchemeLangBarButton::~SchemeLangBarButton() { DllRelease(); }

void SchemeLangBarButton::SetDisplayText(std::wstring text) { text_ = std::move(text); }

// ---- IUnknown -------------------------------------------------------------
STDMETHODIMP SchemeLangBarButton::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;
    if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_ITfLangBarItem) ||
        ::IsEqualIID(riid, IID_ITfLangBarItemButton)) {
        *ppv = static_cast<ITfLangBarItemButton*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) SchemeLangBarButton::AddRef() { return ::InterlockedIncrement(&ref_); }

STDMETHODIMP_(ULONG) SchemeLangBarButton::Release() {
    const LONG c = ::InterlockedDecrement(&ref_);
    if (c == 0) delete this;
    return c;
}

// ---- ITfLangBarItem ---------------------------------------------------
STDMETHODIMP SchemeLangBarButton::GetInfo(TF_LANGBARITEMINFO* pInfo) {
    if (pInfo == nullptr) return E_INVALIDARG;
    pInfo->clsidService = CLSID_MyabcTextService;
    pInfo->guidItem = GUID_MyabcSchemeLangBar;
    // TF_LBI_STYLE_BTN_BUTTON：普通按钮（文字+可选图标，左键点击产生 OnClick）。
    // TF_LBI_STYLE_SHOWNINTRAY：允许宿主把它放进系统托盘的语言栏收纳区（现代
    // Windows 版本对语言栏项的常见摆放方式）。
    pInfo->dwStyle = TF_LBI_STYLE_BTN_BUTTON | TF_LBI_STYLE_SHOWNINTRAY;
    pInfo->ulSort = 0;
    ::wcsncpy_s(pInfo->szDescription, L"myabc 输入方案", _TRUNCATE);
    return S_OK;
}

STDMETHODIMP SchemeLangBarButton::GetStatus(DWORD* pdwStatus) {
    if (pdwStatus == nullptr) return E_INVALIDARG;
    *pdwStatus = 0;   // 始终启用/可见，不做禁用态
    return S_OK;
}

STDMETHODIMP SchemeLangBarButton::Show(BOOL /*fShow*/) { return S_OK; }

STDMETHODIMP SchemeLangBarButton::GetTooltipString(BSTR* pbstrToolTip) {
    if (pbstrToolTip == nullptr) return E_INVALIDARG;
    *pbstrToolTip = ::SysAllocString(L"myabc 当前输入方案（点击切换，或按 Ctrl+Shift+W）");
    return *pbstrToolTip != nullptr ? S_OK : E_OUTOFMEMORY;
}

// ---- ITfLangBarItemButton -----------------------------------------------
STDMETHODIMP SchemeLangBarButton::OnClick(TfLBIClick click, POINT /*pt*/, const RECT* /*prcArea*/) {
    if (click == TF_LBI_CLK_LEFT && on_click_) on_click_();
    return S_OK;
}

STDMETHODIMP SchemeLangBarButton::InitMenu(ITfMenu* /*pMenu*/) {
    return E_NOTIMPL;   // v1 不做右键菜单，左键点击循环切换已足够
}

STDMETHODIMP SchemeLangBarButton::OnMenuSelect(UINT /*wID*/) { return E_NOTIMPL; }

STDMETHODIMP SchemeLangBarButton::GetIcon(HICON* phIcon) {
    if (phIcon == nullptr) return E_INVALIDARG;
    // 复用 tsf-service.rc 里已有的 IDI_MYABC（资源 ID 1，见该 .rc 文件注释），
    // 跟 AddLanguageProfile 的图标是同一份资源，不新增图标资产。
    *phIcon = static_cast<HICON>(
        ::LoadImageW(static_cast<HMODULE>(DllInstanceHandle()), MAKEINTRESOURCEW(1), IMAGE_ICON,
                    16, 16, LR_DEFAULTCOLOR));
    return *phIcon != nullptr ? S_OK : E_FAIL;
}

STDMETHODIMP SchemeLangBarButton::GetText(BSTR* pbstrText) {
    if (pbstrText == nullptr) return E_INVALIDARG;
    *pbstrText = ::SysAllocString(text_.c_str());
    return *pbstrText != nullptr ? S_OK : E_OUTOFMEMORY;
}

}  // namespace myabc::tsf
