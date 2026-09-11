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
#include <string>

#include "config_defaults.hpp"
#include "deploy_flow.hpp"

namespace {

void PrintUsage() {
    std::puts(
        "用法: myabc-deployer.exe --register | --unregister | --status [--langid <hex>]\n"
        "      myabc-deployer.exe --export-userdict <path> | --import-userdict <path> "
        "| --clear-userdict");
}

std::string NarrowUtf8(const wchar_t* w) {
    if (w == nullptr || *w == L'\0') return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<std::size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 0) ::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    myabc::deploy::DeployParams params;
    params.langid = myabc::config::Defaults().langid;  // 反硬编码：默认来自 src/config

    enum class Action {
        kNone, kRegister, kUnregister, kStatus,
        kExportUserDict, kImportUserDict, kClearUserDict,
    } action = Action::kNone;
    std::string userdict_path;

    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], L"--register") == 0) action = Action::kRegister;
        else if (std::wcscmp(argv[i], L"--unregister") == 0) action = Action::kUnregister;
        else if (std::wcscmp(argv[i], L"--status") == 0) action = Action::kStatus;
        else if (std::wcscmp(argv[i], L"--langid") == 0 && i + 1 < argc) {
            params.langid = static_cast<std::uint16_t>(std::wcstoul(argv[++i], nullptr, 0));
        } else if (std::wcscmp(argv[i], L"--export-userdict") == 0 && i + 1 < argc) {
            action = Action::kExportUserDict;
            userdict_path = NarrowUtf8(argv[++i]);
        } else if (std::wcscmp(argv[i], L"--import-userdict") == 0 && i + 1 < argc) {
            action = Action::kImportUserDict;
            userdict_path = NarrowUtf8(argv[++i]);
        } else if (std::wcscmp(argv[i], L"--clear-userdict") == 0) {
            action = Action::kClearUserDict;
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

    // M5（plan 06 §3.4）：用户词库管理只走命名管道跟引擎对话，不碰 HKCR/TSF 注册表，
    // 不需要 COM/管理员权限——独立于下面 register/unregister/status 的 COM 初始化路径。
    if (action == Action::kExportUserDict) return myabc::deploy::RunExportUserDict(userdict_path);
    if (action == Action::kImportUserDict) return myabc::deploy::RunImportUserDict(userdict_path);
    if (action == Action::kClearUserDict) return myabc::deploy::RunClearUserDict();

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
        default: break;
    }

    ::CoUninitialize();
    return rc;
}
