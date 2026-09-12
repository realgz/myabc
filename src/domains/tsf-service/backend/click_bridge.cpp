// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/click_bridge.cpp

#include "click_bridge.hpp"

// 本文件是 TIP 侧的 ClickBridge 实现；下面这个是 candidate-ui/tsf-service 共用的
// 跨进程契约（窗口类名 + 消息号），见该文件头注释。
#include "click_bridge_protocol.hpp"

namespace myabc::tsf {

ClickBridge::~ClickBridge() { Destroy(); }

bool ClickBridge::Create(std::function<void(int)> on_select) {
    if (hwnd_ != nullptr) return true;   // 幂等：已经建过就不重复建
    on_select_ = std::move(on_select);

    const HINSTANCE hinst = ::GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &ClickBridge::WndProc;
    wc.hInstance = hinst;
    wc.lpszClassName = myabc::ipc::kClickBridgeWindowClass;
    // 重复注册（同进程多个 TIP 实例场景）返回 0/ERROR_CLASS_ALREADY_EXISTS，忽略即可。
    ::RegisterClassExW(&wc);

    hwnd_ = ::CreateWindowExW(0, myabc::ipc::kClickBridgeWindowClass, L"", 0, 0, 0, 0, 0,
                             HWND_MESSAGE, nullptr, hinst, this);
    return hwnd_ != nullptr;
}

void ClickBridge::Destroy() {
    if (hwnd_ != nullptr) {
        ::DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    on_select_ = nullptr;
}

LRESULT CALLBACK ClickBridge::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }
    if (msg == myabc::ipc::kWmSelectCandidateByClick) {
        auto* self = reinterpret_cast<ClickBridge*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self != nullptr && self->on_select_) {
            self->on_select_(static_cast<int>(wp));
        }
        return 0;
    }
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace myabc::tsf
