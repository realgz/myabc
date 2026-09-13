# 输入方案选择：智能ABC / 普通拼音 / 五笔字型（86 版）

- 日期：2026-09-13
- 领域：input-engine（含 tsf-service 侧的热键/持久化落点）
- 状态：生效中
- 触发来源：用户对话（"加入多个输入法方案的选择：普通拼音，五笔（需实现）"）+
  deep-planner 规划（`docs/plan/08-wubi-input-scheme-plan.md`，含用户对 §9 三个
  决策点的拍板与后续对 §4.2/§4.3 的纠正，§10 为该计划的最终权威版本）

## 背景

myabc 上线以来只有一条拼音转换链路（libpinyin），且拼音的按键路由（尤其是
2026-09-11 定型的"空格两段式确认"）是专为"智能ABC"这种数字键身兼"笔形码/数字
续写"与"选字"两种含义的场景设计的。用户要求新增五笔字型输入（从零实现），并在
澄清过程中指出：两段式确认本身是智能ABC 的特定产物，不是"拼音"这个大类必须具备
的东西——由此确定为三态模型而非最初计划的二态模型。

## 决策内容

### 1. 三态 `InputScheme`

`InputScheme{kSmartAbc, kPlainPinyin, kWubi}`（`candidate_source.hpp`），与
`InputMode`（中/英文）是正交维度。

- **kSmartAbc**（默认，兼容既有行为）：libpinyin 候选 + 空格两段式确认 + 笔形
  辅助码 + `i` 数字金额模式。`session.cpp` 里这部分代码逐字未改。
- **kPlainPinyin**（新增）：跟 kSmartAbc 共用同一个 libpinyin 引擎/
  `PinyinCandidateSource`（全拼/简拼/混拼/模糊音照常可用），但按键路由不同——
  数字键任何时候直接选字，空格任何时候立即选中候选[0]，不架住。因为数字键
  永远不会被追加进 `raw_`，笔形辅助码/`i` 数字模式在这个方案下结构性地没有
  触发条件（不需要额外开关关闭，是路由改变的自然结果）。
- **kWubi**（新增，从零实现）：候选来源换成 `WubiCandidateSource`，按键路由
  跟 kPlainPinyin 共用同一套"数字直选、空格直选"逻辑（`RouteDigitKeyDirect`）。

### 2. 五笔数据来源与查表

