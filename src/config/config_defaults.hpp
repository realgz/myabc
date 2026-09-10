// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/config/config_defaults.hpp --- 编译期默认配置（反硬编码落点，不变量 8）
//
// 依据：docs/architecture/system-overview.md §5（配置化清单）
//       docs/plan/01-m0-tsf-skeleton-plan.md §3.2
//
// 规则：业务代码只读 Config 对象，源码里不出现裸字符串 / 裸数字。
//       运行期从 %APPDATA%\myabc\config.toml 覆盖（config_loader）。
// M0 只需 ipc / engine 少量键 + langid；其余键随里程碑补充（保持本结构单点扩展）。

#ifndef MYABC_CONFIG_DEFAULTS_HPP
#define MYABC_CONFIG_DEFAULTS_HPP

#include <cstdint>
#include <string>

namespace myabc::config {

struct IpcConfig {
    // {sid} 占位符由 config_loader 在运行期替换为当前用户 SID。
    std::string pipe_name_template = R"(\\.\pipe\myabc-engine-{sid})";
    std::uint32_t connect_timeout_ms = 2000;   // 首次连接（含拉起引擎）总超时
    std::uint32_t request_timeout_ms = 50;     // 单次按键请求超时，超时降级
    // 重试退避序列（毫秒）。空 => 不退避直接失败。
    std::string connect_backoff_ms_csv = "50,100,200,400";
};

struct EngineConfig {
    // 相对可执行文件目录解析；空 => 与 TIP DLL 同目录下的 myabc-engine.exe。
    std::string exe_path = "myabc-engine.exe";
    std::string model_dir = "data";                 // 相对安装目录
    std::string user_data_dir = "{appdata}/myabc/userdata";
    std::uint32_t idle_exit_minutes = 10;
};

struct Config {
    IpcConfig ipc;
    EngineConfig engine;

    // zh-CN，注册 language profile 用。DECISION: system-overview §5（langid 键）。
    std::uint16_t langid = 0x0804;
};

// 编译期默认值集合（值语义，调用方可拷贝后改）。
inline Config Defaults() { return Config{}; }

}  // namespace myabc::config

#endif  // MYABC_CONFIG_DEFAULTS_HPP
