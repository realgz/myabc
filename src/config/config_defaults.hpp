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
    // M2（plan 03 §3.2）：引擎 -> myabc-ui 的单向推送通道，独立命名管道。
    std::string ui_pipe_name_template = R"(\\.\pipe\myabc-ui-{sid})";
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
    // quanpin（严格全拼，关闭简拼/混拼）/jianpin/hunpin（简拼与混拼，libpinyin 用同一个
    // PINYIN_INCOMPLETE 开关处理两者，见 docs/decisions/_debt-log.md 2026-09-11）/
    // shuangpin（预留，未实现）。M3 起默认 hunpin，对标智能ABC习惯（plan 04 §3.5）。
    std::string scheme = "hunpin";
    // 模糊音开关名集合，取值：c_ch/s_sh/z_zh/f_h/g_k/l_n/l_r/an_ang/en_eng/in_ing/all
    // （对应 libpinyin PinyinAmbiguity2 位，见 backend/libpinyin_wrapper.cpp 的映射表）。
    // 默认空——不默认开模糊音（用户输入不准确就不该被"纠正"，需显式开启）。
    std::vector<std::string> fuzzy;

    // M4（plan 05 §3.2/§3.1）。
    std::string number_lead_key = "i";   // i 引导中文数字/金额，如 i2025、i1234.56
    bool bihuo_enabled = true;           // 拼音候选后接笔形辅助码二级筛选
    // DECISION: docs/decisions/_debt-log.md 2026-09-11「笔形辅助码触发键」——
    // plan 05 §5 的字面示例是拼音后直接接数字（"wo3"），但 candidates.select_keys
    // 默认就是全部数字（"123456789"，M1 起的既有行为，见 session.cpp 组字态路由），
    // 拼音后裸数字必然先被 select_keys 当选字键吃掉，两者无法共存。改用独立触发键
    // （反引号，不与字母/select_keys/page_keys/标点映射冲突）：先按这个键进入笔形
    // 输入态，例如 "wo`3"，之后 1-5 才追加为笔形码，不影响原有"数字=选字"行为。
    std::string bihuo_lead_key = "`";
};

struct OutputConfig {
    // unicode/gb2312/gbk，决定 CharsetFilter（system-overview §6）。
    std::string charset = "gbk";
};

// 候选窗渲染最小必需项（M1：GDI 渲染，见 candidate-ui 域的 DECISION 注释）。
// 颜色/主题/跟随光标等留到换 D2D 渲染时再加，避免为用不上的字段先建模。
struct UiConfig {
    std::string font = "Microsoft YaHei UI";
    unsigned font_size_pt = 12;
    // M2：myabc-ui.exe 相对安装目录解析（同目录约定，见 engine.exe_path 同款注释）。
    std::string exe_path = "myabc-ui.exe";
};

struct Config {
    IpcConfig ipc;
    EngineConfig engine;
    CandidatesConfig candidates;
    InputConfig input;
    OutputConfig output;
    UiConfig ui;

    std::uint16_t langid = kDefaultLangId;
};

// 编译期默认值集合（值语义，调用方可拷贝后改）。
inline Config Defaults() { return Config{}; }

}  // namespace myabc::config

#endif  // MYABC_CONFIG_DEFAULTS_HPP
