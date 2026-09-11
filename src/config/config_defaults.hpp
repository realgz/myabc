// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/config/config_defaults.hpp --- 编译期默认配置（反硬编码落点，不变量 8）
//
// 依据：docs/architecture/system-overview.md §5（配置化清单）
//       docs/plan/01-m0-tsf-skeleton-plan.md §3.2
//
// 规则：业务代码只读 Config 对象，源码里不出现裸字符串 / 裸数字。
//       运行期从 %APPDATA%\myabc\config.toml 覆盖（config_loader，TOML 解析仍是 TODO，
//       见 docs/decisions/_debt-log.md 2026-09-11）。
// M1 补了 candidates/input/output；ui.* 等留到真用上（候选窗换 D2D 渲染）时再加。

#ifndef MYABC_CONFIG_DEFAULTS_HPP
#define MYABC_CONFIG_DEFAULTS_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace myabc::config {

// zh-CN。language profile 注册用。集中定义一次，其它域引用此常量而非裸写。
// DECISION: docs/architecture/system-overview.md §5（langid 键）。
inline constexpr std::uint16_t kDefaultLangId = 0x0804;

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

// M1 新增（docs/plan/02-m1-libpinyin-quanpin-plan.md §3.6）。
struct CandidatesConfig {
    unsigned page_size = 9;
    std::string page_prev_keys = "-,";   // 任一字符触发上一页
    std::string page_next_keys = "=.";   // 任一字符触发下一页
    std::string select_keys = "123456789";
};

struct InputConfig {
    // quanpin/jianpin/hunpin（M3 起）/shuangpin（预留）。M1 只实现 quanpin。
    std::string scheme = "quanpin";
    // 模糊音开关集合，映射 libpinyin pinyin_option_t 的 PinyinAmbiguity2 位。M1 为空。
    std::vector<std::string> fuzzy;
};

struct OutputConfig {
    // unicode/gb2312/gbk，决定 CharsetFilter（system-overview §6）。
    std::string charset = "gbk";
};

struct Config {
    IpcConfig ipc;
    EngineConfig engine;
    CandidatesConfig candidates;
    InputConfig input;
    OutputConfig output;

    std::uint16_t langid = kDefaultLangId;
};

// 编译期默认值集合（值语义，调用方可拷贝后改）。
inline Config Defaults() { return Config{}; }

}  // namespace myabc::config

#endif  // MYABC_CONFIG_DEFAULTS_HPP
