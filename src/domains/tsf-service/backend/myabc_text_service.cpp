// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/myabc_text_service.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.3
//
// 参考微软 SampleIME（MIT）：SampleIME.cpp 的 Activate/Deactivate、KeyEventSink.cpp 的
// _InitKeyEventSink / OnKeyDown 结构。DECISION: docs/decisions/_debt-log.md（来源登记）。

#include "myabc_text_service.hpp"

#include <sddl.h>

#include <string>

#include <wil/com.h>

#include "config_loader.hpp"
#include "dll_refcount.hpp"
#include "edit_session.hpp"
#include "guids.hpp"

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
        const int len = ::WideCharToMultiByte(CP_UTF8, 0, p, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            out.assign(static_cast<std::size_t>(len - 1), '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, p, -1, out.data(), len, nullptr, nullptr);
        }
        free(p);
    }
    return out;
}

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::wstring SelfDir() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(static_cast<HMODULE>(DllInstanceHandle()), path,
                         static_cast<DWORD>(std::size(path)));
    std::wstring p(path);
    const auto slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

}  // namespace

CMyabcTextService::CMyabcTextService() { DllAddRef(); }

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

    const HRESULT hr = InitSinks();
    if (FAILED(hr)) {
        Deactivate();
        return hr;
    }
    ConnectEngineAndHello();  // M0：一次 hello 往返，失败只记日志不阻塞
    return S_OK;
}

STDMETHODIMP CMyabcTextService::Deactivate() {
    UninitSinks();
    ipc_.reset();
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
    const myabc::config::Config cfg = myabc::config::Load(CurrentUserSid(), AppDataDir());

    IpcClientConfig ic;
    ic.pipe_name = Widen(cfg.ipc.pipe_name_template);
    // engine.exe_path 相对安装目录解析（M0：与 TIP DLL 同目录）。
    ic.engine_exe_path = SelfDir() + L"\\" + Widen(cfg.engine.exe_path);
    ic.connect_timeout_ms = cfg.ipc.connect_timeout_ms;
    ic.request_timeout_ms = cfg.ipc.request_timeout_ms;

    ipc_ = std::make_unique<IpcClient>(std::move(ic));
    if (ipc_->Connect()) {
        const std::string ver = ipc_->Hello();
        ::OutputDebugStringA(("[myabc] engine hello -> \"" + ver + "\"\n").c_str());
    } else {
        ::OutputDebugStringA("[myabc] engine hello: connect failed (M0 non-fatal)\n");
    }
}

// ---- ITfThreadMgrEventSink（M0 占位）-----------------------------------
STDMETHODIMP CMyabcTextService::OnInitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnUninitDocumentMgr(ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnSetFocus(ITfDocumentMgr*, ITfDocumentMgr*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnPushContext(ITfContext*) { return S_OK; }
STDMETHODIMP CMyabcTextService::OnPopContext(ITfContext*) { return S_OK; }

// ---- ITfKeyEventSink ---------------------------------------------------
STDMETHODIMP CMyabcTextService::OnSetFocus(BOOL /*fForeground*/) { return S_OK; }

STDMETHODIMP CMyabcTextService::OnTestKeyDown(ITfContext* /*pic*/, WPARAM wParam, LPARAM /*lParam*/,
                                             BOOL* pfEaten) {
    // 不变量 3：必须本地同步答复。
    *pfEaten = key_router_.IsInterestedKey(static_cast<int>(wParam), CompositionState::Idle) ? TRUE
                                                                                            : FALSE;
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM /*lParam*/,
                                         BOOL* pfEaten) {
    if (!key_router_.IsInterestedKey(static_cast<int>(wParam), CompositionState::Idle)) {
        *pfEaten = FALSE;
        return S_OK;
    }
    *pfEaten = TRUE;
    // M0：'A' -> 上屏写死的"啊"。异常一律吞掉并放行（不变量：按键异常不影响宿主）。
    CommitText(pic, KeyRouter::kM0CommitForA);
    return S_OK;
}

STDMETHODIMP CMyabcTextService::OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}
STDMETHODIMP CMyabcTextService::OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}
STDMETHODIMP CMyabcTextService::OnPreservedKey(ITfContext*, REFGUID, BOOL* pfEaten) {
    *pfEaten = FALSE;
    return S_OK;
}

// ---- helpers ---------------------------------------------------------
void CMyabcTextService::CommitText(ITfContext* context, const wchar_t* text) {
    if (context == nullptr || tid_ == TF_CLIENTID_NULL) return;

    auto* session = new (std::nothrow) CInsertTextEditSession(context, tid_, text);
    if (session == nullptr) return;

    HRESULT hrSession = E_FAIL;
    context->RequestEditSession(tid_, session, TF_ES_ASYNCDONTCARE | TF_ES_READWRITE, &hrSession);
    session->Release();
}

}  // namespace myabc::tsf
