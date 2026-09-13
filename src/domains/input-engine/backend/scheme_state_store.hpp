// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/scheme_state_store.hpp --- 输入方案选择的跨进程持久化
//
// 依据：docs/plan/08-wubi-input-scheme-plan.md §9 决策点 3（用户拍板：需要持久化，
//       方向为"不实现完整 TOML 解析器，改用一个独立的小状态文件"）
//       docs/decisions/input-engine/20260913-wubi-input-scheme.md
//
// DECISION: 不为这一个字段实现完整 TOML 解析器（config_loader 的 TOML 解析从 M1
// 起就是 TODO，见 docs/decisions/_debt-log.md 2026-09-11）——只写一个内容只有一行
// 的小文件（%APPDATA%\myabc\scheme-state.txt，内容就是 method 字符串本身，如
// "wubi"，不是 JSON/TOML，没有解析开销），setConfig 处理成功后落盘一次，
// engine_main.cpp 启动时读一次覆盖 cfg.input.method。文件不存在/内容不认识：静默
// 忽略，回退到 cfg 默认值，不报错、不崩溃（同项目里其它"数据缺失即安全空操作"的
// 既有惯例，见 backend/bihuoma_table.hpp、backend/wubi_table.hpp DECISION）。

#ifndef MYABC_ENGINE_SCHEME_STATE_STORE_HPP
#define MYABC_ENGINE_SCHEME_STATE_STORE_HPP

#include <optional>
#include <string>

namespace myabc::engine {

// 读取上次持久化的 method 值（"smartabc"/"pinyin"/"wubi"）。文件不存在/打不开/内容
// 为空返回 std::nullopt——调用方应保留 cfg 默认值，不视为错误。不校验值是否是
// 三个合法取值之一：交给调用方（engine_main.cpp）用同一套 "未知值兜底 smartabc" 的
// 既有逻辑处理，这里只负责读文件本身。
std::optional<std::string> LoadSchemeState(const std::string& path);

// 写入当前 method 值，覆盖已有内容。失败（如目录不存在、只读文件系统）静默忽略——
// 持久化是锦上添花的体验增强，不应该因为写盘失败就影响 setConfig 本身的成功响应。
void SaveSchemeState(const std::string& path, const std::string& method);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_SCHEME_STATE_STORE_HPP
