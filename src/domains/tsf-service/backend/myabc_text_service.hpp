// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/myabc_text_service.hpp --- TSF TIP 主对象
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//       docs/plan/03-m2-engine-process-ipc-plan.md §3.3（DLL 变薄：候选窗迁出）
//       docs/architecture/system-overview.md §7 不变量 1/3/4/5
//
// M2：完整 processKey 往返 + 预编辑显示（ITfComposition）+ Shift 中英切换；候选窗渲染
// 已搬到独立的 myabc-ui.exe（不再链 candidate-ui/gdi32），TIP 只在应用完 ITfComposition
// 后把光标矩形经 setCaretRect 告诉引擎，候选明细由引擎直接推给 myabc-ui（不经 TIP）。
// DECISION: 单一 CMyabcTextService 实例同一时刻只跟踪一个"当前有焦点的 context"的组字
// 状态（composition_/session_id_ 都是单值成员，不是按 context 建表）。多文档同时组字
// 会互相干扰；M0/M1/M2 的记事本单窗口验收场景不受影响，真正的按 context 隔离留后续。

#ifndef MYABC_TSF_TEXT_SERVICE_HPP
#define MYABC_TSF_TEXT_SERVICE_HPP

#include <msctf.h>
#include <windows.h>

#include <memory>

#include "click_bridge.hpp"
#include "composition.hpp"
#include "config_defaults.hpp"
#include "ipc_client.hpp"
#include "key_router.hpp"
#include "mode_manager.hpp"
#include "scheme_hotkey_detector.hpp"
#include "scheme_lang_bar_button.hpp"

namespace myabc::tsf {

class CMyabcTextService final : public ITfTextInputProcessorEx,
                                public ITfThreadMgrEventSink,
                                public ITfKeyEventSink,
                                public ITfCompositionSink {
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

    // ITfThreadMgrEventSink（M1 仍是占位——按 context 维护组字状态留 M2）
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

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ec, ITfComposition* composition) override;

private:
    ~CMyabcTextService();

    HRESULT InitSinks();
    void    UninitSinks();
    void    ConnectEngineAndHello();

    // 把引擎 Response.result 应用到文档（起/改/结束组字），composing=true 时顺带把
    // 光标矩形经 setCaretRect 告诉引擎（候选明细由引擎自己推给 myabc-ui，见类头 M2 说明）。
    void ApplyEngineResponse(ITfContext* context, const ipc::Response& resp);
    void HideAndResetComposition(ITfContext* context);

    // 鼠标点击候选（见 click_bridge.hpp / docs/decisions/tsf-service/
    // 20260912-mouse-candidate-select.md）：ClickBridge 只在正在组字期间存在，
    // 生命周期在 ApplyEngineResponse 的 composing 状态变化处管理。cached_context_
    // 缓存组字所在的 ITfContext——鼠标点击是异步事件，没有 OnKeyDown 那样现成的
    // pic 参数，必须自己存一份并正确 AddRef/Release。
    void SetCachedContext(ITfContext* context);
    void OnCandidateClicked(int index);

    // 2026-09-13（docs/decisions/input-engine/20260913-wubi-input-scheme.md）：
    // 热键（Ctrl+Shift+W 默认）与语言栏按钮点击共用同一个切换动作——本地循环到
    // 下一个方案（smartabc->pinyin->wubi->smartabc），乐观更新语言栏文字，再单向
    // 通知引擎（同 SetCaretRect 既有惯例：失败不阻塞，不特殊处理，下次 Hello 会
    // 用引擎侧的权威状态纠正本地缓存）。
    void TriggerSchemeSwitch();
    HRESULT InitLangBar();
    void UninitLangBar();

    LONG ref_ = 1;
    ITfThreadMgr* thread_mgr_ = nullptr;
    TfClientId tid_ = TF_CLIENTID_NULL;
    DWORD thread_mgr_cookie_ = TF_INVALID_COOKIE;

    // M1 固定值：单一活跃组合的 sessionId（见类头 DECISION）。
    static constexpr std::uint32_t kSessionId = 1;

    myabc::config::Config config_;
    KeyRouter key_router_;
    ModeManager mode_manager_;
    std::unique_ptr<IpcClient> ipc_;
    CompositionController composition_;
    ClickBridge click_bridge_;
    ITfContext* cached_context_ = nullptr;   // 手动 AddRef/Release，见 SetCachedContext

    // 2026-09-13：输入方案切换（见 docs/decisions/input-engine/20260913-wubi-input-scheme.md）。
    SchemeHotkeyDetector scheme_hotkey_;
    ITfLangBarItemMgr* lang_bar_mgr_ = nullptr;         // 手动 AddRef/Release
    SchemeLangBarButton* scheme_lang_bar_button_ = nullptr;   // 手动 AddRef/Release
    // 本地缓存的当前方案（"smartabc"/"pinyin"/"wubi"）——权威状态始终在引擎侧，
    // 这里只是为了计算"下一个方案"+驱动语言栏文字，Hello() 响应会纠正它。
    std::string current_scheme_method_ = "smartabc";
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_TEXT_SERVICE_HPP
