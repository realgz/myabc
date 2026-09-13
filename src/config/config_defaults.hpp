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
    // 2026-09-12（外部候选源扩展协议，见
    // docs/decisions/input-engine/20260912-extension-candidate-provider.md）：
    // 第三方候选提供者（如闭源的 inputserver 项目，不属于本项目）连接的独立
    // 命名管道，跟上面两条都不共用。
    std::string extension_pipe_name_template = R"(\\.\pipe\myabc-extension-{sid})";
    // 首次连接（含拉起引擎）总超时。DECISION（真机反馈"新机器上记事本卡顿+没候选"，
    // 见 docs/decisions/_debt-log.md 2026-09-11）：引擎冷启动要先跑完
    // pinyin_init()（加载全部词库/模型，本机实测约几十毫秒，但更慢的磁盘/首次
    // 运行被杀毒软件实时扫描时可能明显更久）才开始监听管道，在此之前 TIP 侧
    // Connect() 的每次重试都会失败；原 2000ms 在较慢的机器上可能不够，导致那次
    // 按键直接判定"引擎没接住"、原样放行字母，表现为卡顿+不出候选。调大到 8000ms
    // 只影响"连不上时最多等多久"这个上限，连接一旦成功立刻返回，不影响正常按键
    // 延迟（那是 request_timeout_ms 管的，未改）。
    std::uint32_t connect_timeout_ms = 8000;
    std::uint32_t request_timeout_ms = 50;     // 单次按键请求超时，超时降级
    // 重试退避序列（毫秒）。空 => 不退避直接失败。
    std::string connect_backoff_ms_csv = "50,100,200,400";
};

struct EngineConfig {
    // 相对可执行文件目录解析；空 => 与 TIP DLL 同目录下的 myabc-engine.exe。
    std::string exe_path = "myabc-engine.exe";
    std::string model_dir = "data";                 // 相对安装目录
    std::string user_data_dir = "{appdata}/myabc/userdata";
    // DECISION（同上 connect_timeout_ms 条目）：调大到 30 分钟，减少正常使用中（打字
    // 间隙常见的几分钟停顿）触发引擎冷启动的频率——每次冷启动都是一次"可能撞上
    // connect_timeout_ms 上限"的风险窗口，退出得越不频繁，用户越不容易撞上。
    // 引擎本身内存占用不大（词库常驻），30 分钟真空闲不算浪费。
    std::uint32_t idle_exit_minutes = 30;
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
    // 注意：本字段只管拼音"方案家族"内部的严格度（全拼/简拼/混拼），是否使用拼音
    // 还是五笔由下面的 method 字段决定，两者是正交维度，不要混淆
    // （docs/decisions/input-engine/20260913-wubi-input-scheme.md）。
    std::string scheme = "hunpin";
    // 模糊音开关名集合，取值：c_ch/s_sh/z_zh/f_h/g_k/l_n/l_r/an_ang/en_eng/in_ing/all
    // （对应 libpinyin PinyinAmbiguity2 位，见 backend/libpinyin_wrapper.cpp 的映射表）。
    // 默认空——不默认开模糊音（用户输入不准确就不该被"纠正"，需显式开启）。
    std::vector<std::string> fuzzy;

    // M4（plan 05 §3.2/§3.1）。
    std::string number_lead_key = "i";   // i 引导中文数字/金额，如 i2025、i1234.56
    // 拼音候选后接笔形辅助码二级筛选（如 "wo31"）。DECISION: docs/decisions/_debt-log.md
    // 2026-09-11「笔形辅助码触发键」条目已废弃——不再需要独立触发键，select_keys
    // （数字选字）改为只在按过一次空格之后才生效，笔形数字在此之前一直有效，两者
    // 天然不冲突，见 session.cpp 组字态路由。
    bool bihuo_enabled = true;

    // 2026-09-13（docs/decisions/input-engine/20260913-wubi-input-scheme.md）：
    // 三选一输入方案——smartabc（默认，本字段所有既有语义原样保留）/pinyin（普通
    // 拼音：同一个 libpinyin 引擎，但数字键任何时候直接选字、空格任何时候立即选中
    // 候选[0]，不架住）/wubi（候选来源换成 WubiCandidateSource，按键路由跟 pinyin
    // 共用同一套"数字直选、空格直选"逻辑）。method==pinyin/wubi 时，上面的
    // scheme/fuzzy/number_lead_key/bihuo_enabled 字段结构性地不被使用（路由改变
    // 本身就让笔形码/数字模式没有触发条件，不需要额外开关关闭）。
    std::string method = "smartabc";

    // TIP 侧识别的切换热键描述串，格式 "ctrl+shift+<字母>"（tsf-service 侧的
    // scheme_hotkey_detector 解析，不在 src/config 里做字符串解析逻辑，本字段只是
    // 反硬编码落点本身）。三态循环切换：smartabc -> pinyin -> wubi -> smartabc。
    std::string method_switch_hotkey = "ctrl+shift+w";
    // 注意：五笔编码表路径（wubi86.txt）不在这里建模——跟 bihuoma.txt 走同一条既有
    // 惯例：SessionOptions::bihuo_data_path 从来不是 Config 字段，而是 engine_main.cpp
    // 的 ToSessionOptions() 里用 SelfDir() 拼出来的安装期相对路径常量，wubi_data_path
    // 同样如此（见 engine_main.cpp）。
};

struct OutputConfig {
    // unicode/gb2312/gbk，决定 CharsetFilter（system-overview §6）。
    std::string charset = "gbk";
};

// M5（plan 06 §3.1/§3.5）：用户词库自学习。
// DECISION: docs/decisions/_debt-log.md 2026-09-11——plan 06 §3.5 还列了
// min_uses_to_promote（造词前先攒够 N 次再收录）和 decay_enabled（长期未用词频衰减）。
// 未加这两个字段：libpinyin 自带的 pinyin_remember_user_input 本身就是"按 count
// 累加"的模型，每次都记一次、count 自然爬升，效果等同于一个软性的"用得越多排得越
// 前"，不需要在它之上再叠一层"攒够 N 次才允许创建"的硬门槛（后者需要额外一份跨
// 重启的 (拼音,词条)->次数 计数存储，纯为一个没有验收判据要求的场景加复杂度）；
// decay 同理，plan 自己也写"可选"，没有任何验收判据依赖它。真出现"误选一次就把
// 干扰词顶到前排"的实际反馈，再回来加门槛，字段/存储格式届时再定。
struct LearningConfig {
    bool enabled = true;
    // 每提交 N 次（有 commit 的 processKey/selectCandidate/commitComposition）主动
    // pinyin_save 一次，防止 taskkill/崩溃丢失学习结果（plan §3.2 的"每 N 次 commit"
    // 半，"每 M 分钟"半未实现——没有后台定时器基础设施，按提交次数触发已覆盖 M5-3
    // 的验收场景：造词后 taskkill，见 _debt-log.md）。
    unsigned autosave_every_n_commits = 20;
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
    LearningConfig learning;

    std::uint16_t langid = kDefaultLangId;
};

// 编译期默认值集合（值语义，调用方可拷贝后改）。
inline Config Defaults() { return Config{}; }

}  // namespace myabc::config

#endif  // MYABC_CONFIG_DEFAULTS_HPP
