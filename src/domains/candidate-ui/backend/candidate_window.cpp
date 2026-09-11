// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/candidate-ui/backend/candidate_window.cpp
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.4

#include "candidate_window.hpp"

#include <string>

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
        RECT item_line{kPaddingPx, y, client.right - kPaddingPx, y + kLineHeightPx};
        // 智能ABC 风格空格两段式确认（armed_index，见 candidate_view_model.hpp）：
        // 这一项被"架住"但还没真正选中——画一个高亮底色区分于普通候选行，让用户在
        // 按第二次空格/数字键确认前，能看清楚"再按一下就是它了"。
        if (model_.armed_index >= 0 && static_cast<std::size_t>(model_.armed_index) == i) {
            RECT hl{0, y, client.right, y + kLineHeightPx};
            const HBRUSH brush = ::CreateSolidBrush(RGB(0xCC, 0xE5, 0xFF));
            ::FillRect(dc, &hl, brush);
            ::DeleteObject(brush);
        }
        const std::wstring text = std::to_wstring(i + 1) + L". " + model_.items[i];
        ::DrawTextW(dc, text.c_str(), static_cast<int>(text.size()), &item_line,
                   DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);
        y += kLineHeightPx;
    }

    ::SelectObject(dc, old_font);
    ::EndPaint(hwnd, &ps);
}

}  // namespace myabc::ui
