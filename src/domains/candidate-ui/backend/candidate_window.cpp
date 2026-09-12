// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/candidate-ui/backend/candidate_window.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.4

#include "candidate_window.hpp"

#include <windowsx.h>   // GET_X_LPARAM/GET_Y_LPARAM

#include <string>

#include "click_bridge_protocol.hpp"

namespace myabc::ui {

namespace {
constexpr wchar_t kWindowClass[] = L"MyabcCandidateWindow";
constexpr UINT WM_MYABC_SHOW = WM_APP + 1;
constexpr UINT WM_MYABC_HIDE = WM_APP + 2;
constexpr UINT WM_MYABC_QUIT = WM_APP + 3;

constexpr int kPaddingPx = 8;
constexpr int kLineHeightPx = 22;
}  // namespace

CandidateWindow::~CandidateWindow() { Destroy(); }

bool CandidateWindow::Create(const std::wstring& font_name, unsigned font_size_pt) {
    ready_event_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);

    LOGFONTW lf{};
    lf.lfHeight = -static_cast<int>(font_size_pt * 96 / 72);   // pt -> px @ 96 DPI 近似
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfQuality = CLEARTYPE_QUALITY;
    wcsncpy_s(lf.lfFaceName, font_name.c_str(), _TRUNCATE);
    font_ = ::CreateFontIndirectW(&lf);

    thread_ = ::CreateThread(nullptr, 0, &CandidateWindow::ThreadProc, this, 0, nullptr);
    if (thread_ == nullptr) return false;

    ::WaitForSingleObject(ready_event_, 2000);
    return hwnd_ != nullptr;
}

void CandidateWindow::Destroy() {
    if (hwnd_ != nullptr) {
        ::PostMessageW(hwnd_, WM_MYABC_QUIT, 0, 0);
    }
    if (thread_ != nullptr) {
        ::WaitForSingleObject(thread_, 2000);
        ::CloseHandle(thread_);
        thread_ = nullptr;
    }
    if (ready_event_ != nullptr) {
        ::CloseHandle(ready_event_);
        ready_event_ = nullptr;
    }
    if (font_ != nullptr) {
        ::DeleteObject(font_);
        font_ = nullptr;
    }
    hwnd_ = nullptr;
}

void CandidateWindow::Show(const RECT& anchor, const CandidateViewModel& model) {
    if (hwnd_ == nullptr) return;
    auto* req = new (std::nothrow) ShowRequest{anchor, model};
    if (req == nullptr) return;
    ::PostMessageW(hwnd_, WM_MYABC_SHOW, 0, reinterpret_cast<LPARAM>(req));
}

void CandidateWindow::Hide() {
    if (hwnd_ != nullptr) ::PostMessageW(hwnd_, WM_MYABC_HIDE, 0, 0);
}

DWORD WINAPI CandidateWindow::ThreadProc(LPVOID param) {
    auto* self = static_cast<CandidateWindow*>(param);
    self->RunOnWindowThread();
    return 0;
}

void CandidateWindow::RunOnWindowThread() {
    const HINSTANCE hinst = ::GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &CandidateWindow::WndProc;
    wc.hInstance = hinst;
    wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, kWindowClass,
                             L"", WS_POPUP | WS_BORDER, 0, 0, 100, 100, nullptr, nullptr, hinst,
                             this);
    ::SetEvent(ready_event_);

    MSG msg;
    while (::GetMessageW(&msg, nullptr, 0, 0) > 0) {
        ::TranslateMessage(&msg);
        ::DispatchMessageW(&msg);
    }

    if (hwnd_ != nullptr) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    ::UnregisterClassW(kWindowClass, hinst);
}

void CandidateWindow::HandleShow(ShowRequest* req) {
    model_ = std::move(req->model);
    delete req;
    // 每次内容/锚点刷新都清掉旧的悬停高亮：候选窗通常会跟着光标挪位置，鼠标物理
    // 位置没变，但换到新内容/新位置后原来那个下标不再有意义，等下一次真实的
    // WM_MOUSEMOVE 自然会算出新的悬停项（大概率是 -1，因为窗口已经挪走了）。
    hover_index_ = -1;

    const int width = 240;
    const int height = kPaddingPx * 2 + kLineHeightPx * (1 + static_cast<int>(model_.items.size()));

    ::SetWindowPos(hwnd_, HWND_TOPMOST, 0, 0, width, height, SWP_NOMOVE | SWP_NOACTIVATE);
    ::InvalidateRect(hwnd_, nullptr, TRUE);
    ::ShowWindow(hwnd_, SW_SHOWNOACTIVATE);
}

