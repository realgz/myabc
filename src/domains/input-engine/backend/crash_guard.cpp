// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/crash_guard.cpp
// 依据：docs/plan/03-m2-engine-process-ipc-plan.md §3.4

#include "crash_guard.hpp"

#include <windows.h>

#include <dbghelp.h>

#include <cstdio>

namespace myabc::engine {

namespace {

// 全局：异常处理器不能安全地做堆分配/复杂初始化，日志目录提前存好。
char g_log_dir[MAX_PATH] = {};

LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info) {
    char path[MAX_PATH * 2];
    const DWORD tick = ::GetTickCount();
    std::snprintf(path, sizeof(path), "%s\\myabc-engine-crash-%lu.dmp", g_log_dir,
                 static_cast<unsigned long>(tick));

    const HANDLE file = ::CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                      FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{};
        mei.ThreadId = ::GetCurrentThreadId();
        mei.ExceptionPointers = info;
        mei.ClientPointers = FALSE;

        ::MiniDumpWriteDump(::GetCurrentProcess(), ::GetCurrentProcessId(), file,
                           MiniDumpNormal, &mei, nullptr, nullptr);
        ::CloseHandle(file);
    }

    // DECISION: 不在这里调 pinyin_save——见头文件注释。
    return EXCEPTION_EXECUTE_HANDLER;
}

}  // namespace

void InstallCrashGuard(const std::string& log_dir) {
    ::GetFullPathNameA(log_dir.c_str(), static_cast<DWORD>(sizeof(g_log_dir)), g_log_dir, nullptr);
    ::CreateDirectoryA(g_log_dir, nullptr);   // 已存在则忽略失败
    ::SetUnhandledExceptionFilter(&OnUnhandledException);
}

}  // namespace myabc::engine
