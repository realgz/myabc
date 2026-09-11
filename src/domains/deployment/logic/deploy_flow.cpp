// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/logic/deploy_flow.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4

#include "deploy_flow.hpp"

#include <windows.h>

#include <sddl.h>

#include <cstdio>
#include <string>

#include "com_register.hpp"
#include "config_loader.hpp"
#include "engine_client.hpp"
#include "tsf_register.hpp"

namespace myabc::deploy {

namespace {

const wchar_t* kDisplayName = L"智能ABC (myabc)";

// DECISION: docs/decisions/_debt-log.md 2026-09-11——SelfDir/CurrentUserSid/AppDataDir
// 这几个小工具函数在 engine_main.cpp、myabc_text_service.cpp 里各有一份几乎相同的
// 拷贝，这是第三份。都是纯技术、无业务含义的代码（构建思路见 code-quality-standards.md
// §1"纯技术性、无业务含义的工具函数才允许放 shared/"），够得上抽到 src/shared/ 的门槛，
// 但抽取属于跨 M5 范围的重构，本轮先照抄现有模式落地功能，抽取留作后续整理项。
std::wstring SelfDir() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring p(path);
    const auto slash = p.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : p.substr(0, slash);
}

std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

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

// 落盘操作（export/import/clear）不能只靠"进程存在" 就以为引擎已加载好目标用户的
// user_dir——如果没有正在跑的引擎，EngineClient::Connect() 会拉起一个新的，用的是
// config.engine.user_data_dir（跟 TIP 拉起时用的是同一份配置解析结果），数据一致。
EngineClient MakeEngineClient() {
    const std::string sid = CurrentUserSid();
    const myabc::config::Config cfg = myabc::config::Load(sid, AppDataDir());

    EngineClientConfig ecfg;
    ecfg.pipe_name = Widen(cfg.ipc.pipe_name_template);
    ecfg.engine_exe_path = SelfDir() + L"\\" + Widen(cfg.engine.exe_path);
    return EngineClient(ecfg);
}

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

int RunExportUserDict(const std::string& path) {
    if (path.empty()) {
        std::fprintf(stderr, "缺文件路径：--export-userdict <path>\n");
        return 2;
    }
    EngineClient client = MakeEngineClient();
    std::string err;
    if (!client.UserDictExport(path, err)) {
        std::fprintf(stderr, "导出失败：%s\n", err.c_str());
        return 1;
    }
    std::printf("已导出用户词库到 %s\n", path.c_str());
    return 0;
}

int RunImportUserDict(const std::string& path) {
    if (path.empty()) {
        std::fprintf(stderr, "缺文件路径：--import-userdict <path>\n");
        return 2;
    }
    EngineClient client = MakeEngineClient();
    std::string err;
    if (!client.UserDictImport(path, err)) {
        std::fprintf(stderr, "导入失败：%s\n", err.c_str());
        return 1;
    }
    std::printf("已从 %s 导入用户词库\n", path.c_str());
    return 0;
}

int RunClearUserDict() {
    EngineClient client = MakeEngineClient();
    std::string err;
    if (!client.UserDictClear(err)) {
        std::fprintf(stderr, "清空失败：%s\n", err.c_str());
        return 1;
    }
    std::puts("已清空用户词库");
    return 0;
}

}  // namespace myabc::deploy
