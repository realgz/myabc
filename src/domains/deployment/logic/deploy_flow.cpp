// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/logic/deploy_flow.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4

#include "deploy_flow.hpp"

#include <windows.h>

#include <cstdio>
#include <string>

#include "com_register.hpp"
#include "tsf_register.hpp"

namespace myabc::deploy {

namespace {

const wchar_t* kDisplayName = L"智能ABC (myabc)";

void Report(const char* step, HRESULT hr) {
    if (SUCCEEDED(hr)) {
        std::printf("  [ok]   %s\n", step);
    } else {
        std::printf("  [FAIL] %s (hr=0x%08lX)\n", step, static_cast<unsigned long>(hr));
    }
}

}  // namespace

int RunRegister(const DeployParams& p) {
    std::printf("myabc-deployer --register (langid=0x%04X)\n", p.langid);

    HRESULT hr = RegisterComServer(L"");
    Report("COM InprocServer32", hr);
    if (FAILED(hr)) return 1;

    hr = RegisterProfiles(p.langid, kDisplayName, L"");
    Report("TSF language profile", hr);
    if (FAILED(hr)) {
        UnregisterComServer();
        return 1;
    }

    hr = RegisterCategories();
    Report("TSF categories (TIP_KEYBOARD + TIPCAP)", hr);
    if (FAILED(hr)) {
        UnregisterProfiles(p.langid);
        UnregisterComServer();
        return 1;
    }

    const bool ok = IsProfileRegistered(p.langid);
    std::printf("  verify: profile %s\n", ok ? "present" : "NOT FOUND");
    return ok ? 0 : 1;
}

int RunUnregister(const DeployParams& p) {
    std::printf("myabc-deployer --unregister (langid=0x%04X)\n", p.langid);
    // 顺序与注册相反；每步尽量执行，最后汇总。
    Report("TSF categories", UnregisterCategories());
    Report("TSF language profile", UnregisterProfiles(p.langid));
    Report("COM InprocServer32", UnregisterComServer());

    const bool gone = !IsProfileRegistered(p.langid);
    std::wstring leftover;
    const bool com_gone = !QueryComServer(leftover);
    std::printf("  verify: profile %s, InprocServer32 %s\n", gone ? "removed" : "STILL PRESENT",
                com_gone ? "removed" : "STILL PRESENT");
    return (gone && com_gone) ? 0 : 1;
}

int RunStatus(const DeployParams& p) {
    const bool profile = IsProfileRegistered(p.langid);
    std::wstring dll;
    const bool com = QueryComServer(dll);

    std::printf("myabc-deployer --status (langid=0x%04X)\n", p.langid);
    std::printf("  TSF profile      : %s\n", profile ? "registered" : "not registered");
    if (com) {
        std::printf("  InprocServer32   : %ls\n", dll.c_str());
    } else {
        std::printf("  InprocServer32   : not registered\n");
    }
    return (profile && com) ? 0 : 2;   // 2 = 未完全注册（非错误）
}

}  // namespace myabc::deploy
