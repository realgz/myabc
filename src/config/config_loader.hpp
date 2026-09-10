// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/config/config_loader.hpp --- 运行期配置加载（M0：仅返回默认值 + 占位符展开）
//
// 依据：docs/architecture/system-overview.md §5
//       docs/plan/01-m0-tsf-skeleton-plan.md §3.2（"M0 可仅返回默认值，留 TODO"）

#ifndef MYABC_CONFIG_LOADER_HPP
#define MYABC_CONFIG_LOADER_HPP

#include <string>

#include "config_defaults.hpp"

namespace myabc::config {

// 加载配置：M0 = Defaults()，并把已知占位符展开：
//   {sid}     -> current_user_sid（调用方传入，避免此层依赖 windows.h）
//   {appdata} -> appdata_dir（同上）
// TODO(M1+): 读取 %APPDATA%\myabc\config.toml 覆盖默认值。
Config Load(const std::string& current_user_sid, const std::string& appdata_dir);

// 供测试 / 无 Windows 环境使用：只做占位符替换。
Config ExpandPlaceholders(Config cfg, const std::string& current_user_sid,
                          const std::string& appdata_dir);

}  // namespace myabc::config

#endif  // MYABC_CONFIG_LOADER_HPP
