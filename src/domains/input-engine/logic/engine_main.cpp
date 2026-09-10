// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/engine_main.cpp --- myabc-engine.exe 入口
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5 / §5（M0-2 / M0-4 / M0-8）
//       docs/architecture/system-overview.md §4
//
// 用法：
//   myabc-engine.exe --selftest [--model-dir <dir>]   -> 打印自测行，退出 0（M0 占位）
//   myabc-engine.exe [--pipe <name>] [--sid <sid>]     -> 进管道服务端循环
//
// M0 不接入 libpinyin：--selftest 仅确认可执行文件能跑、参数解析正常。

#include <windows.h>
#include <sddl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "config_loader.hpp"
#include "dispatcher.hpp"
#include "pipe_server.hpp"
#include "single_instance.hpp"

namespace {

std::string ArgValue(int argc, char** argv, const char* flag, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
    }
    return fallback;
}

bool HasFlag(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) return true;
    }
    return false;
}

std::string CurrentUserSid() {
    // 供互斥量 / 管道名占位符使用。失败时回退到会话隔离的固定串。
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

}  // namespace

int main(int argc, char** argv) {
    const std::string sid = ArgValue(argc, argv, "--sid", CurrentUserSid());
    const myabc::config::Config cfg = myabc::config::Load(sid, AppDataDir());

    if (HasFlag(argc, argv, "--selftest")) {
        const std::string model_dir = ArgValue(argc, argv, "--model-dir", cfg.engine.model_dir);
        // DECISION: docs/plan/00-toolchain-and-build-plan.md §3.5 —— 真自测（nihao->你好）
        // 依赖词库二进制，推迟到 M1（见 docs/decisions/_debt-log.md 2026-09-10）。
        std::printf("selftest: libpinyin not wired yet (M0). model-dir=%s. exit 0\n",
                    model_dir.c_str());
        return 0;
    }

    const std::string pipe_name = ArgValue(argc, argv, "--pipe", cfg.ipc.pipe_name_template);

    myabc::engine::SingleInstanceGuard guard("Local\\myabc-engine-" + sid);
    if (!guard.acquired()) {
        std::fprintf(stderr, "已有 myabc-engine 实例在运行（sid=%s），退出。\n", sid.c_str());
        return 0;
    }

    myabc::engine::Dispatcher dispatcher;
    myabc::engine::PipeServerOptions opts;
    opts.pipe_name = pipe_name;
    opts.idle_exit_minutes = cfg.engine.idle_exit_minutes;

    std::fprintf(stderr, "myabc-engine %s 监听 %s\n", myabc::engine::Dispatcher::kEngineVersion,
                 pipe_name.c_str());
    return myabc::engine::RunPipeServer(opts, dispatcher);
}
