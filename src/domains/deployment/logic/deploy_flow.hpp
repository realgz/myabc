// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/deployment/logic/deploy_flow.hpp --- 注册/反注册/状态 的编排
//
// 依据：docs/plan/01-m0-tsf-skeleton-plan.md §3.4 / §5（M0-5 / M0-9）
//       docs/architecture/system-overview.md §7 不变量 4（注册残留清零）

#ifndef MYABC_DEPLOY_FLOW_HPP
#define MYABC_DEPLOY_FLOW_HPP

#include <cstdint>

#include "config_defaults.hpp"

namespace myabc::deploy {

struct DeployParams {
    std::uint16_t langid = myabc::config::kDefaultLangId;
};

// 返回进程退出码（0 成功）。每步结果打印到 stdout/stderr。
int RunRegister(const DeployParams& p);
int RunUnregister(const DeployParams& p);
int RunStatus(const DeployParams& p);

}  // namespace myabc::deploy

#endif  // MYABC_DEPLOY_FLOW_HPP
