// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/myabc_text_service.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.5
//
// 参考微软 SampleIME（MIT）：SampleIME.cpp 的 Activate/Deactivate、KeyEventSink.cpp 的
// _InitKeyEventSink / OnKeyDown 结构。DECISION: docs/decisions/_debt-log.md（来源登记）。

#include "myabc_text_service.hpp"

#include <sddl.h>

#include <cwctype>
#include <string>

#include <wil/com.h>

#include "composition_state.hpp"
#include "config_loader.hpp"
#include "dll_refcount.hpp"
#include "edit_session.hpp"
#include "guids.hpp"
#include "text_convert.hpp"

namespace myabc::tsf {

namespace {

std::string CurrentUserSid() {
    wil::unique_handle token;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return "nosid";
    DWORD len = 0;
    ::GetTokenInformation(token.get(), TokenUser, nullptr, 0, &len);
    if (len == 0) return "nosid";
    std::string buf(len, '\0');
    if (!::GetTokenInformation(token.get(), TokenUser, buf.data(), len, &len)) return "nosid";
    const auto* tu = reinterpret_cast<const TOKEN_USER*>(buf.data());
    LPSTR s = nullptr;
    std::string sid = "nosid";
    if (::ConvertSidToStringSidA(tu->User.Sid, &s) && s) {
        sid = s;
        ::LocalFree(s);
    }
    return sid;
}

std::string AppDataDir() {
    wchar_t* p = nullptr;
    size_t n = 0;
    std::string out = ".";
    if (_wdupenv_s(&p, &n, L"APPDATA") == 0 && p) {
        out = Narrow(p);
        free(p);
    }
    return out;
}

std::wstring SelfDir() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(static_cast<HMODULE>(DllInstanceHandle()), path,
                         static_cast<DWORD>(std::size(path)));
    std::wstring p(path);
    const auto slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

// vk/lParam -> 键盘布局下的可打印字符（不含 Ctrl/Alt 组合、死键合成，够 M1 用）。
wchar_t VkToChar(WPARAM vk, LPARAM lParam) {
    BYTE state[256] = {};
    if (!::GetKeyboardState(state)) return L'\0';
    const UINT scan = (static_cast<UINT>(lParam) >> 16) & 0xFFu;
    wchar_t buf[2] = {};
    const int n = ::ToUnicode(static_cast<UINT>(vk), scan, state, buf, 2, 0);
    return n == 1 ? buf[0] : L'\0';
}

}  // namespace

CMyabcTextService::CMyabcTextService() : key_router_(config_.candidates) { DllAddRef(); }

CMyabcTextService::~CMyabcTextService() { DllRelease(); }

// ---- IUnknown -------------------------------------------------------------
STDMETHODIMP CMyabcTextService::QueryInterface(REFIID riid, void** ppv) {
    if (ppv == nullptr) return E_INVALIDARG;
    *ppv = nullptr;
    if (::IsEqualIID(riid, IID_IUnknown) || ::IsEqualIID(riid, IID_ITfTextInputProcessor) ||
        ::IsEqualIID(riid, IID_ITfTextInputProcessorEx)) {
        *ppv = static_cast<ITfTextInputProcessorEx*>(this);
    } else if (::IsEqualIID(riid, IID_ITfThreadMgrEventSink)) {
        *ppv = static_cast<ITfThreadMgrEventSink*>(this);
    } else if (::IsEqualIID(riid, IID_ITfKeyEventSink)) {
        *ppv = static_cast<ITfKeyEventSink*>(this);
    } else if (::IsEqualIID(riid, IID_ITfCompositionSink)) {
        *ppv = static_cast<ITfCompositionSink*>(this);
    } else {
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) CMyabcTextService::AddRef() { return ::InterlockedIncrement(&ref_); }

STDMETHODIMP_(ULONG) CMyabcTextService::Release() {
    const LONG c = ::InterlockedDecrement(&ref_);
    if (c == 0) delete this;
    return c;
}

// ---- ITfTextInputProcessorEx --------------------------------------------
STDMETHODIMP CMyabcTextService::Activate(ITfThreadMgr* ptim, TfClientId tid) {
    return ActivateEx(ptim, tid, 0);
}

STDMETHODIMP CMyabcTextService::ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD /*dwFlags*/) {
    thread_mgr_ = ptim;
    if (thread_mgr_) thread_mgr_->AddRef();
    tid_ = tid;

    config_ = myabc::config::Load(CurrentUserSid(), AppDataDir());

    const HRESULT hr = InitSinks();
    if (FAILED(hr)) {
        Deactivate();
        return hr;
    }

    ConnectEngineAndHello();
    return S_OK;
}

STDMETHODIMP CMyabcTextService::Deactivate() {
    UninitSinks();
    if (ipc_) ipc_->FocusOut(kSessionId);
    ipc_.reset();
    composition_.OnExternallyTerminated();   // 防御性清本地指针；文档侧由框架负责终止
    if (thread_mgr_) {
        thread_mgr_->Release();
        thread_mgr_ = nullptr;
    }
    tid_ = TF_CLIENTID_NULL;
    return S_OK;
}

HRESULT CMyabcTextService::InitSinks() {
    if (thread_mgr_ == nullptr) return E_UNEXPECTED;

    wil::com_ptr_nothrow<ITfKeystrokeMgr> keystroke;
    HRESULT hr = thread_mgr_->QueryInterface(IID_PPV_ARGS(&keystroke));
    if (FAILED(hr)) return hr;
    hr = keystroke->AdviseKeyEventSink(tid_, static_cast<ITfKeyEventSink*>(this), TRUE);
    if (FAILED(hr)) return hr;

    wil::com_ptr_nothrow<ITfSource> source;
    hr = thread_mgr_->QueryInterface(IID_PPV_ARGS(&source));
    if (SUCCEEDED(hr)) {
        source->AdviseSink(IID_ITfThreadMgrEventSink, static_cast<ITfThreadMgrEventSink*>(this),
                           &thread_mgr_cookie_);
    }
    return S_OK;
}

void CMyabcTextService::UninitSinks() {
    if (thread_mgr_ == nullptr) return;

    if (thread_mgr_cookie_ != TF_INVALID_COOKIE) {
        wil::com_ptr_nothrow<ITfSource> source;
        if (SUCCEEDED(thread_mgr_->QueryInterface(IID_PPV_ARGS(&source)))) {
            source->UnadviseSink(thread_mgr_cookie_);
        }
        thread_mgr_cookie_ = TF_INVALID_COOKIE;
    }
    wil::com_ptr_nothrow<ITfKeystrokeMgr> keystroke;
    if (SUCCEEDED(thread_mgr_->QueryInterface(IID_PPV_ARGS(&keystroke)))) {
        keystroke->UnadviseKeyEventSink(tid_);
    }
}

void CMyabcTextService::ConnectEngineAndHello() {
    IpcClientConfig ic;
    ic.pipe_name = Widen(config_.ipc.pipe_name_template);
    ic.engine_exe_path = SelfDir() + L"\\" + Widen(config_.engine.exe_path);
    ic.connect_timeout_ms = config_.ipc.connect_timeout_ms;
    ic.request_timeout_ms = config_.ipc.request_timeout_ms;

    ipc_ = std::make_unique<IpcClient>(std::move(ic));
    if (ipc_->Connect()) {
        const std::string ver = ipc_->Hello();
        ::OutputDebugStringA(("[myabc] engine hello -> \"" + ver + "\"\n").c_str());
        ipc_->InitSession(kSessionId);
    } else {
        ::OutputDebugStringA("[myabc] engine hello: connect failed (non-fatal, 按需重连)\n");
    }
}

// ---- ITfThreadMgrEventSink（M1 仍是占位，见头文件 DECISION）--------------
STDMETHODIMP CMyabcTextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnPushContext(ITfContext*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnPopContext(ITfContext*) { return S_OK; }

// ---- ITfKeyEventSink ---------------------------------------------------
STDMETHODIMP CMyabcTextService::OnSetFocus(BOOL /*fForeground*/) {
    // M2：候选窗不在 TIP 进程内了，失焦时的隐藏由引擎在下一次 composing=false 时
    // 经 uiHide 处理（或干脆维持显示直到用户结束组字——候选窗本就该跟着 caret 走，
    // 焦点还在同一光标位置时没必要强制隐藏）。
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnTestKeyDown(ITfContext* /*pic*/, WPARAM wParam, LPARAM lParam,
                                             BOOL* pfEaten) {
    // 不变量 3：必须本地同步答复，不问引擎。
    const wchar_t ch = VkToChar(wParam, lParam);
    *pfEaten = key_router_.IsInterestedKey(static_cast<int>(wParam), ch, composition_.active(),
                                          mode_manager_.mode())
                  ? TRUE
                  : FALSE;
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam,
                                         BOOL* pfEaten) {
    mode_manager_.OnKeyDown(static_cast<int>(wParam));

    const int vk = static_cast<int>(wParam);
    const wchar_t ch = VkToChar(wParam, lParam);
    const bool composing = composition_.active();

    if (!key_router_.IsInterestedKey(vk, ch, composing, mode_manager_.mode())) {
        *pfEaten = FALSE;
        return S_OK;
    }
    *pfEaten = TRUE;   // 不变量：接下来无论如何都不再放行原键，异常时改走"原样插入"兜底

    if (!ipc_ || (!ipc_->connected() && !ipc_->Connect())) {
        HideAndResetComposition(pic);
        if (ch != L'\0') {
            auto* session = new (std::nothrow) CInsertTextEditSession(pic, tid_, std::wstring(1, ch));
            if (session) {
                HRESULT hr = E_FAIL;
                pic->RequestEditSession(tid_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
        }
        return S_OK;
    }

    ipc::Response resp;
    const bool ok = ipc_->ProcessKey(kSessionId, vk, static_cast<unsigned>(ch), resp);
    if (!ok) {
        // M1-12：引擎无响应/超时 -> 降级，结束组字，不阻塞宿主 UI 线程。
        ::OutputDebugStringA("[myabc] processKey timeout/IO error -> 降级\n");
        HideAndResetComposition(pic);
        return S_OK;
    }

    if (!resp.ok || !resp.result.value("handled", false)) {
        // 引擎明确没接住这个键：别丢字符，原样插入。
        if (ch != L'\0') {
            auto* session = new (std::nothrow) CInsertTextEditSession(pic, tid_, std::wstring(1, ch));
            if (session) {
                HRESULT hr = E_FAIL;
                pic->RequestEditSession(tid_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hr);
                session->Release();
            }
        }
        return S_OK;
    }

    ApplyEngineResponse(pic, resp);
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnTestKeyUp(ITfContext* /*pic*/, WPARAM wParam, LPARAM lParam,
                                           BOOL* pfEaten) {
    const wchar_t ch = VkToChar(wParam, lParam);
    *pfEaten = key_router_.IsInterestedKey(static_cast<int>(wParam), ch, composition_.active(),
                                          mode_manager_.mode())
                  ? TRUE
                  : FALSE;
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM /*lParam*/,
                                       BOOL* pfEaten) {
    const bool toggled = mode_manager_.OnKeyUp(static_cast<int>(wParam));
    if (toggled && composition_.active()) HideAndResetComposition(pic);
    *pfEaten = FALSE;   // Shift/普通键弹起本身不产生字符
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

// ---- ITfCompositionSink -------------------------------------------------
STDMETHODIMP CMyabcTextService::OnCompositionTerminated(TfEditCookie /*ec*/,
                                                        ITfComposition* /*composition*/) {
    composition_.OnExternallyTerminated();
    // 候选窗隐藏由引擎在下一次 processKey/... 算出 composing=false 时经 uiHide 处理；
    // 这里只是外部中止（如切焦点）——引擎侧状态留到下次交互再由 cancelComposition
    // 之类的调用收敛，不在这里额外发请求（避免在任意回调里发起 IPC）。
    return S_OK;
}

// ---- helpers ---------------------------------------------------------
void CMyabcTextService::HideAndResetComposition(ITfContext* context) {
    if (composition_.active() && context != nullptr) {
        composition_.Cancel(context, tid_);
    }
}

void CMyabcTextService::ApplyEngineResponse(ITfContext* context, const ipc::Response& resp) {
    CompositionState state;
    state.ApplyResult(resp.result);

    if (state.has_commit) {
        composition_.EndWithText(context, tid_, state.commit_text);
        return;
    }

    if (state.composing) {
        RECT caret{};
        composition_.StartOrUpdate(context, tid_, this, state.preedit, &caret);
        if (ipc_) {
            ipc_->SetCaretRect(kSessionId, caret.left, caret.top, caret.right - caret.left,
                              caret.bottom - caret.top);
        }
        return;
    }

    // handled=true 但既没 commit 也不再 composing（ESC 取消 / 退格清空到底）。
    // 引擎已经在算出 composing=false 的同一时刻自己推了 uiHide，这里只用管本地 TSF 状态。
    HideAndResetComposition(context);
}

}  // namespace myabc::tsf
