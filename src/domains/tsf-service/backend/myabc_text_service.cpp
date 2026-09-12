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

bool IsCtrlDown() { return (::GetKeyState(VK_CONTROL) & 0x8000) != 0; }

// DECISION（用户 2026-09-12 要求：不按空格也能用 Ctrl+数字直接选字，见
// src/domains/input-engine/logic/session/session.cpp DECISION）：Ctrl 按住时，
// ToUnicode 对数字键通常给不出字符（数字没有标准的"控制字符"定义，不像 Ctrl+字母
// 有 0x01-0x1A 那套），VkToChar 会返回 L'\0'，导致 key_router 认不出这是个"有意义
// 的数字键"。VK_0..VK_9 的值恰好等于 ASCII '0'..'9'，Ctrl+数字场景下直接从 vk 合成
// 字符，不依赖 ToUnicode 这条不可靠的路径。非 Ctrl 或非数字键时行为不变。
wchar_t VkToCharCtrlAware(WPARAM vk, LPARAM lParam, bool ctrl) {
    if (ctrl && vk >= '0' && vk <= '9') return static_cast<wchar_t>(vk);
    return VkToChar(vk, lParam);
}

}  // namespace

CMyabcTextService::CMyabcTextService() : key_router_(config_.candidates) { DllAddRef(); }

CMyabcTextService::~CMyabcTextService() {
    SetCachedContext(nullptr);   // 防御性收尾：正常路径下 Deactivate() 应该已经清过
    DllRelease();
}

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
    click_bridge_.Destroy();
    SetCachedContext(nullptr);
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
    // DECISION（真机反馈"启动会冻结窗口"，见 docs/decisions/_debt-log.md
    // 2026-09-12）：这里以前调阻塞的 Connect()（含 CreateProcess 拉起引擎 + 重试），
    // 会卡住宿主应用激活输入法时的那次调用，冷启动慢的机器上感觉像整个窗口冻结。
    // 改成 EnsureConnectedAsync()：立即返回，真正的连接工作在后台线程里跑，不阻塞
    // 这次激活。若这次没连上（十有八九，因为引擎还没起来），Hello/InitSession 不
    // 强求立刻做——OnKeyDown 每次按键都会自己 EnsureConnectedAsync()，一旦后台线程
    // 连上了自然接上，不需要在这里等。
    if (ipc_->EnsureConnectedAsync()) {
        const std::string ver = ipc_->Hello();
        ::OutputDebugStringA(("[myabc] engine hello -> \"" + ver + "\"\n").c_str());
        ipc_->InitSession(kSessionId);
    } else {
        ::OutputDebugStringA("[myabc] engine hello: connecting in background（按需重连）\n");
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
    const bool ctrl = IsCtrlDown();
    const wchar_t ch = VkToCharCtrlAware(wParam, lParam, ctrl);
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
    const bool ctrl = IsCtrlDown();
    const wchar_t ch = VkToCharCtrlAware(wParam, lParam, ctrl);
    const bool composing = composition_.active();

    if (!key_router_.IsInterestedKey(vk, ch, composing, mode_manager_.mode())) {
        *pfEaten = FALSE;
        return S_OK;
    }
    *pfEaten = TRUE;   // 不变量：接下来无论如何都不再放行原键，异常时改走"原样插入"兜底

    // DECISION（真机反馈"打字卡顿冻结"，见 docs/decisions/_debt-log.md 2026-09-12）：
    // 原来这里调阻塞的 Connect()，冷启动慢时这一下按键会卡住 UI 线程最长
    // connect_timeout_ms。改用 EnsureConnectedAsync()：没连上就立即返回 false（走
    // 下面"原样插入字符"的降级路径，跟以前"引擎没接住"的降级路径一样），真正的
    // 连接尝试转到后台线程，不阻塞这次按键。
    if (!ipc_ || (!ipc_->connected() && !ipc_->EnsureConnectedAsync())) {
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
    const bool ok = ipc_->ProcessKey(kSessionId, vk, static_cast<unsigned>(ch), ctrl, resp);
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
    // 跟 OnTestKeyDown 用同一份判定（M1 R2 debt-log 记的既有惯例），包括 Ctrl+数字
    // 的字符合成，否则 KeyUp 可能因为判定不一致而给出跟 KeyDown 矛盾的 *pfEaten。
    const wchar_t ch = VkToCharCtrlAware(wParam, lParam, IsCtrlDown());
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
    click_bridge_.Destroy();   // 组字被外部中止（如切焦点）：鼠标点击桥也一并收掉
    SetCachedContext(nullptr);
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
        click_bridge_.Destroy();   // 组字结束：鼠标点击桥没有存在的意义了
        SetCachedContext(nullptr);
        return;
    }

    if (state.composing) {
        RECT caret{};
        composition_.StartOrUpdate(context, tid_, this, state.preedit, &caret);
        if (ipc_) {
            ipc_->SetCaretRect(kSessionId, caret.left, caret.top, caret.right - caret.left,
                              caret.bottom - caret.top);
        }
        // 鼠标点击选字（见 click_bridge.hpp）：只在正在组字期间存在这个桥，
        // 组字状态每次刷新（包括点击本身触发的这次 ApplyEngineResponse）都要
        // 重新缓存最新的 context——理论上同一次组字里 context 不会变，但用赋值
        // 而不是"只在没缓存时才存"更简单也更保险（万一真的变了也能跟上）。
        SetCachedContext(context);
        if (!click_bridge_.active()) {
            click_bridge_.Create([this](int index) { OnCandidateClicked(index); });
        }
        return;
    }

    // handled=true 但既没 commit 也不再 composing（ESC 取消 / 退格清空到底）。
    // 引擎已经在算出 composing=false 的同一时刻自己推了 uiHide，这里只用管本地 TSF 状态。
    HideAndResetComposition(context);
    click_bridge_.Destroy();
    SetCachedContext(nullptr);
}

void CMyabcTextService::SetCachedContext(ITfContext* context) {
    if (cached_context_ == context) return;
    if (cached_context_ != nullptr) cached_context_->Release();
    cached_context_ = context;
    if (cached_context_ != nullptr) cached_context_->AddRef();
}

void CMyabcTextService::OnCandidateClicked(int index) {
    // 在 ClickBridge 的 WndProc 里同步调用——跟 OnKeyDown 同一个线程（宿主应用
    // UI 线程），可以放心做同样的 COM/TSF 调用。cached_context_ 为空说明组字已经
    // 在别的路径结束了（正常的竞态：点击消息还在路上，组字先一步收尾），直接丢弃。
    if (!ipc_ || cached_context_ == nullptr) return;

    ipc::Response resp;
    const bool ok = ipc_->SelectCandidate(kSessionId, index, resp);
    if (!ok || !resp.ok || !resp.result.value("handled", false)) return;

    ApplyEngineResponse(cached_context_, resp);
}

}  // namespace myabc::tsf
