// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/crash_guard.hpp --- 顶层异常兜底（落 minidump）
//
// 依据：docs/plan/03-m2-engine-process-ipc-plan.md §3.4
//
// DECISION: 只落 minidump + 退出，不在崩溃处理器里调用 pinyin_save——崩溃现场的
// libpinyin/glib 状态可能已损坏，从异常处理器回调进业务逻辑本身就有二次崩溃风险，
// 真正专业的作法就是"尽快、尽量少动作地"把现场存下来退出。见 _debt-log.md。

#ifndef MYABC_ENGINE_CRASH_GUARD_HPP
#define MYABC_ENGINE_CRASH_GUARD_HPP

#include <string>

namespace myabc::engine {

// 安装 SetUnhandledExceptionFilter；崩溃时把 minidump 写到 <log_dir>\myabc-engine-crash-*.dmp。
// 幂等，进程内调一次即可（通常在 main 开头）。
void InstallCrashGuard(const std::string& log_dir);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_CRASH_GUARD_HPP
