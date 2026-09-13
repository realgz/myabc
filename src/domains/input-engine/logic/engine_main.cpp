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
#include "extension_bridge.hpp"
#include "libpinyin_wrapper.hpp"
#include "pipe_server.hpp"
#include "scheme_state_store.hpp"
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
    // 同上模式（见 backend/wubi_table.hpp DECISION）。
    opts.wubi_data_path = SelfDir() + "\\data\\wubi86.txt";
    opts.learning_enabled = cfg.learning.enabled;
    opts.autosave_every_n_commits = cfg.learning.autosave_every_n_commits;

    // 2026-09-13（docs/decisions/input-engine/20260913-wubi-input-scheme.md）：
    // cfg.input.method 三选一映射到 InputScheme；未知值兜底 kSmartAbc（同 protocol
    // 层 HandleSetConfig 拒绝未知值不同——这里是启动期读配置，容错优先于报错，跟
    // 项目里"配置缺失/不认识就退回安全默认值"的既有惯例一致）。
    opts.scheme = myabc::engine::MethodStringToInputScheme(cfg.input.method)
                      .value_or(myabc::engine::InputScheme::kSmartAbc);
    return opts;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string sid = ArgValue(argc, argv, "--sid", CurrentUserSid());
    myabc::config::Config cfg = myabc::config::Load(sid, AppDataDir());

    // 2026-09-13：方案选择持久化（用户拍板要求，见 scheme_state_store.hpp DECISION）。
    // 启动时读一次状态文件覆盖 cfg.input.method；文件不存在（首次运行/从未切换过）
    // 时 LoadSchemeState 返回 nullopt，cfg 保留编译期默认值 "smartabc"，行为不变。
    // 目录显式创建（不依赖 engine.Init() 创建 user_data_dir 的副作用——真实默认配置
    // 下 user_data_dir 恰好也在 %APPDATA%\myabc 下所以副作用凑巧生效，但 --user-dir
    // 被覆盖成其它路径时（测试、未来的多用户场景）不应该依赖这个隐式关系，同
    // crash_guard.cpp InstallCrashGuard 的"已存在则忽略失败"既有写法）。
    const std::string myabc_appdata_dir = AppDataDir() + "\\myabc";
    ::CreateDirectoryA(myabc_appdata_dir.c_str(), nullptr);
    const std::string scheme_state_path = myabc_appdata_dir + "\\scheme-state.txt";
    if (const auto saved = myabc::engine::LoadSchemeState(scheme_state_path)) {
        cfg.input.method = *saved;
    }

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

    // 2026-09-12（外部候选源扩展协议，见
    // docs/decisions/input-engine/20260912-extension-candidate-provider.md）：
    // 跟 UiBridge 不同，这条管道不会自动拉起任何进程——第三方提供者是否安装/
    // 运行完全是可选的，引擎这边只是把管道开着，没人连也不影响正常使用。
    const std::string extension_pipe_name =
        ArgValue(argc, argv, "--extension-pipe", cfg.ipc.extension_pipe_name_template);
    myabc::engine::ExtensionBridge extension_bridge(extension_pipe_name);

    myabc::engine::SessionOptions session_opts = ToSessionOptions(cfg);
    // --autosave-every-n-commits：测试用小值覆盖（M5-3 验收要小到几次 commit 就能触发
    // autosave，不必等默认值 20），同款做法见下面 --idle-exit-seconds。
    const std::string autosave_arg = ArgValue(argc, argv, "--autosave-every-n-commits", "");
    if (!autosave_arg.empty()) {
        session_opts.autosave_every_n_commits =
            static_cast<unsigned>(std::strtoul(autosave_arg.c_str(), nullptr, 10));
    }
    myabc::engine::Dispatcher dispatcher(engine, session_opts, &ui_bridge, &extension_bridge,
                                         scheme_state_path);
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
