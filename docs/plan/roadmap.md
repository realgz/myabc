# myabc 实施路线图（roadmap）

- 状态：生效
- 更新日期：2026-09-09
- 来源：deep-planner 规划
- 上游：docs/requirements/smart-abc-ime-requirements.md、docs/architecture/system-overview.md
- 下游细化计划：本目录 00~07 各文件

## 执行方须知（无会话上下文也能消费）

- 本项目当前为空仓（master 无提交，仅 docs 骨架）。所有 src/ 代码由执行方（Sonnet）按各里程碑计划新建。
- 工具链、依赖、CMake 骨架见 `00-toolchain-and-build-plan.md`，是 M0 的前置。
- 严格遵守 `docs/architecture/system-overview.md` §7 的九条不变量，以及 `docs/review-standards/`。
- 每个里程碑结束：跑该里程碑"验收判据" + `tests/review/` 全绿，才可交下一轮质检。
- 每个里程碑完成后打 git tag：`m0-skeleton` / `m1-quanpin` / ... 作为回滚锚点。
- 可参考但不得整体拷贝（License/架构差异）：微软 SampleIME（MIT，TSF 骨架）、Weasel（GPLv3，架构）、
  PIME（LGPL，命名管道 JSON 后端）。拷贝 SampleIME 代码片段时保留其版权头并在 `docs/decisions/_debt-log.md` 登记来源。

## 里程碑总览

| 里程碑 | 目标 | 关键产物 | 详细计划 |
|---|---|---|---|
| M0 | 工具链就绪 + 空 TSF TIP 骨架能注册、被选中、拦截按键、上屏写死的字；引擎进程与 IPC 通道跑通 echo | myabc-tip.dll（骨架）、myabc-deployer.exe、myabc-engine.exe（echo）、顶层 CMake | 00 + 01 |
| M1 | 引擎接入 libpinyin 全拼 -> 智能整句候选；候选窗显示；翻页选字上屏 | input-engine（libpinyin）、candidate-ui、processKey 全链路 | 02 |
| M2 | 引擎进程生命周期加固 + 异步按键路径 + 候选渲染迁到 myabc-ui.exe，DLL 变薄 | myabc-ui.exe、引擎自动拉起/单实例/崩溃恢复 | 03 |
| M3 | 简拼 / 混拼 / 音节切分歧义（xian -> xi'an 与 xian） | JianpinParser / HunpinParser、切分提示 | 04 |
| M4 | 笔形辅助码（1-5）、i 引导中文数字 / 金额大写、GBK 生僻字 | 笔形筛选、NumberCurrencyCandidateSource、GbkFilter | 05 |
| M5 | 用户词库自学习（记新词、调词频，跨重启保留） | user-dict domain | 06 |
| M6 | 浏览器 / UWP / Office 兼容打磨 + 32/64 位宿主 | AppContainer 管道 ACL、x86 TIP、兼容矩阵测试 | 07 |

## 依赖关系

```
00 工具链/构建 ──> 01 M0 ──> 02 M1 ──> 03 M2 ──> 04 M3 ──> 05 M4 ──> 06 M5 ──> 07 M6
                                 └──> (candidate-ui M1 内嵌版)
```

M3/M4/M5 之间弱耦合，M2 完成后可并行，但建议顺序推进以降低质检复杂度。

## MVP 定义

M0-M5 全部通过 + M6 的兼容矩阵中"记事本 / WordPad / Chrome / Edge / 一个 UWP 应用 / Word / Excel"
七项全绿，且 x86 与 x64 宿主均可加载，即为 MVP。详细逐条判据见 `docs/review-standards/acceptance-criteria.md`。

## 已知遗留风险（贯穿所有里程碑，详见各计划"风险与回滚"）

- libpinyin 在 MinGW-w64 UCRT64 的构建仍需实机验证，Plan B 触发判据见 00 号计划 §6。
- 整体 GPLv3 copyleft；若 MSYS2 Berkeley DB 为 6.x（AGPLv3），组合作品受 AGPL §13 约束（离线 IME 义务基本落空，但需登记）。
- ARM64 仅架构预留，不实现。
- 安装包不签名，SmartScreen 会告警；开发期用 regsvr32 / deployer。
- AppContainer（UWP）下管道访问需要正确 ACL，是 M6 硬骨头。
