# 07 · M6 计划（浏览器 / UWP / Office 兼容打磨 + 32/64 位）

- 状态：生效
- 更新日期：2026-09-09
- 前置：M5（tag m5-userdict）
- 对应需求：3.3 平台兼容（验收硬指标）
- 回滚锚点：tag m6-compat / v0.1-mvp

## 1. 背景与目标
让 IME 在需求 3.3 列出的全部宿主可靠工作，x86 与 x64 宿主都能加载。

## 2. 现状分析
- M0-M5 主要在记事本验证。
- AppContainer（UWP）下 TIP 与引擎管道通信需要显式 ACL；未处理。
- x86 tip dll 在 M0 已构建，但按键/IPC/候选未在 32 位宿主端到端验证。
- Office 有自己的文本存储与合成怪癖（Word 的富文本 range、Excel 编辑栏）。

## 3. 文件级改造点

### 3.1 AppContainer / 管道 ACL
| 文件 | 内容 |
|---|---|
| input-engine/backend/pipe_server.cpp | CreateNamedPipe 时用 SECURITY_ATTRIBUTES，SDDL 授予 "ALL APPLICATION PACKAGES"（S-1-15-2-1）
  与当前用户读写；DACL 从 config.ipc.pipe_sddl 取（可配，默认给 AppContainer 读写） |
| tsf-service/backend/engine_supervisor.cpp | AppContainer 内无法 CreateProcess 拉起引擎 -> 检测 token（IsAppContainer），是则不自拉，
  只连接；引擎的拉起交给桌面 broker（登录启动项 myabc-engine --daemon 或计划任务） |
| deployment/backend | 注册时安装 broker 自启（HKCU\...\Run 或 Task Scheduler），--unregister 时移除 |

### 3.2 宿主适配
| 文件 | 内容 |
|---|---|
| tsf-service/backend/composition.cpp | 处理 ITfContextComposition 不支持的宿主：fallback 到 SetText + 手动光标；
  查询 TF_ES_NOOWNERSHIP / TS_SD_READONLY，只读区放行原键 |
| tsf-service/backend/edit_session.cpp | Word：用 ITfRange 而非绝对偏移；Excel 编辑栏焦点检测 |
| tsf-service/logic/composition_state.cpp | 浏览器地址栏（reconversion 不支持）：禁用部分上屏依赖的特性，基本组字仍工作 |
| tsf-service/backend/display_attribute.cpp | 确认在 Chrome/Edge/UWP 下预编辑下划线正确；不支持则纯文本预编辑 |

### 3.3 32 位链路
- scripts/build.ps1 -Arch x86 常态化；stage 到 out/<config>/x86/。
- deployment 注册 x86 CLSID InprocServer32（Wow6432Node）。
- 引擎保持 x64 单一；x86 tip <-> x64 引擎同一命名管道（协议本就架构无关，校验 struct 无指针/无 long）。

### 3.4 config
- ipc.pipe_sddl；compat.disable_display_attribute_hosts（进程名列表）；
  compat.force_sync_edit_hosts。

## 4. 不改动清单
- 不支持 ARM64（不构建、不测试）。
- 不做 IMM32 回退路径（reference 已定 TSF 唯一实现）。
- 不改转换/学习逻辑与既有判据。
- 不做安装包签名 / 商店上架。

## 5. 可执行验收判据（兼容矩阵，人工 + 半自动）

对每个宿主执行标准脚本：输入 woshizhongguoren 选整句、输入 bj 选北京、Shift 切英文打 abc、
中文模式打逗号句号、退格重选、ESC 取消。

| # | 宿主 | 期望 |
|---|---|---|
| M6-1 | 记事本（Win10 22H2 + Win11 23H2） | 全部动作正确 |
| M6-2 | WordPad | 全部正确 |
| M6-3 | Chrome：地址栏 + 网页 textarea | 组字、候选、上屏正确；地址栏至少基本组字可用 |
| M6-4 | Edge：同上 | 同上 |
| M6-5 | UWP：设置搜索框 / 邮件 | 候选窗出现、上屏正确；引擎经 broker 已运行 |
| M6-6 | Word | 富文本域组字、上屏、预编辑正确 |
| M6-7 | Excel：单元格 + 编辑栏 | 上屏正确，不串位 |
| M6-8 | 32 位宿主（如 x86 版某编辑器 / SysWOW64 程序） | 加载 x86 tip.dll，端到端正确 |
| M6-9 | 快速切换宿主焦点（Alt+Tab 组字中） | 无残留预编辑、无崩溃、候选窗随焦点隐藏 |
| M6-10 | 安全桌面（UAC 提示）不加载/不崩 | 无异常 |
| M6-11 | tests/review/run_all | 全绿 |
| M6-12 | tests/integration 兼容回归脚本（能自动化的部分：记事本/WordPad/UIA 驱动） | 通过 |

回归判据：M1-M5 全部判据重跑通过；历史坑清单（卸载残留、locale 解析、O_BINARY round-trip、
引擎崩溃恢复、异步丢键）全部纳入回归套件。

## 6. 风险与回滚
- UWP broker 自启被杀软拦截：提供手动启动指引；引擎 --daemon 模式低资源常驻。
- Office 版本差异大：锁定测试版本（Microsoft 365 当前 + Office 2021），其它尽力。
- 浏览器更新导致 TSF 行为回归：兼容矩阵纳入发布前门禁。
- 回滚：git checkout m5-userdict。