LRESULT CALLBACK CandidateWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<CandidateWindow*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (msg) {
        case WM_NCCREATE: {
            auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
            ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                               reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
            return ::DefWindowProcW(hwnd, msg, wp, lp);
        }
        case WM_MYABC_SHOW: {
            auto* req = reinterpret_cast<CandidateWindow::ShowRequest*>(lp);
            if (self != nullptr && req != nullptr) {
                // 先按 anchor 挪窗口位置（左下角对齐光标矩形左下角），再刷新内容。
                const RECT a = req->anchor;
                ::SetWindowPos(hwnd, HWND_TOPMOST, a.left, a.bottom, 0, 0,
                              SWP_NOSIZE | SWP_NOACTIVATE);
                self->HandleShow(req);
            }
            return 0;
        }
        case WM_MYABC_HIDE:
            ::ShowWindow(hwnd, SW_HIDE);
            return 0;
        case WM_MYABC_QUIT:
            ::PostQuitMessage(0);
            return 0;
        case WM_PAINT:
            if (self != nullptr) self->Paint(hwnd);
            return 0;
        case WM_MOUSEMOVE: {
            if (self != nullptr) {
                self->HandleMouseMove(hwnd, POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            // 见 HandleMouseMove 里 TrackMouseEvent(TME_LEAVE) 的登记——鼠标真的移出
            // 窗口客户区（不是移到另一行）时才会收到这个消息。
            if (self != nullptr && self->hover_index_ != -1) {
                self->hover_index_ = -1;
                self->tracking_mouse_ = false;
                ::InvalidateRect(hwnd, nullptr, TRUE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (self != nullptr) {
                self->HandleLButtonUp(POINT{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)});
            }
            return 0;
        }
        default:
            return ::DefWindowProcW(hwnd, msg, wp, lp);
    }
}

void CandidateWindow::Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    const HDC dc = ::BeginPaint(hwnd, &ps);

    const HFONT old_font = static_cast<HFONT>(::SelectObject(dc, font_));
    ::SetBkMode(dc, TRANSPARENT);

    RECT client{};
    ::GetClientRect(hwnd, &client);
    ::FillRect(dc, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));

    int y = kPaddingPx;
    RECT line{kPaddingPx, y, client.right - kPaddingPx, y + kLineHeightPx};
    std::wstring preedit_line = model_.preedit;
    ::DrawTextW(dc, preedit_line.c_str(), static_cast<int>(preedit_line.size()), &line,
               DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
    y += kLineHeightPx;

    for (std::size_t i = 0; i < model_.items.size(); ++i) {
        // 智能ABC 风格空格两段式确认（armed_index，见 candidate_view_model.hpp）：
        // 这一项被"架住"但还没真正选中——画一个高亮底色区分于普通候选行，让用户在
        // 按第二次空格/数字键确认前，能看清楚"再按一下就是它了"。鼠标悬停（hover_index_）
        // 用更浅的底色，两者都命中时 armed 优先（更明确的状态盖过纯粹的鼠标位置提示）。
        const RECT hl = ItemRectFor(i);
        COLORREF hl_color = 0;
        bool draw_hl = true;
        if (model_.armed_index >= 0 && static_cast<std::size_t>(model_.armed_index) == i) {
            hl_color = RGB(0xCC, 0xE5, 0xFF);
        } else if (hover_index_ >= 0 && static_cast<std::size_t>(hover_index_) == i) {
            hl_color = RGB(0xE8, 0xE8, 0xE8);
        } else {
            draw_hl = false;
        }
        if (draw_hl) {
            const HBRUSH brush = ::CreateSolidBrush(hl_color);
            ::FillRect(dc, &hl, brush);
            ::DeleteObject(brush);
        }
        RECT item_line{kPaddingPx, y, client.right - kPaddingPx, y + kLineHeightPx};
        const std::wstring text = std::to_wstring(i + 1) + L". " + model_.items[i];
        ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &item_line,
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        y += kLineHeightPx;
    }

    ::SelectObject(dc, old_font);
    ::EndPaint(hwnd, &ps);
}

RECT CandidateWindow::ItemRectFor(std::size_t index) const {
    // 跟 Paint() 里算 y 的方式完全一致（+1 跳过第一行 preedit）；整行宽度（0 到
    // client.right），不是 Paint() 里画文字用的带左右 padding 的 item_line——
    // 高亮/命中测试要覆盖整行，点哪都算数，不用非要点在文字上。
    RECT client{};
    ::GetClientRect(hwnd_, &client);
    const int y = kPaddingPx + kLineHeightPx * (1 + static_cast<int>(index));
    return RECT{0, y, client.right, y + kLineHeightPx};
}

int CandidateWindow::HitTest(POINT client_pt) const {
    for (std::size_t i = 0; i < model_.items.size(); ++i) {
        const RECT r = ItemRectFor(i);
        if (client_pt.x >= r.left && client_pt.x < r.right && client_pt.y >= r.top &&
            client_pt.y < r.bottom) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void CandidateWindow::HandleMouseMove(HWND hwnd, POINT client_pt) {
    if (!tracking_mouse_) {
        // 只有登记过 TME_LEAVE，鼠标真正移出客户区时才会收到 WM_MOUSELEAVE——不登记
        // 的话悬停高亮会在鼠标移出窗口后卡住不消失。
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        if (::TrackMouseEvent(&tme)) tracking_mouse_ = true;
    }
    const int hit = HitTest(client_pt);
    if (hit != hover_index_) {
        hover_index_ = hit;
        ::InvalidateRect(hwnd, nullptr, TRUE);
    }
}

void CandidateWindow::HandleLButtonUp(POINT client_pt) {
    const int hit = HitTest(client_pt);
    if (hit >= 0) NotifyTipOfClick(hit);
}

void CandidateWindow::NotifyTipOfClick(int index) const {
    // 见 src/shared/click-bridge-protocol/click_bridge_protocol.hpp：TIP 只在正在
    // 组字期间维护这个窗口，系统同一时刻最多一个，找到即代表就是当前活跃的那个。
    const HWND target = ::FindWindowExW(HWND_MESSAGE, nullptr, ipc::kClickBridgeWindowClass, nullptr);
    if (target != nullptr) {
        ::PostMessageW(target, ipc::kWmSelectCandidateByClick, static_cast<WPARAM>(index), 0);
    }
    // 找不到（正好这一瞬间组字状态已经结束）：无害地丢弃这次点击，不重试——跟其它
    // 单向推送失败时的处理原则一致（比如 ui_bridge 写失败就放弃这次推送）。
}

}  // namespace myabc::ui
