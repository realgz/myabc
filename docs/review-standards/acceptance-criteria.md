# 交付验收标准（acceptance-criteria）

- 状态：生效
- 更新日期：2026-09-09
- 由 tests/review/ 部分自动校验，其余人工核对
- 关联：docs/plan/roadmap.md、各里程碑计划的"可执行验收判据"

## 0. 通用（每个里程碑都必须满足）

| # | 判据 | 验证方式 |
|---|---|---|
| G1 | 该里程碑计划里列的"可执行验收判据"逐条通过 | 跑对应命令，留输出 |
| G2 | tests/review/run_all 全绿 | CI / 本地脚本 |
| G3 | 前序里程碑的判据回归重跑全过 | 回归套件 |
| G4 | 打了对应 git tag，工作区干净 | git status / git tag |
| G5 | 新增/变更的决策承载点有 DECISION: 注释，必要时新增 ADR | check_decision_comments |
| G6 | 无新增硬编码（路径/端点/超时/键位/langid/字体） | check_hardcode |
| G7 | 所有新源文件含 SPDX-License-Identifier: GPL-3.0-or-later | check_spdx |
| G8 | 架构不变量（system-overview §7）无违反 | check_tip_isolation + check_structure + 人工 |

## 1. "MVP 做完了"的定义（M0-M5 通过 + 以下全部为真）

### 1.1 安装 / 注册
- [ ] myabc-deployer.exe --register 在 Win10 22H2 与 Win11 23H2 上成功，输入法列表出现"智能ABC (myabc)"。
- [ ] --unregister 后列表移除，HKCR\CLSID\{clsid}、CTF profile、Run/Task 自启项全部清除（无残留）。
- [ ] x64 与 x86 的 InprocServer32 均注册正确。

### 1.2 核心输入（记事本 + WordPad）
- [ ] 全拼整句：woshizhongguoren -> "我是中国人" 为 top1 候选，空格上屏。
- [ ] 短句：nihao -> "你好" top1。
- [ ] 简拼：bj -> "北京"；混拼 bjing/beij -> "北京" 命中。
- [ ] 音节切分：xian 候选含"西安"与 xian 系字；预编辑显示 xi'an 隔音。
- [ ] 翻页：config 配的上/下页键有效；1-9 选字有效；空格选 top1。
- [ ] 退格删拼音、ESC 取消、词间连续上屏、部分上屏（选中前段词继续组后段）。
- [ ] Shift 单击切中/英；英文模式字母直接上屏；中文模式标点映射（，。；：？！""''）。

### 1.3 智能ABC经典特性
- [ ] 笔形辅助码：拼音 + 1-5 二级筛选生效。
- [ ] i 引导：i2025 -> 中文数字候选；i1234.56 -> 金额大写候选（符合 GB/T 15835）。
- [ ] GBK：GBK 字集内生僻字（如"喆""堃"）可出现在候选并正确上屏。

### 1.4 学习
- [ ] 造的新词、被反复选择的词，跨引擎重启后仍优先出现。
- [ ] 用户词库可导出为文本、可清空、可导入。
- [ ] 用户库文件缺失/损坏时引擎仍正常启动（降级无学习）。

### 1.5 稳定性 / 性能
- [ ] 引擎进程崩溃或被杀后自动恢复，宿主应用不崩、不卡死超过 ipc.request_timeout_ms。
- [ ] TIP 侧按键处理 p99 < 30ms（不含引擎异步部分）；无丢键（200 字压测）。
- [ ] 引擎空闲 idle_exit_minutes 后自退出，再输入自动重启。
- [ ] myabc-tip.dll 不依赖 d2d1/dwrite/libpinyin/glib（dumpbin /dependents 验证）。
- [ ] 连续使用 1 小时无内存增长趋势（引擎 RSS 稳定）。

### 1.6 兼容矩阵（MVP 必过项）
- [ ] 记事本、WordPad、Chrome（地址栏+网页）、Edge、一个 UWP 应用（设置搜索/邮件）、Word、Excel
      —— 标准动作脚本全部正确。
- [ ] x86 宿主与 x64 宿主均可加载并端到端工作。

## 2. 非 MVP 门槛（明确不作为验收阻塞项）
- 皮肤/主题系统、云输入、双拼、五笔、手写语音、ARM64、安装包签名、商店上架。
- 浏览器地址栏的 reconversion / 完整候选联想（基本组字可用即可）。

## 3. 发布前最终门禁
- [ ] docs/review-standards/code-review-checklist.md 全部勾选。
- [ ] docs/check/ 下有 qa-reviewer 的通过报告。
- [ ] 主会话咨询 advisor 的最终把关记录 + 遗留风险清单。
