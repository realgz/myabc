// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/logic/deploy_flow.hpp --- 注册/反注册/状态 的编排
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4 / §5（M0-5 / M0-9）
//       docs/architecture/system-overview.md §7 不变量 4（注册残留清零）

#ifndef MYABC_DEPLOY_FLOW_HPP
#define MYABC_DEPLOY_FLOW_HPP

#include <cstdint>
#include <string>

#include "config_defaults.hpp"

namespace myabc::deploy {

struct DeployParams {
    std::uint16_t langid = myabc::config::kDefaultLangId;
};

// 返回进程退出码（0 成功）。每步结果打印到 stdout/stderr。
int RunRegister(const DeployParams& p);
int RunUnregister(const DeployParams& p);
int RunStatus(const DeployParams& p);

// M5（plan 06 §3.4）：用户词库管理，经命名管道跟（已运行或临时拉起的）引擎对话。
// path 为空时非法（export/import 必须指定文件）；clear 不需要 path。
int RunExportUserDict(const std::string& path);
int RunImportUserDict(const std::string& path);
int RunClearUserDict();

}  // namespace myabc::deploy

#endif  // MYABC_DEPLOY_FLOW_HPP
