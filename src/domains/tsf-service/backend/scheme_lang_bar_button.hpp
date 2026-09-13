// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/scheme_lang_bar_button.hpp --- 输入方案语言栏状态按钮
//
// 依据：docs/plan/08-wubi-input-scheme-plan.md §9 决策点 2（用户拍板：v1 需要
//       可见状态提示，覆盖计划默认推荐的"纯热键、无可见指示"）
//       docs/decisions/input-engine/20260913-wubi-input-scheme.md
//
// 0% 起点的新 COM 实现（`ITfLangBarItemButton`，此前项目里完全没有任何
// `ITfLangBarItem*` 实现）。职责单一：在语言栏里显示当前输入方案的文字
// （"智能ABC"/"普通拼音"/"五笔"），点击时触发一次方案切换（跟热键走同一个
// 回调，语义完全一致）。不做菜单/多级交互，符合 v1 最小可见状态指示的要求。

#ifndef MYABC_TSF_SCHEME_LANG_BAR_BUTTON_HPP
#define MYABC_TSF_SCHEME_LANG_BAR_BUTTON_HPP

#include <msctf.h>
#include <windows.h>

#include <functional>
#include <string>

namespace myabc::tsf {

class SchemeLangBarButton final : public ITfLangBarItemButton {
public:
    // on_click：用户左键点击按钮时调用（调用方负责真正发起方案切换，跟热键回调
    // 复用同一份逻辑）。构造后引用计数为 1（调用方持有这一份，直到
    // ITfLangBarItemMgr::RemoveItem 之后再 Release）。
    explicit SchemeLangBarButton(std::function<void()> on_click);

    // 更新显示文字（引擎响应/热键或点击触发切换后调用）。语言栏宿主会在下次
    // 重绘时读取新文字（ITfLangBarItemMgr 的既有刷新机制，不需要手动强制重绘）。
    void SetDisplayText(std::wstring text);

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo) override;
    STDMETHODIMP GetStatus(DWORD* pdwStatus) override;
    STDMETHODIMP Show(BOOL fShow) override;
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip) override;

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) override;
    STDMETHODIMP InitMenu(ITfMenu* pMenu) override;
    STDMETHODIMP OnMenuSelect(UINT wID) override;
    STDMETHODIMP GetIcon(HICON* phIcon) override;
    STDMETHODIMP GetText(BSTR* pbstrText) override;

private:
    ~SchemeLangBarButton();

    LONG ref_ = 1;
    std::wstring text_ = L"智能ABC";
    std::function<void()> on_click_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_SCHEME_LANG_BAR_BUTTON_HPP
