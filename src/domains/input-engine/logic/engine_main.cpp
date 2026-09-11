// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/logic/engine_main.cpp --- myabc-engine.exe 入口
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.5 / §5
//       docs/plan/02-m1-libpinyin-quanpin-plan.md §3.3 / §5（M1-1：真 selftest）
//       docs/architecture/system-overview.md §4
//
// 用法：
//   myabc-engine.exe --selftest [--model-dir <dir>]
//       -> pinyin_init(model-dir) -> 解析 "nihao" -> 断言含"你好"，打印结果，退出 0/1
//   myabc-engine.exe [--pipe <name>] [--sid <sid>] [--model-dir <dir>] [--user-dir <dir>]
//       -> 进管道服务端循环，真实接入 libpinyin

#include <windows.h>
#include <sddl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "config_loader.hpp"
#include "crash_guard.hpp"
#include "dispatcher.hpp"
#include "libpinyin_wrapper.hpp"
#include "pipe_server.hpp"
#include "session/session.hpp"
#include "single_instance.hpp"
#include "ui_bridge.hpp"

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

std::string TempDir() {
    char buf[MAX_PATH] = {};
    const DWORD n = ::GetTempPathA(static_cast<DWORD>(sizeof(buf)), buf);
    return n > 0 ? std::string(buf, n) : ".";
}

std::string SelfDir() {
    char path[MAX_PATH] = {};
    ::GetModuleFileNameA(nullptr, path, static_cast<DWORD>(sizeof(path)));
    std::string p(path);
    const auto slash = p.find_last_of("\\/");
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

// M3（plan 04 §3.5）：quanpin 严格全拼（不接受简拼/混拼）；jianpin/hunpin 允许
// （libpinyin 对两者用同一个 PINYIN_INCOMPLETE 开关，见 libpinyin_wrapper.cpp 头注释）。
void ApplySchemeAndFuzzy(myabc::engine::LibPinyinEngine& engine, const myabc::config::Config& cfg) {
    const bool incomplete = cfg.input.scheme != "quanpin";
    engine.ApplyInputOptions(incomplete, cfg.input.fuzzy);
}

myabc::engine::SessionOptions ToSessionOptions(const myabc::config::Config& cfg) {
    myabc::engine::SessionOptions opts;
    opts.page_size = cfg.candidates.page_size;
    opts.page_prev_keys = cfg.candidates.page_prev_keys;
    opts.page_next_keys = cfg.candidates.page_next_keys;
    opts.select_keys = cfg.candidates.select_keys;
    if (!cfg.input.number_lead_key.empty()) opts.number_lead_key = cfg.input.number_lead_key[0];
    opts.bihuo_enabled = cfg.input.bihuo_enabled;
    // 相对安装目录（assets/data/bihuoma.txt 随包分发）；文件不存在时 BihuoTable 静默
    // 留空，过滤退化成空操作，不影响其它候选（见 backend/bihuoma_table.hpp DECISION）。
    opts.bihuo_data_path = SelfDir() + "\\data\\bihuoma.txt";
    return opts;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string sid = ArgValue(argc, argv, "--sid", CurrentUserSid());
    const myabc::config::Config cfg = myabc::config::Load(sid, AppDataDir());

    if (HasFlag(argc, argv, "--selftest")) {
        const std::string model_dir = ArgValue(argc, argv, "--model-dir", cfg.engine.model_dir);
        // selftest 用临时用户目录，不污染真实 %APPDATA%\myabc\userdata。
        const std::string user_dir = ArgValue(argc, argv, "--user-dir", TempDir() + "myabc-selftest");

        myabc::engine::LibPinyinEngine engine;
        if (!engine.Init(model_dir, user_dir)) {
            std::printf("selftest: pinyin_init 失败（model-dir=%s）。exit 1\n", model_dir.c_str());
            return 1;
        }
        ApplySchemeAndFuzzy(engine, cfg);

        engine.ParseAndGuess("nihao");
        const std::string sentence = engine.CurrentSentence();
        const bool ok = sentence.find("你好") != std::string::npos;

        std::printf("selftest: sentence[0] = %s\n", sentence.c_str());
        std::printf("selftest: %s. exit %d\n", ok ? "PASS" : "FAIL", ok ? 0 : 1);
        return ok ? 0 : 1;
    }

    const std::string pipe_name = ArgValue(argc, argv, "--pipe", cfg.ipc.pipe_name_template);
    const std::string model_dir = ArgValue(argc, argv, "--model-dir", cfg.engine.model_dir);
    const std::string user_dir = ArgValue(argc, argv, "--user-dir", cfg.engine.user_data_dir);

    myabc::engine::SingleInstanceGuard guard("Local\\myabc-engine-" + sid);
    if (!guard.acquired()) {
        std::fprintf(stderr, "已有 myabc-engine 实例在运行（sid=%s），退出。\n", sid.c_str());
        return 0;
    }

    myabc::engine::InstallCrashGuard(AppDataDir() + "\\myabc\\logs");

    myabc::engine::LibPinyinEngine engine;
    if (!engine.Init(model_dir, user_dir)) {
        std::fprintf(stderr,
                     "警告：libpinyin 初始化失败（model-dir=%s, user-dir=%s）。"
                     "processKey 将始终 handled:false，hello/shutdown 仍可用。\n",
                     model_dir.c_str(), user_dir.c_str());
    } else {
        ApplySchemeAndFuzzy(engine, cfg);
    }

    const std::string ui_pipe_name = ArgValue(argc, argv, "--ui-pipe", cfg.ipc.ui_pipe_name_template);
    const std::string ui_exe_path =
        ArgValue(argc, argv, "--ui-exe", SelfDir() + "\\" + cfg.ui.exe_path);
    myabc::engine::UiBridge ui_bridge(ui_pipe_name, ui_exe_path);

    myabc::engine::Dispatcher dispatcher(engine, ToSessionOptions(cfg), &ui_bridge);
    myabc::engine::PipeServerOptions server_opts;
    server_opts.pipe_name = pipe_name;
    server_opts.idle_exit_minutes = cfg.engine.idle_exit_minutes;
    // --idle-exit-seconds：测试用极小值覆盖（M2-4 验收要求秒级，不必等分钟级默认值）。
    const std::string idle_seconds_arg = ArgValue(argc, argv, "--idle-exit-seconds", "");
    if (!idle_seconds_arg.empty()) {
        server_opts.idle_exit_seconds_override =
            static_cast<unsigned>(std::strtoul(idle_seconds_arg.c_str(), nullptr, 10));
    }

    std::fprintf(stderr, "myabc-engine %s 监听 %s / ui %s（libpinyin %s）\n",
                 myabc::engine::Dispatcher::kEngineVersion, pipe_name.c_str(),
                 ui_pipe_name.c_str(), engine.ready() ? "ready" : "NOT ready");
    return myabc::engine::RunPipeServer(server_opts, dispatcher);
}