`assets/data/wubi86.txt`：转换自 [rime/rime-wubi](https://github.com/rime/rime-wubi)
的 `wubi86.dict.yaml`（LGPL-3.0，与 myabc GPL-3.0-or-later 兼容）。转换规则、
License 归属链路（Wozy 原始码表 -> Yu Yuwei/Chen Xing/Gong Chen 转 Rime 格式）
见该文件头部注释与 `assets/README.md`。**只做精确码查表，不实现拆字算法**
（词组编码已经是词典里现成数据）。`WubiTable`（`backend/wubi_table.hpp/.cpp`）
仿照 `BihuoTable` 的"安全空表"模式：文件缺失/损坏时静默留空，五笔模式退化为
"任何编码都查不到候选"而非崩溃。

### 3. 单键字根候选不自动确认（对计划 W1 判据的修正，真机/e2e 测试中发现的真 bug）

**这是本次实现中偏离原计划文档的最重要一点**：`docs/plan/08-...md` §5.5/§7.2
W1 原本设想复用 `Recompute()` 既有的"候选只剩一个就自动选中"规则（这条规则是
为拼音设计的：libpinyin 对完整输入做语境分析后给出的单一候选是高置信度终态）。

**实测发现**：86 版五笔的 25 个字根键每一个都有唯一对应的"键名字"单字（如
`"a"`->`"工"`）。这意味着几乎任何 2 键以上词的第 1 个字母敲下时，都会先命中
"当前编码长度下只有一个精确匹配"这个条件——如果沿用拼音那套规则，用户永远打不出
任何多键词：敲下 `"a"` 就被自动提交+清空组字状态，永远无法继续敲出 `"aaad"`
（"工期"）。这跟拼音场景的"候选收窄到一个=不再有更多合理延伸"完全不是一回事：
五笔是**精确码表查询**，不同长度的编码是完全独立、不相关的两次查询，"当前长度
恰好只有一条匹配"不能推出"用户不打算再输入了"。

**修正**：`Session::Recompute()` 的自动选中规则改为
`if (opts_.scheme != InputScheme::kWubi && candidates_.size() == 1) return SelectCandidate(0);`
——五笔的单字根候选需要显式确认（空格键，`kWubi` 走统一的直选路由，一次空格
即可），这也更符合真实五笔输入法"单根字敲完按空格上屏"的实际使用习惯（并非
反例：真实五笔里孤立的单键字根字本来就是要按空格才上屏，不是敲一下就自动跳出）。

拼音路径（`SelectCandidate()` 内 `current_source_uses_engine_choose_==true` 分支
的类似规则）不受影响——`kWubi` 走 `UsesEngineChoose()==false` 的原子候选路径，
从不触碰那段代码。

### 4. 数字键路由三分：`RouteDigitKeyPinyin` vs `RouteDigitKeyDirect`

`Session::ProcessKey` 组字态下"数字键怎么解释"按 scheme 拆成两个私有方法：
- `RouteDigitKeyPinyin`（仅 kSmartAbc）：原样迁移 2026-09-11 决定的既有逻辑
  （数字模式续写/笔形辅助码续写/`space_armed_` 之后的 `select_keys`），一字
  不改，返回 `std::optional<SessionResult>`（`std::nullopt` 表示这三段都不该
  拦截这个键，调用方继续走翻页/字母追加分支——对应改造前"这几个 if 都不成立
  就自然往下掉"的既有控制流）。
- `RouteDigitKeyDirect`（kPlainPinyin 与 kWubi 共用）：数字键任何时候都直接
  `select_keys` 选字，不检查 `space_armed_`。

`VK_SPACE` 分支同样按 scheme 分叉：kSmartAbc 保留原有两段式逻辑（逐字不改）；
kPlainPinyin/kWubi 统一 `return SelectCandidate(0);`。

五笔编码长度上限 4 键：`raw_.size()>=4 && IsAsciiLetter(ch)` 时吃掉多余字母
不追加（`BuildViewResult(true)`），维持当前组字状态，而不是让 `raw_` 变成
永远查不到编码的死态。

### 5. `setConfig` 首次真正实现 + 协议版本 8→9

`Method::kSetConfig` 此前只是协议里的占位方法名（落到 `default:` 分支返回
"not implemented"）。本次实现 `{"input":{"method":"smartabc"|"pinyin"|"wubi"}}`
这一个 patch 键，切换全局方案（`SessionManager::SetSchemeForAll` ->
`Session::SetScheme`，正在组字的会话被强制 `ResetToIdle()`，语义等价
`CancelComposition()`，并推 `uiHide`）。语义从"未实现"变成"真正处理"本身
就是一次行为跳变，按不变量 7 递增 `kProtocolVersion`（8→9），即使 wire 上
方法名字段没变。

### 6. 持久化：小状态文件，不是 TOML 解析器

用户在 §9 决策点明确要求"方案选择需要持久化"（拒绝计划推荐的默认"不持久化"）。
`config_loader` 从 M1 起就没有真正的 TOML 解析（`docs/decisions/_debt-log.md`
2026-09-11），本次不为这一个字段单独实现完整解析器，改为一个只有一行内容的
小文件 `%APPDATA%\myabc\scheme-state.txt`（内容就是 method 字符串本身，如
`"wubi"`）：`Dispatcher::HandleSetConfig` 成功后落盘一次
（`scheme_state_store.hpp` 的 `SaveSchemeState`），`engine_main.cpp` 启动时
读一次覆盖 `cfg.input.method`（`LoadSchemeState`）。目录显式用
`CreateDirectoryA` 保证存在（不依赖 `engine.Init()` 创建 `user_data_dir` 的
副作用——真实默认配置下二者恰好同在 `%APPDATA%\myabc`，但 `--user-dir` 被
覆盖成其它路径时不应该依赖这层隐式关系，真机 e2e 测试中曾因此复现"状态文件
从未被创建"的问题，修正后已用真实进程重启验证通过）。文件不存在/内容不认识
一律静默回退到默认值，不报错、不崩溃。

### 7. v1 语言栏可见状态指示（用户拍板要求，覆盖计划的默认推荐）

计划 §4.2/§9 决策点 2 原本推荐 v1 只做"纯热键切换、无可见状态指示"（因为
`ITfLangBarItemButton` 是 0% 起点的全新 COM 工作量）。用户明确要求 v1 必须有
可见状态提示，本次按用户要求实现（tsf-service 侧新增 COM 实现，见对应文件
清单）。

## 被否决的备选方案

- **语言栏按钮作为唯一切换手段**：计划 §4.4 已论证否决——语言栏图标在很多
  现代应用（全屏/游戏/部分 UWP）经常被系统托管栏吞掉或不可见，不可靠；但按
  用户要求仍作为**状态指示**（非切换入口）的一部分实现，切换本身仍以热键为主。
- **按候选来源/scheme 对空格两段式做差异化处理**：`20260911-space-key-two-step-confirm.md`
  已经否决过一次（拼音 vs 数字/金额来源之间），本次延续同一条哲学——kSmartAbc
  内部不区分来源，kPlainPinyin/kWubi 统一直选，不引入更多特殊情形。
- **为五笔实现拆字算法/98 版数据**：用户任务描述已明确排除，数据源本身已提供
  预先算好的编码，不需要移植 rime 的 `encoder.rules`（那是给词典里没有的新词
  动态生成编码用的，不是查表逻辑需要的）。

## 影响范围

- `src/domains/input-engine/logic/candidate/candidate_source.hpp`：新增
  `InputScheme` 枚举 + `InputContext::scheme`。
- `src/domains/input-engine/logic/candidate/pinyin_candidate_source.cpp`、
  `number_currency_source.cpp`：`Handles()` 加 scheme 守卫。
- 新增 `src/domains/input-engine/backend/wubi_table.{hpp,cpp}`、
  `src/domains/input-engine/logic/candidate/wubi_candidate_source.hpp`。
- `src/domains/input-engine/logic/candidate/source_registry.{hpp,cpp}`：注册
  `WubiCandidateSource`，`BuildDefaultSourceRegistry` 新增 `wubi_table` 参数。
- `src/domains/input-engine/logic/session/session.{hpp,cpp}`：`SessionOptions::scheme`
  / `wubi_data_path`，`RouteDigitKeyPinyin`/`RouteDigitKeyDirect` 分派，
  `VK_SPACE` 按 scheme 分叉，`Recompute()` 自动选中规则排除 kWubi，新增
  `SetScheme()`。
- `src/domains/input-engine/logic/session/session_manager.{hpp,cpp}`：新增
  `SetSchemeForAll()`。
- `src/domains/input-engine/logic/dispatcher.{hpp,cpp}`：`wubi_table_` 成员，
  真正实现 `HandleSetConfig`，构造函数新增 `scheme_state_path` 参数。
- 新增 `src/domains/input-engine/backend/scheme_state_store.{hpp,cpp}`。
- `src/domains/input-engine/logic/engine_main.cpp`：`cfg.input.method` 映射
  `InputScheme`，加载 `wubi_data_path`，启动时读状态文件、显式创建
  `%APPDATA%\myabc` 目录。
- `src/config/config_defaults.hpp`：`InputConfig::method`（三值）、
  `method_switch_hotkey`。
- `src/shared/ipc-protocol/protocol.hpp`：`kProtocolVersion` 8→9。
- `assets/data/wubi86.txt`（新增数据资产）、`assets/README.md`、
  `scripts/stage.ps1`。
- `tests/unit/wubi_table_test.cpp`（新增，含合成表 + §3.2 已验证真实数据行）。
- tsf-service 侧（热键检测、语言栏可见状态、IPC 客户端 `setConfig` 封装）：
  见后续提交的对应文件清单（本文档随实现进度追加，不改上面已写好的部分）。

## 关联

- 代码引用：`src/domains/input-engine/logic/session/session.cpp`（多处
  `DECISION: docs/decisions/input-engine/20260913-wubi-input-scheme.md` 注释）
- 相关决策：
  `docs/decisions/input-engine/20260911-space-key-two-step-confirm.md`（本决策
  延续其"不按来源做差异化处理"的哲学，并新增"kWubi/kPlainPinyin 不受两段式
  限制"这一条新增而非修订的补充规则）
  `docs/decisions/input-engine/20260912-extension-candidate-provider.md`（同一批
  `CandidateSource` 适配器体系的前一次扩展先例）
- 计划文档：`docs/plan/08-wubi-input-scheme-plan.md`（§10 为最终权威版本，
  本决策文档记录的是该计划落地实施后的最终状态，含实施中发现并修正的
  W1 判据错误）
