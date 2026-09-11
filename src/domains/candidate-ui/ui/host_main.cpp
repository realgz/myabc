// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/candidate-ui/ui/host_main.cpp --- myabc-ui.exe 入口
//
// 依据：docs/plan/03-m2-engine-process-ipc-plan.md §3.1
//
// 只做一件事：连引擎的 uiShow/uiHide 推送管道，把收到的消息转成 CandidateWindow 的
// Show/Hide 调用。引擎断线（崩溃/重启）时重连；重连前隐藏候选窗，不留悬空内容。
// 单实例（命名互斥量），重复启动的实例发现已有实例后直接退出（见 ui_bridge.cpp 的
// "重复拉起无害"假设）。

#include <windows.h>
#include <sddl.h>

#include <cstdio>
#include <cstdlib>
#include <string>

#include "candidate_window.hpp"
#include "config_loader.hpp"
#include "frame.hpp"
#include "json_codec.hpp"

namespace {

std::string CurrentUserSid() {
    HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) return "nosid";
    DWORD len = 0;
    ::GetTokenInformation(token, TokenUser, nullptr, 0, &len);
    std::string buf(len, '\0');
    std::string sid = "nosid";
    if (len > 0 && ::GetTokenInformation(token, TokenUser, buf.data(), len, &len)) {
        const auto* tu = reinterpret_cast<const TOKEN_USER*>(buf.data());
        LPSTR s = nullptr;
        if (::ConvertSidToStringSidA(tu->User.Sid, &s) && s) {
            sid = s;
            ::LocalFree(s);
        }
    }
    ::CloseHandle(token);
    return sid;
}

std::string AppDataDir() {
    if (const char* p = std::getenv("APPDATA")) return p;
    return ".";
}

std::wstring Widen(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int n =
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), w.data(), n);
    return w;
}

void ApplyMessage(myabc::ui::CandidateWindow& win, const myabc::ipc::Request& req) {
    using myabc::ipc::Method;
    if (req.method == Method::kUiHide) {
        win.Hide();
        return;
    }
    if (req.method != Method::kUiShow) return;

    myabc::ui::CandidateViewModel vm;
    vm.preedit = Widen(req.params.value("preedit", std::string{}));
    if (const auto it = req.params.find("candidates"); it != req.params.end() && it->is_array()) {
        for (const auto& c : *it) vm.items.push_back(Widen(c.value("text", std::string{})));
    }
    if (const auto it = req.params.find("page"); it != req.params.end() && it->is_object()) {
        vm.page_index = it->value("index", 0);
        vm.page_total = it->value("total", 0);
    }

    RECT anchor{};
    if (const auto it = req.params.find("caretRect"); it != req.params.end() && it->is_object()) {
        anchor.left = it->value("x", 0);
        anchor.top = it->value("y", 0);
        anchor.right = anchor.left + it->value("w", 0);
        anchor.bottom = anchor.top + it->value("h", 0);
    }
    win.Show(anchor, vm);
}

}  // namespace

int main() {
    // 单实例：命名互斥量，重复启动直接退出（引擎重复 CreateProcess 时无害）。
    const std::string sid = CurrentUserSid();
    ::CreateMutexA(nullptr, TRUE, ("Local\\myabc-ui-" + sid).c_str());
    if (::GetLastError() == ERROR_ALREADY_EXISTS) return 0;

    const myabc::config::Config cfg = myabc::config::Load(sid, AppDataDir());
    const std::string pipe_name = cfg.ipc.ui_pipe_name_template;

    myabc::ui::CandidateWindow window;
    if (!window.Create(Widen(cfg.ui.font), cfg.ui.font_size_pt)) return 1;

    std::string body;
    for (;;) {
        HANDLE pipe = ::CreateFileA(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                    OPEN_EXISTING, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            ::Sleep(1000);   // 引擎可能还没起来/正在重启，退避重试
            continue;
        }

        const myabc::ipc::ReadFn reader = [pipe](void* buf, std::size_t n) -> std::ptrdiff_t {
            DWORD got = 0;
            if (!::ReadFile(pipe, buf, static_cast<DWORD>(n), &got, nullptr)) {
                const DWORD e = ::GetLastError();
                return (e == ERROR_BROKEN_PIPE || e == ERROR_PIPE_NOT_CONNECTED) ? 0 : -1;
            }
            return static_cast<std::ptrdiff_t>(got);
        };

        while (myabc::ipc::ReadFrame(reader, body) == myabc::ipc::FrameStatus::kOk) {
            myabc::ipc::Request req;
            if (myabc::ipc::DecodeRequest(body, req) == myabc::ipc::DecodeStatus::kOk) {
                ApplyMessage(window, req);
            }
        }

        ::CloseHandle(pipe);
        window.Hide();   // 引擎断线：候选窗不留悬空内容
        ::Sleep(500);
    }
}
