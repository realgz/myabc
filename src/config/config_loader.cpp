// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/config/config_loader.cpp
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.2

#include "config_loader.hpp"

#include <cstddef>
#include <string>
#include <string_view>

namespace myabc::config {

namespace {

void ReplaceAll(std::string& s, std::string_view from, std::string_view to) {
    if (from.empty()) return;
    for (std::size_t pos = s.find(from); pos != std::string::npos;
         pos = s.find(from, pos + to.size())) {
        s.replace(pos, from.size(), to);
    }
}

}  // namespace

Config ExpandPlaceholders(Config cfg, const std::string& current_user_sid,
                          const std::string& appdata_dir) {
    ReplaceAll(cfg.ipc.pipe_name_template, "{sid}", current_user_sid);
    ReplaceAll(cfg.ipc.ui_pipe_name_template, "{sid}", current_user_sid);
    ReplaceAll(cfg.ipc.extension_pipe_name_template, "{sid}", current_user_sid);
    ReplaceAll(cfg.engine.user_data_dir, "{appdata}", appdata_dir);
    ReplaceAll(cfg.engine.exe_path, "{appdata}", appdata_dir);
    ReplaceAll(cfg.engine.model_dir, "{appdata}", appdata_dir);
    return cfg;
}

Config Load(const std::string& current_user_sid, const std::string& appdata_dir) {
    // TODO(M1+): 若 %APPDATA%\myabc\config.toml 存在，解析并覆盖 Defaults()。
    return ExpandPlaceholders(Defaults(), current_user_sid, appdata_dir);
}

}  // namespace myabc::config
