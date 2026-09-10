// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/ui/main.cpp --- myabc-deployer.exe 入口
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4 / §5
//
// 用法：myabc-deployer.exe --register | --unregister | --status [--langid <hex>]
// 需要管理员权限（写 HKCR）。COM apartment-threaded。

#include <windows.h>
#include <objbase.h>   // CoInitializeEx / CoUninitialize

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config_defaults.hpp"
#include "deploy_flow.hpp"

namespace {

void PrintUsage() {
    std::puts("用法: myabc-deployer.exe --register | --unregister | --status [--langid <hex>]");
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    myabc::deploy::DeployParams params;
    params.langid = myabc::config::Defaults().langid;  // 反硬编码：默认来自 src/config

    enum class Action { kNone, kRegister, kUnregister, kStatus } action = Action::kNone;

    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], L"--register") == 0) action = Action::kRegister;
        else if (std::wcscmp(argv[i], L"--unregister") == 0) action = Action::kUnregister;
        else if (std::wcscmp(argv[i], L"--status") == 0) action = Action::kStatus;
        else if (std::wcscmp(argv[i], L"--langid") == 0 && i + 1 < argc) {
            params.langid = static_cast<std::uint16_t>(std::wcstoul(argv[++i], nullptr, 0));
        } else {
            std::wprintf(L"未知参数: %ls\n", argv[i]);
            PrintUsage();
            return 2;
        }
    }

    if (action == Action::kNone) {
        PrintUsage();
        return 2;
    }

    const HRESULT hrco = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hrco)) {
        std::fprintf(stderr, "CoInitializeEx 失败: 0x%08lX\n", static_cast<unsigned long>(hrco));
        return 1;
    }

    int rc = 2;
    switch (action) {
        case Action::kRegister:   rc = myabc::deploy::RunRegister(params); break;
        case Action::kUnregister: rc = myabc::deploy::RunUnregister(params); break;
        case Action::kStatus:     rc = myabc::deploy::RunStatus(params); break;
        case Action::kNone:       break;
    }

    ::CoUninitialize();
    return rc;
}
