# 08 · 输入方案选择：智能ABC / 普通拼音 / 五笔字型（86 版）计划

- 状态：生效（§9 三个决策点已由用户拍板：热键用默认 `Ctrl+Shift+W`；v1 需要
  语言栏可见状态；方案选择需要持久化。**用户随后又纠正了 §4.2/§4.3 的核心
  判断，实际是三种模式而非两种，见 §10 补充修订——请先读 §10 再看 §4/§5/§7，
  §10 是最终权威版本**）
- 更新日期：2026-09-13（§10 补充修订）
- 前置：M5（当前 HEAD；建议开工前打 tag `pre-wubi` 作回滚锚点）
- 对应需求：新增功能——`docs/requirements/smart-abc-ime-requirements.md` §4 当前明确把
  "五笔、双拼方案"列为非目标，本计划包含对该文档的修订建议（见 §8）
- 回滚锚点：`git tag pre-wubi`（开工前，本计划 §5 全部改动都应能通过 `git revert`/
  `git reset --hard pre-wubi` 完全撤销）

## 1. 背景与目标

用户要求："加入多个输入法方案的选择：普通拼音，五笔（需实现）"。拆成两件独立但耦合的事：

1. **五笔字型输入法从零实现**——myabc 目前只有拼音一条转换链路（libpinyin），没有任何
   五笔相关代码/数据/拆字逻辑。
2. **在"拼音"与"五笔"两个完全不同的输入方案之间切换**——myabc 目前没有任何设置面、
   没有语言栏 UI、没有运行期配置热加载，`config.toml` 的解析从 M1 起就是
   `TODO(M1+)`（`src/config/config_loader.cpp:33`），只有编译期默认值 + 启动时一次性
   加载（`config_loader.cpp:35-38` `Load()`）。

完成后的可观察效果：
- 用户可以在真机上通过一个热键（默认 `Ctrl+Shift+W`）在"拼音"与"五笔"之间切换，
  切换后当前及后续所有文本框的组字行为立即按新方案生效。
- 五笔模式下：键入字根编码（a-y，最多 4 键）能产出正确的单字/词组候选（数据来自
  真实开源五笔码表，见 §3），数字键 1-9 任何时候都直接选字（不需要先按空格），
  候选 ≤1 个时空格直接上屏、≥2 个时保留现有"空格两段式确认"机制（见 §4.3 的取舍
  说明）。
- 拼音模式下的全部既有行为（简拼/混拼、模糊音、笔形辅助码、i 数字金额、
  Ctrl+数字直选、**空格两段式确认**、用户自学习）**逐条验证零回归**——这是本计划
  优先级最高的红线，见 §7 验收判据。

## 2. 现状分析

### 2.1 架构与适配器点位
- `docs/architecture/system-overview.md` §6：`CandidateSource` 是已确立的"3+ 分支收敛
  为接口"适配器点位，现有实现 PinyinCandidateSource / NumberCurrencyCandidateSource /
  PunctuationSource / EnglishPassthroughSource / ExtensionCandidateSource（2026-09-12 新增，
  第 5 个）。新增候选来源 = 新增一个类 + `source_registry.cpp` 登记，不改分发逻辑
  （`source_registry.cpp:19-24` `Resolve()` 就是简单的"第一个 `Handles()==true` 独占胜出"
  循环）。
- §7 不变量 1/2/6：TIP（MSVC）不链接 libpinyin/glib；跨 MinGW/MSVC 边界只传字节级协议。
  五笔数据/查表逻辑必须放在 input-engine（MinGW 侧），不能碰 tsf-service。

### 2.2 `CandidateSource` 接口（`src/domains/input-engine/logic/candidate/candidate_source.hpp`）
- `InputContext`（14-35 行）目前字段：`mode`（kChinese/kEnglish）、`raw`、`composing`、
  `field_hint`（2026-09-12 新增，外部候选源用）。**没有"当前用的是拼音还是五笔"这个
  维度**——这正是本计划必须补的字段。
- `CandidateSource::Handles()`/`Produce()`/`AutoCommit()`/`UsesEngineChoose()` 四个虚函数
  （39-55 行）本身足够通用，**不需要新增虚函数**（见 §4.1 结论）。

### 2.3 各 `Handles()` 现状（会与五笔 raw 产生真实冲突的两处）
- `pinyin_candidate_source.cpp:13-15`：`return ctx.mode == InputMode::kChinese && !ctx.raw.empty();`
  —— 任何非空 raw 都会被拼音源接管。五笔 raw（如 `"aet"`）同样满足这个条件，若不加
  scheme 判断，会直接被拼音源"抢走"（因为它在 `source_registry.cpp:34` 最后一个加入，
  但 `Resolve()` 是"谁先 Handles()==true 谁赢"，不是"谁加得晚谁赢"，实际以后加入的
  WubiCandidateSource 放在它之前反而会让拼音失效）——**必须显式加 scheme 守卫**，不能
  靠加入顺序侥幸避免冲突。
- `number_currency_source.cpp:27-31`：`raw[0] == lead_key_`（默认 `'i'`，见
  `config_defaults.hpp:81`）。`i` 恰好也是合法的五笔字根键（数据里 `工期` 的码是
  `aaad`，`w`=人、`r`=的、`a`=工，`i` 同样是 25 个有效字根键之一）——五笔模式下打
  `i` 开头的编码必须走 WubiCandidateSource，不能被数字模式误吞。**必须显式加 scheme
  守卫**（即使当前数字路由重构后 raw_ 里理论上不会出现数字导致这条路径大概率不会真
  的触发，也不能靠"隐式不共存"这种跨文件的默认假设——这类假设正是本项目 debt-log
  里反复出现的真 bug 根源，见 2026-09-11"M4 遗留真 bug"条目）。
- `punctuation_source.cpp:40-43`、`english_passthrough_source.cpp:9-11`：分别要求
  `raw.size()==1` 且是标点、或 `mode==kEnglish`——都不会跟五笔的纯字母 raw 冲突，
  **不需要改**。

### 2.4 `Session` 组字态按键路由（唯一目标级别的不变量所在地）
`src/domains/input-engine/logic/session/session.hpp` / `session.cpp`：
- `session.hpp:27-46` `SessionOptions`：`number_lead_key`、`bihuo_enabled`、
  `bihuo_data_path` 是拼音专属配置，随 `Dispatcher` 构造函数（`dispatcher.cpp:22-34`）
  一次性传入所有 Session 共享。**没有"当前方案"这个维度**。
- `session.cpp:28-136` `ProcessKey()`：组字态下的完整按键路由链，从上到下依次是：
  1. `35-41` Ctrl+数字：任何时候直接 `select_keys`（2026-09-12 决策，见
     `docs/decisions/input-engine/20260911-space-key-two-step-confirm.md` "补充
     （2026-09-12，Ctrl+数字直选）"）。
  2. `43-55` VK_BACK / VK_ESCAPE。
  3. `57-72` VK_SPACE 两段式确认（**唯一目标**，见
     `docs/decisions/input-engine/20260911-space-key-two-step-confirm.md`）：
     `candidates_.size()<=1` 直接选中；否则第一次架住、第二次才选中。**这段代码本身
     不引用任何拼音专属字段，天然 scheme-agnostic**，是本计划唯一可以完全不碰、直接
     复用给五笔的分支。
  4. `74-89` 数字模式（`raw_.front()==number_lead_key`）：拼音专属，五笔必须绕开。
  5. `91-100` 笔形辅助码（`bihuo_enabled` 时数字 1-5 追加）：拼音专属，五笔必须绕开。
  6. `102-108` 数字选择状态（`space_armed_` 之后数字才是 `select_keys`）：**这条正是
     背景条目 7 提到的冲突点**——五笔里数字键从来不需要先"架住"才生效。
  7. `110-118` 翻页键 / 字母追加：翻页天然 scheme-agnostic；字母追加
     （`raw_ += ToLowerAscii(ch)`）目前没有长度上限，五笔编码最长 4 键，需要加上限。
- `session.cpp:138-180` `SelectCandidate()`：`current_source_uses_engine_choose_` 已经
  把"是否要查 libpinyin 内部数组"抽象成一个 bool 分流（M5 前修的真 bug，见
  `docs/decisions/_debt-log.md` 2026-09-11"M4 遗留真 bug"条目）——**五笔候选走
  `UsesEngineChoose()==false` 这条已有分支，跟 `NumberCurrencySource` 完全一样，不需要
  改这个函数一行代码**。
- `session.cpp:225-253` `Recompute()`：`InputContext ctx{InputMode::kChinese, raw_,
  composing_, field_hint_};`（228 行）——需要补第 5 个字段（当前方案）。

结论：**空格两段式确认 / 翻页 / SelectCandidate 的 engine-choose 分流三处完全不用改**；
真正需要按 scheme 分叉的只有"数字键怎么解释"和"raw_ 长度上限"两小块，且都集中在
`ProcessKey()` 一处，改造面比看起来小得多。

### 2.5 config 现状（背景条目 6 要求先搞清楚）
`src/config/config_defaults.hpp:70-87` `InputConfig::scheme` 当前语义：
`"hunpin"`（M3 起默认）/`"quanpin"`/`"jianpin"`，**只管拼音内部的严格度**（是否接受
简拼/混拼，见 `engine_main.cpp:88-93` `ApplySchemeAndFuzzy()`：
`incomplete = cfg.input.scheme != "quanpin"`）。跟本计划要新增的"拼音 vs 五笔"这种
输入法大类切换是两个完全不同维度的概念，**绝不能复用同一个字段**，否则
`"scheme":"wubi"` 和 `"scheme":"quanpin"` 会变成同一个字符串槽位里语义互斥的两套值，
后续任何人读这行配置都会先猜错一次。本计划新增独立字段 `InputConfig::method`
（见 §5.6）。

### 2.6 运行期配置现状（背景条目 3 要求先确认）
`src/config/config_loader.cpp:35-38`：
```
Config Load(...) {
    // TODO(M1+): 若 %APPDATA%\myabc\config.toml 存在，解析并覆盖 Defaults()。
    return ExpandPlaceholders(Defaults(), current_user_sid, appdata_dir);
}
```
**从 M1 到现在，TOML 从未真正被解析过**——`assets/config/config.sample.toml` 只是样例
文件（`assets/README.md:14`）。这意味着：即使是现有的拼音内部方案
（quanpin/jianpin/hunpin），今天也**没有任何运行期切换手段**，只能改编译期默认值
重新编译，或者等 TOML loader 真正落地。本计划面对的"如何在不启动 TOML 解析器的
前提下做到运行期可切换"这个问题，在 myabc 历史上从未被解决过，没有现成机制可抄，
必须新建一条独立的、不依赖 TOML 的切换通路（见 §4.2）。

### 2.7 语言栏 / 热键现状
- `src/domains/tsf-service/logic/mode_manager.hpp/.cpp`：中/英文切换是**纯 TIP 本地
  状态机**（Shift 单击手势检测，`mode_manager.cpp:12-30`），因为英文模式下的行为差异
  简单到"字母直接放行，引擎完全不知情"（`session.hpp:10-12` DECISION 注释），不需要
  IPC 往返。**这个先例不能直接照抄给拼音/五笔切换**——五笔和拼音的组字状态机差异
  （raw_ 累积规则、候选生成、数字键语义）复杂到必须由引擎侧的 `Session` 决定，
  纯 TIP 本地状态机做不到。
- 语言栏按钮（`ITfLangBarItemButton`）：`docs/architecture/system-overview.md` §3.1
  提到"ui/：语言栏按钮（ITfLangBarItem）等 TIP 自身的极简 UI"，但检索
  `src/domains/tsf-service` 全目录**没有任何 `ITfLangBarItem*` 实现**——这是一个 0%
  起点的全新 COM 工作量，不是"补几行"的量级。
- `key_router.cpp:29-51` `IsInterestedKey()`：本地快速判断"这个键值不值得问引擎"，
  目前完全没有"非组字态下识别一个全局热键组合"的逻辑（Shift 检测在
  `myabc_text_service.cpp` 里跟 `key_router` 平行、独立于 `IsInterestedKey`
  判断之外直接处理，见 `myabc_text_service.cpp:250-252` `OnKeyDown` 里先调
  `mode_manager_.OnKeyDown()`）。新热键要复用这个"平行于 key_router、直接在
  `OnKeyDown`/`OnKeyUp` 里处理"的现有模式，而不是塞进 `IsInterestedKey`。

## 3. 技术选型与数据来源调研

### 3.1 版本选型：86 版 vs 98 版

结论：**86 版**。理由（联网调研结果）：
- 98 版虽然码元更规范（245 码元 vs 86 版 130 字根）、重码率更低、覆盖繁体+中日韩
  大字符集，但发布方王码公司对 98 版长期维持专利/授权限制，绝大多数免费/开源
  输入法（包括本计划要用的数据源）都只提供 86 版数据，98 版几乎找不到高质量开源
  码表。
- 86 版是事实上的社区标准——历史更久、安装基数更大、开源数据源成熟。
- 与本项目现有笔形辅助码（智能ABC风格，`docs/decisions/_debt-log.md` 2026-09-12
  "笔形辅助码数据表"条目）气质一致：先求"能用、可信、开源"，不追求最先进版本。

### 3.2 候选数据源调查（方法论同 2026-09-12 笔形辅助码调研：不满足于 WebFetch 的转述
摘要，用 `curl` 拉原始文件自己解析，避免二手转述引入错误）

| 数据源 | License | 说明 | 取舍 |
|---|---|---|---|
| **`rime/rime-wubi` 的 `wubi86.dict.yaml`**（`github.com/rime/rime-wubi`） | **LGPL-3.0**（repo 根 LICENSE 文件确认） | 官方 RIME 组织维护，源自"极点五笔"（Jidian Wubi）码表，经 Yu Yuwei / Chen Xing / Gong Chen 转换为 Rime 词典格式（文件头 Changelog 明确记录：Original table author Wozy，Update to Jidian 6）。实测下载全文件（136,981 行）分析：单字条目 32,680 行（含大量 CJK 扩展区罕见字）、双字词 48,453 行、三字词 4,518 行、四字词 7,934 行。格式：`text\tcode\tweight[\tstem]`，Tab 分隔，按权重（真实语料词频）排序。**采用** |
| `yekingyan/rime-wubi-86-single` | 未逐一核实（同类 Rime 五笔码表的社区分支） | "single" 版通常指单文件精简版，未详细比对 | 备选，若主数据源后续出问题可比对交叉验证 |
| `zhmars/rime-wubi-simp` | 未逐一核实 | 简体定制版 | 备选 |
| `lxgw/wubi86-super` | 未逐一核实 | 支持 Unicode 14.0 扩展字符集的加强版 | 覆盖面可能更大但引入更多生僻扩展区字符，增加转换脚本复杂度，本计划不需要这么大的覆盖面 |
| 王码 98 版官方码表 | 未找到可信开源来源 | 见 §3.1 专利限制 | **不采用** |
| 自建拆字算法 + 字根表现算编码 | N/A | 用户任务描述已明确排除（"不需要自己实现拆字算法"） | **不采用**——拆字规则本身歧义多、极易出错，是五笔实现里最难做对的部分，直接用现成"字/词->编码"映射表规避这个风险 |

**验证方法**（不是转述，是自己下载原始文件后用真实数据核对）：
```
curl -sL https://raw.githubusercontent.com/rime/rime-wubi/master/wubi86.dict.yaml -o wubi86.dict.yaml
```
抽查确认的真实数据行（后续 §7 验收判据直接复用这些真实行，不编造）：
```
工	a	99454797	aa      <- 单键字根字（键名字）
人	w	1002212710	ww
工期	aaad	5350000     <- 双字词，编码已经算好，不需要再跑任何拆字/编码算法
散	aet	96000000        <- 同码字示例 1/2（同一个编码 "aet" 有两个候选）
㣉	aet	4190            <- 同码字示例 2/2，权重远低于"散"
```
这直接证明了：(1) 词组编码已经是词典里现成的数据，不需要实现"word encoding rules"
（yaml 头部 `encoder.rules` 那段是 Rime 部署时用来**给词典里没有的新词**动态生成编码
用的，我们只取已经算好编码的存量数据，不需要移植这段算法）；(2) 同码字（重码）现象
真实存在，验证了 §4.3 讨论的"数字键选重码字"场景不是臆想。

**License 结论**：LGPL-3.0 与 myabc 整体 GPL-3.0-or-later 完全兼容（LGPL 是比 GPL 更
宽松的弱 Copyleft，GPL 项目吸收 LGPL 内容不受限；况且这里只是把数据转换后作为
资产文件收录，不是链接一个 LGPL 库）。参照现有 `assets/data/bihuoma.txt` 的做法，
在数据文件头 + `assets/README.md` 完整记录来源/License/转换规则/原始 Changelog 归属
（不能只写"来自 rime"，要把 Wozy/Yu Yuwei/Chen Xing/Gong Chen 的贡献链路一并注明，
尊重上游署名）。

### 3.3 字根表（25 键字根图）是否需要

**不需要**作为运行时数据——本计划采用"现成字/词->编码映射表直接查表"的架构（用户
任务描述已经定的方向），字根表只在"如何把汉字拆解成编码"这个正向过程中用得上，
而我们拿到的是已经算好的反向映射（编码->字/词），运行时只做精确查表，不做拆字。
字根图**可以**作为 v2 的"帮助/速查面板"UI 素材（用户不知道某个字根在哪个键位时
查阅），但这是纯展示型可选项，不影响输入功能，本计划标记为**不在 MVP 范围内**
（见 §6 不改动清单）。

## 4. 架构决策

### 4.1 候选生成放哪：CandidateSource 新实现（选 (a) 的候选生成部分）

**结论**：五笔候选生成走**现有 `CandidateSource` 适配器**，新增一个
`WubiCandidateSource` 类，**不改 `CandidateSource` 接口本身**（`Handles`/`Produce`/
`AutoCommit`/`UsesEngineChoose` 四个虚函数原样够用）。

理由：
- `UsesEngineChoose()` 默认 `false` 这条路径已经是为"跟 libpinyin 无关的原子候选来源"
  设计的（`NumberCurrencySource` 就是这个模式，见 `candidate_source.hpp:49-54` 注释），
  五笔候选跟 libpinyin 完全无关，天然吻合这条已有分支，`Session::SelectCandidate`/
  `CommitComposition` 一行都不用改（§2.4 已论证）。
- 五笔的"选中即整句提交"（没有拼音那种"部分确认、继续下一段"的分段概念——五笔一个
  编码要么对应完整的字/词，没有 libpinyin 式的整句路径搜索）跟 `AutoCommit()==false`
  + `UsesEngineChoose()==false` 的组合完全匹配现有语义，不需要新增任何标志位。
- 这就是 code-quality-standards §3.2 说的"新增一种实现 = 新增一个类 + 注册表登记，
  不允许修改已有分发逻辑"的标准场景，`CandidateSource` 现在有 5 个实现
  （pinyin/number/punctuation/english/extension），加上 wubi 变成 6 个，完全在这条
  适配器边界的设计意图之内。

### 4.2 按键路由放哪：Session 内按 scheme 分叉（选 (b)，但用最小化实现，不建 3+ 适配器接口）

**结论**：`Session::ProcessKey` 组字态下"数字键怎么解释"这一小块逻辑按 `opts_.scheme`
拆成两个私有方法分派，**不**为此新建一个通用的 `KeyRoutePolicy` 接口类。

理由（回应用户任务里 (a)/(b) 的判断要求）：
- 五笔和拼音在"翻页/回退/取消/空格两段式/字母追加/EngineChoose 分流"这些方面
  **完全一致**，真正不同的只有"数字键在架住候选之前该不该被当续写字符"这一件事
  （§2.4 结论）——差异面小到不值得为此抽象一整层接口。
- `code-quality-standards.md` §3.2 的强制适配器线是"3 个以上具体实现"，当前只有 2 个
  scheme（拼音/五笔），达不到强制阈值。用户任务本身要求的也不是"预留无限多种输入
  方案"，是具体的"拼音 + 五笔"两个。为 2 个变体建一整套接口层（虚函数、注册表、
  组合类）是过度设计，会让本来几十行的改动膨胀成一套新框架，违反"改造面越小、
  回归风险越低"的原则。
- 但也不能像用户任务警告的那样把两套数字键语义"简单粗暴塞进同一条分支链条"——
  本计划的做法是：`ProcessKey` 组字态分支链条里，凡是"数字键怎么解释"这一段，
  **统一收口成一次 `if (opts_.scheme == InputScheme::kWubi) return RouteDigitKeyWubi(...);
  else` 走原有的 in_number_mode/bihuo/select_keys 三段逻辑（原样搬进
  `RouteDigitKeyPinyin`，不改一行判断本身）**——两套语义物理上分离到两个函数里，
  各自独立可读、互不干扰，只有一个分派点，不是"到处插 if scheme==xxx"式的散弹式
  改动。
- **升级路径留白**：如果未来真加入第三种编码类输入方案（如仓颉、郑码——不包括
  双拼，双拼仍属于"拼音家族"，走 `InputConfig::scheme` 而非 `method`），且新方案的
  按键语义又跟五笔/拼音都不同，届时 `RouteDigitKeyXxx` 已经三个以上，再按
  code-quality-standards §3.2 的强制线提升为正式的 `KeyRoutePolicy` 接口。本计划在
  `session.cpp` 对应位置留一条注释说明这个升级触发条件（做法同项目里其它"未达
  3+ 门槛暂不建接口"的既有先例，如 `docs/decisions/_debt-log.md` 2026-09-11
  "M5：`SelfDir`/`CurrentUserSid`/`AppDataDir` 第三份重复实现"条目的处理方式）。

### 4.3 空格两段式确认 + 数字键选字：五笔模式下的具体设计（回应背景条目 4/7）

**数字键（`select_keys`，默认 `"123456789"`）：五笔模式下任何时候都直接选字，
不需要先按空格"架住"。**

理由：
- 真实五笔的标准操作习惯就是"重码直接按数字 1-4 选"，不存在拼音场景里"数字键
  在架住之前是笔形码/金额续写"这种双重语义——五笔的数字键从第一天起就只有一个
  含义：选第几个候选。把 `space_armed_` 门槛套在五笔数字键上，是把拼音场景的
  特殊设计（因为拼音数字键要跟笔形码/数字模式复用同一批按键，才不得不引入"架住"
  这个中间态来消歧）错误地照搬到一个根本不存在这种复用冲突的场景，只会让熟悉
  五笔的用户觉得"输入法反应迟钝，明明按了数字怎么没反应，还要再按一次空格"。
- 用真实数据验证过重码场景真实存在（`aet` -> 散/㣉，见 §3.2），数字键选字是五笔
  用户高频操作，不能人为拖慢。
- `Ctrl+数字直选`（`session.cpp:35-41`）这条现有分支在五笔模式下变成"永远生效但
  跟不按 Ctrl 时效果相同"的冗余路径——**不删除它**，保持这个分支 scheme-agnostic、
  统一生效（不为它加 `if scheme==pinyin` 排除），因为它仍然正确、无害，删除它反而
  要多写一次判断，属于不必要的特殊化。

**空格键：沿用现有的、完全通用的两段式确认逻辑，不做特殊豁免。**

理由（直接回应用户任务"五笔重码候选经常只有 1-2 个，两段式确认的价值大不大"）：
- `candidates_.size()<=1` 时空格直接选中——这条已有规则天然覆盖"五笔多数编码只有
  一个候选"的最常见情况，两段式的"额外一次按键"成本在这种情况下**根本不会发生**，
  不需要专门为五笔关掉这套机制。
- 当五笔真的出现重码（`candidates_.size()>=2`，如 `aet`）时，两段式确认仍然提供
  同样的价值——防止手滑；而且五笔重码用户的**主流操作方式本来就是直接按数字
  选**（上面已决定数字键不受两段式门槛限制），两段式只是给"习惯用空格确认"的
  用户留的备用路径，两者并行不悖：数字键快速用户完全绕开两段式，空格用户享受
  跟拼音一致的防误触保护。
- 决策文档 `docs/decisions/input-engine/20260911-space-key-two-step-confirm.md` 本身
  的"被否决方案"一节已经明确记录"按候选来源做差异化处理"被否决过一次（当时是
  拼音 vs 数字/金额来源之间），本计划延续同一条哲学，不给五笔单独开豁免口子——
  **不区分方案，是本项目从 M4 起就贯彻的既定简单性原则**，本计划继续遵守，不引入
  新的特殊情形。

**五笔编码长度上限（4 键）：**
真实五笔编码最长 4 键（86 版规则），当前 `ProcessKey` 的字母追加分支
（`session.cpp:114-117`）对拼音没有长度上限（拼音音节可以很长，如
"zhuang"）是正确的，但直接套用给五笔会导致打第 5 个字母时 `raw_` 变成
5 字符、永远查不到任何编码（真实五笔编码不超过 4 位）。五笔模式下第 5 个及之后
的字母键需要被"吃掉但不追加"（`return BuildViewResult(true)`，维持当前组字状态
不变），而不是追加后落到"候选为空"的组字态卡死表现。

### 4.4 方案切换机制（回应背景条目 3）

**结论**：**热键（TIP 本地检测触发） + 新增/激活 IPC `setConfig` 方法**，
**不做**语言栏按钮，**不依赖** `config.toml` 运行期解析。

| 候选方案 | 取舍 |
|---|---|
| **热键 + `setConfig`（推荐）** | 复用 `ModeManager`（Shift 单击切中英文）的"TIP 本地状态机检测手势"先例，但因为五笔/拼音的组字状态机差异复杂到必须由引擎侧决定（§2.7 已论证），检测到手势后经 IPC 通知引擎真正切换，UI（语言栏图标/候选窗提示）留到 v2。改造量：TIP 侧一个新的手势检测类（结构完全照抄 `mode_manager.hpp/.cpp`）+ `IpcClient` 一个新方法 + 引擎侧激活 `setConfig`（协议里已经预留了方法名和占位分支，见下）。 |
| 语言栏菜单（`ITfLangBarItemButton`） | **否决**：0% 起点的全新 COM 工作量（§2.7 已确认现状完全没有任何 `ITfLangBarItem*` 实现），实现成本与本计划核心目标（五笔功能本身）不成比例；且语言栏图标对大多数现代应用（尤其是全屏/游戏/某些 UWP 应用）经常被系统托管栏吞掉或不可见，作为唯一切换手段不可靠。**标记为可选的 v2 增强**，不阻塞本计划交付。 |
| 配置文件（`config.toml`） | **否决**：myabc 从未真正实现运行期 TOML 解析（§2.6），把切换做成"改配置文件 + 重启引擎才生效"体验上完全不是"切换"，是"重新配置"，不满足用户"选择输入法方案"这种交互预期（对比现实世界任何一个双语/双方案输入法，都是热键/菜单即时切换，不需要重启）。 |

**协议落地**：`src/shared/ipc-protocol/protocol.hpp:42` `kProtocolVersion` 现为 8，
`protocol.hpp:72` 已经声明了 `kSetConfig`（`protocol.cpp:26` 也已登记方法名
`"setConfig"`），但 `dispatcher.cpp:36-77` 的 `Handle()` switch **没有 `case
Method::kSetConfig`**，会落到 69-75 行的 `default:` 分支返回
`"method not implemented: setConfig"`。本计划**首次真正实现 `setConfig`**（值域先只
定义 `{"input":{"method":"pinyin"|"wubi"}}` 这一个 patch 键，其余 `setConfig` 用法
留作后续），并把 `kProtocolVersion` 从 8 升到 9（不变量 7：新语义生效必须递增版本号，
即使 wire 上方法名字段没变，语义从"未实现"变成"真正处理"本身就是一次行为跳变，
按项目一贯做法应当升版本号，参照 M5 新增 userDictExport 系列方法时的处理）。

**方案粒度：全局（每个引擎进程/每用户会话一个），不是每个文本框独立。**
理由：现有架构里 `LibPinyinEngine engine_`、`SourceRegistry registry_` 本来就是整个
`myabc-engine.exe` 进程唯一一份、被所有 `Session`（每个文本框一个）共享
（`dispatcher.hpp:76-78`、`session_manager.hpp:6-8` DECISION 注释），切换方案沿用
同一个"进程级全局状态"的既有设计惯性最省事，也最符合用户对"我把输入法切成五笔了"
的直觉理解（不会有"这个输入框还是拼音，那个输入框已经是五笔"这种反直觉的按框
区分）。**正在组字中的文本框**在切换瞬间会被强制取消组字（丢弃未上屏的半成品，
不训练、不上屏——语义等价于当前已有的 `CancelComposition()`），这是所有主流输入法
切换方案/切换中英文时的标准行为，用户能理解。

**持久化：本计划标记为待用户决定的开放问题，见 §9 决策点 3**（默认实现不持久化，
每次 `myabc-engine.exe` 重启回到 `method=pinyin`）。

## 5. 文件级改造点

### 5.1 `src/domains/input-engine/logic/candidate/candidate_source.hpp`
现状（23-35 行）：
```cpp
enum class InputMode { kChinese, kEnglish };

struct InputContext {
    InputMode mode = InputMode::kChinese;
    std::string raw;
    bool composing = false;
    std::string field_hint;
};
```
目标：新增 `InputScheme` 枚举 + `InputContext::scheme` 字段（用显式默认值写法，
沿用 `CandidateItem::commit_text` 那条注释里提到的"聚合初始化省略字段不产生
`-Wmissing-field-initializers` 警告"的既有约定）：
```cpp
enum class InputMode { kChinese, kEnglish };
enum class InputScheme { kPinyin, kWubi };   // 新增：拼音 vs 五笔，跟 InputMode 是正交维度

struct InputContext {
    InputMode mode = InputMode::kChinese;
    std::string raw;
    bool composing = false;
    std::string field_hint;
    InputScheme scheme = InputScheme::kPinyin;   // 新增
};
```
逻辑说明：`CandidateSource` 接口本身（39-55 行）不改一行。

### 5.2 `src/domains/input-engine/logic/candidate/pinyin_candidate_source.cpp`
现状（13-15 行）：
```cpp
bool PinyinCandidateSource::Handles(const InputContext& ctx) const {
    return ctx.mode == InputMode::kChinese && !ctx.raw.empty();
}
```
目标：
```cpp
bool PinyinCandidateSource::Handles(const InputContext& ctx) const {
    return ctx.mode == InputMode::kChinese && ctx.scheme == InputScheme::kPinyin &&
           !ctx.raw.empty();
}
```
逻辑说明：必需的正确性修复（§2.3 已论证冲突真实存在），不是防御性可选项。

### 5.3 `src/domains/input-engine/logic/candidate/number_currency_source.cpp`
现状（27-31 行）同上模式（`ctx.mode != kChinese || raw.size()<2` 提前返回，
`raw[0]!=lead_key_` 提前返回）。
目标：函数开头加一行 `if (ctx.scheme != InputScheme::kPinyin) return false;`。
逻辑说明：数字/金额是拼音输入流的专属约定（`i` 引导），五笔模式下完全不适用
（§2.3 已论证 `i` 是合法五笔字根键，存在真实碰撞风险）。

### 5.4 新增 `src/domains/input-engine/backend/wubi_table.hpp` / `.cpp`
仿照 `bihuoma_table.hpp/.cpp` 的既有模式（`LoadFromFile`/`LoadFromText`/安全空表）：
```cpp
class WubiTable {
public:
    bool LoadFromFile(const std::string& path);   // 找不到文件：静默返回 false，表保持空
    void LoadFromText(const std::string& tsv_text);   // 供单测用

    // code：小写字母编码（1-4 位）。未收录返回空 vector（不是错误，调用方按"组字中
    // 无候选"处理，见 session.cpp Recompute 既有的 candidates_.empty() 分支）。
    // 返回值已按权重降序排列。
    std::vector<std::string> Lookup(const std::string& code) const;

    std::size_t size() const noexcept;

private:
    // code -> [(text, weight)]，LoadFromFile/LoadFromText 里加载完一次性按 weight
    // 降序排序，Lookup 直接返回已排序结果（避免每次查询都排序）。
    std::unordered_map<std::string, std::vector<std::pair<std::string, long long>>> table_;
};
```
文件格式（`assets/data/wubi86.txt`，Tab 分隔，`#` 开头注释行，格式仿
`bihuoma_table.hpp:28` 的既有约定）：`<code>\t<text>\t<weight>`。
逻辑说明：这是五笔专属的新数据结构，跟 `BihuoTable`（字->笔形码，单一映射）方向
相反（编码->候选字/词列表，一对多），不能复用 `BihuoTable` 本身，但复用它"安全
空表/独立于文件系统的 LoadFromText 供单测"这套既有设计模式。

### 5.5 新增 `src/domains/input-engine/logic/candidate/wubi_candidate_source.hpp` / `.cpp`
```cpp
class WubiCandidateSource final : public CandidateSource {
public:
    explicit WubiCandidateSource(const WubiTable& table) : table_(table) {}

    bool Handles(const InputContext& ctx) const override {
        return ctx.mode == InputMode::kChinese && ctx.scheme == InputScheme::kWubi &&
               !ctx.raw.empty();
    }
    std::vector<CandidateItem> Produce(const InputContext& ctx) override {
        std::vector<CandidateItem> out;
        for (const auto& text : table_.Lookup(ctx.raw)) {
            out.push_back(CandidateItem{.text = text, .is_sentence = false});
        }
        return out;   // 空 vector 是合法结果（编码暂无匹配，组字继续，不报错）
    }
    // UsesEngineChoose() 用默认值 false（不重写）——候选是跟 libpinyin 无关的原子
    // 候选列表，选中即整句提交，跟 NumberCurrencySource 走同一条既有分支（见 §4.1）。

private:
    const WubiTable& table_;
};
```
逻辑说明：`Produce()` 目前只做精确码匹配（不做"前缀提示未完成编码"这类增强，
见 §6 不改动清单的 MVP 范围裁剪）。

### 5.6 `src/domains/input-engine/logic/candidate/source_registry.cpp`
现状（26-36 行）`BuildDefaultSourceRegistry` 签名与实现。
目标：新增形参 `const WubiTable& wubi_table`，函数体内新增一行
`reg.Add(std::make_unique<WubiCandidateSource>(wubi_table));`（加入位置在 Pinyin 之前
或之后均可，因为两者 `Handles()` 已按 scheme 互斥，顺序不影响正确性——放在 Pinyin
旁边即可，保持代码可读性）。

### 5.7 `src/domains/input-engine/logic/session/session.hpp`
现状（27-46 行）`SessionOptions`；私有成员（104-123 行）。
目标：
- `SessionOptions` 新增 `InputScheme scheme = InputScheme::kPinyin;`。
- 新增公开方法：`bool SetScheme(InputScheme scheme);`（返回值：这次切换之前会话是否
  正处于组字态——供 `Dispatcher` 决定要不要主动 `PushHide`，语义/签名风格参照现有
  `bool ModeManager::OnKeyUp(int vk)` 用 bool 传递"是否发生了一次有效切换"这一惯例）。
- `ProcessKey` 内部新增两个私有方法声明：
  `SessionResult RouteDigitKeyPinyin(char c, bool ctrl);`（照搬现状 §2.4 第 4/5/6 点
  的既有逻辑，纯剪切，不改判断本身）、
  `SessionResult RouteDigitKeyWubi(char c);`（新逻辑，见 §5.8）。

### 5.8 `src/domains/input-engine/logic/session/session.cpp`
1. `Recompute()`（225-253 行）：228 行的 `InputContext ctx{...}` 补第 5 个字段：
   ```cpp
   const InputContext ctx{InputMode::kChinese, raw_, composing_, field_hint_, opts_.scheme};
   ```
2. `ProcessKey()`（28-136 行）：
   - 第 74-108 行（数字模式 + 笔形辅助码 + 数字选择状态三段）整体替换为：
     ```cpp
     if (ch != 0) {
         const char c = static_cast<char>(ch);
         if (c >= '0' && c <= '9') {
             return opts_.scheme == InputScheme::kWubi ? RouteDigitKeyWubi(c)
                                                        : RouteDigitKeyPinyin(c, ctrl);
         }
     }
     ```
     注意：原 35-41 行"Ctrl+数字任何时候直接选字"分支**保持在最前面、不改**（两个
     scheme 都受益，§4.3 已说明五笔下这条分支变成冗余但无害，不删除、不特殊化）；
     `RouteDigitKeyPinyin` 接收 `ctrl` 参数只是为了保持函数签名跟原语义一致，其内部
     不需要再判断 ctrl（因为 ctrl 分支已经在更早处拦截掉，走不到这里）——如果实现
     时发现这个参数完全用不上，可以去掉，不强制保留。
   - 原 91-100 行的笔形逻辑、82-89 行数字模式逻辑、102-108 行选择状态逻辑，原样
     移入新方法 `RouteDigitKeyPinyin`（纯剪切+封装，不改判断条件本身，这是保证
     "拼音模式零回归"的关键约束——**逐字对照迁移，不得在迁移过程中"顺手"简化或
     调整任何一个既有判断条件**）。
   - 新增 `RouteDigitKeyWubi`：
     ```cpp
     SessionResult Session::RouteDigitKeyWubi(char c) {
         const auto pos = opts_.select_keys.find(c);
         if (pos != std::string::npos) return SelectCandidate(static_cast<int>(pos));
         return BuildViewResult(false);   // 未映射的数字键（如超出 select_keys 范围）：
                                          // 不吃，交还宿主，跟其它"不认识的键"一致处理
     }
     ```
     **不检查 `space_armed_`**——这正是 §4.3 的核心结论。
   - 第 114-117 行字母追加分支前新增五笔长度上限守卫：
     ```cpp
     if (opts_.scheme == InputScheme::kWubi && raw_.size() >= 4 && IsAsciiLetter(ch)) {
         return BuildViewResult(true);   // 五笔编码最长 4 键，多余字母吃掉不追加
     }
     if (ch != 0) {
         const char c = static_cast<char>(ch);
         if (opts_.page_prev_keys.find(c) != std::string::npos) return PageCandidates(-1);
         if (opts_.page_next_keys.find(c) != std::string::npos) return PageCandidates(+1);
         if (IsAsciiLetter(ch)) {
             raw_ += ToLowerAscii(ch);
             return Recompute();
         }
     }
     ```
   - 43-72 行（VK_BACK/VK_ESCAPE/VK_SPACE）**逐字保持不变**。
3. 新增 `Session::SetScheme()`：
   ```cpp
   bool Session::SetScheme(InputScheme scheme) {
       if (opts_.scheme == scheme) return false;
       const bool was_composing = composing_;
       if (composing_) ResetToIdle();   // 丢弃半成品，不训练、不上屏，同 CancelComposition 语义
       opts_.scheme = scheme;
       return was_composing;
   }
   ```

### 5.9 `src/domains/input-engine/logic/session/session_manager.hpp` / `.cpp`
现状（21-38 行 / 全文件）：`opts_` 是构造 `Session` 用的模板，无遍历接口。
目标：新增
```cpp
// 返回值：切换前正处于组字态的 session id 列表（Dispatcher 用来对这些 id 主动推 uiHide）。
std::vector<std::uint32_t> SetSchemeForAll(InputScheme scheme);
```
实现：更新 `opts_.scheme = scheme`（影响后续 `GetOrCreate` 新建的 Session），并遍历
`sessions_` 对每个调用 `session->SetScheme(scheme)`，收集返回 true 的 id。

### 5.10 `src/domains/input-engine/logic/dispatcher.hpp` / `.cpp`
- `dispatcher.hpp`：新增私有成员 `WubiTable wubi_table_;`（跟 `bihuo_table_` 同级，
  76-78 行附近）；新增 `ipc::Response HandleSetConfig(const ipc::Request& req);` 私有
  方法声明。
- `dispatcher.cpp` 构造函数（22-34 行）：新增
  `if (!opts.wubi_data_path.empty()) wubi_table_.LoadFromFile(opts.wubi_data_path);`
  （紧跟 31 行 bihuo 加载之后），`BuildDefaultSourceRegistry` 调用新增 `wubi_table_`
  实参（对应 §5.6 的签名变化）。
- `Handle()`（36-77 行）switch 新增：
  ```cpp
  case Method::kSetConfig:
      return HandleSetConfig(req);
  ```
- 新增 `HandleSetConfig` 实现：解析 `req.params["input"]["method"]`（缺失/空
  字符串：返回 `Ok`+空 `applied`，视为"这次 patch 没有本方法认识的键"，不报错——
  为未来 `setConfig` 扩展到其它 patch 键留出空间）；值只接受 `"pinyin"`/`"wubi"`，
  其它值返回 `Response::Err(kInvalidParams, ...)`；调用
  `const auto affected = sessions_.SetSchemeForAll(scheme);`，对 `affected` 里每个 id
  执行 `pending_ui_.erase(id); if (ui_bridge_) ui_bridge_->PushHide(id);`（照抄
  `HandleFocusOut`，132-133 行的既有写法）。

### 5.11 `src/domains/input-engine/logic/engine_main.cpp`
- `ToSessionOptions()`（95-109 行）新增：
  ```cpp
  opts.scheme = (cfg.input.method == "wubi") ? myabc::engine::InputScheme::kWubi
                                              : myabc::engine::InputScheme::kPinyin;
  opts.wubi_data_path = SelfDir() + "\data\wubi86.txt";
  ```
  （与 105 行 `opts.bihuo_data_path` 同一模式：相对安装目录，文件不存在时
  `WubiTable` 静默留空，五笔模式退化为"任何编码都查不到候选"而不是崩溃）。

### 5.12 `src/config/config_defaults.hpp`
现状（70-87 行）`InputConfig`。
目标：新增两个字段，并在现有 `scheme` 字段注释末尾补一句明确的"不要跟 method
混淆"说明：
```cpp
struct InputConfig {
    // ……（现有 scheme/fuzzy/number_lead_key/bihuo_enabled 字段完全不变）……
    // 注意：本字段（scheme）只管拼音"方案家族"内部的严格度（全拼/简拼/混拼），
    // 是否使用拼音还是五笔由下面的 method 字段决定，两者是正交维度，不要混淆。

    // 2026-09-12 新增：pinyin（默认）/wubi。method==wubi 时，scheme/fuzzy/
    // number_lead_key/bihuo_enabled 全部被忽略（WubiCandidateSource 是一条独立的
    // CandidateSource 实现，不读这些拼音专属字段）。
    std::string method = "pinyin";

    // 2026-09-12 新增：TIP 侧识别的切换热键描述串，格式 "ctrl+shift+<字母>"
    // （tsf-service 侧新增的小 parser 解析，不在 src/config 里做字符串解析逻辑，
    // 本字段只是反硬编码落点本身）。
    std::string method_switch_hotkey = "ctrl+shift+w";
};
```

### 5.13 `src/domains/tsf-service` 侧热键检测与转发
- 新增 `src/domains/tsf-service/logic/scheme_hotkey_detector.hpp` / `.cpp`：结构完全
  仿照 `mode_manager.hpp/.cpp`（纯本地状态机，`OnKeyDown(int vk, bool ctrl, bool shift)`
  判断是否命中配置的组合键，命中时返回 true 且内部状态翻转当前方案标记）。
- `src/domains/tsf-service/backend/ipc_client.hpp/.cpp`：仿照 `SetCaretRect`
  （`ipc_client.hpp:82`）的简单请求模式，新增：
  ```cpp
  bool SetInputMethod(const std::string& method);   // method: "pinyin" | "wubi"
  ```
  内部调用 `CallMethod(Method::kSetConfig, Json{{"input", Json{{"method", method}}}}, out)`。
- `src/domains/tsf-service/backend/myabc_text_service.cpp`：`OnKeyDown`
  （250 行起）在现有 `mode_manager_.OnKeyDown(...)` 调用旁边新增
  `scheme_hotkey_.OnKeyDown(vk, ctrl, shift)` 检测，命中时调用
  `ipc_client_.SetInputMethod(...)`（成功与否都不阻塞——同 `SetCaretRect` 的
  "单向语义强，失败无需特殊处理"既有惯例，`ipc_client.hpp:79-82` 注释）。
- `src/domains/tsf-service/logic/key_router.cpp`：`IsInterestedKey()`
  （29-51 行）在非组字态分支（47-50 行）也需要认得这个热键组合，否则宿主应用会
  在 TIP 之前吃掉这次按键——新增判断：`vk` 匹配热键配置里的字母键且
  `ctrl && shift` 均按住时返回 `true`。**开工前必须在真机上验证 `Ctrl+Shift+W`
  在 Word/Excel/Chrome 里没有被系统或宿主应用预先绑定**（若有冲突，改默认组合键，
  这是本计划留给实现阶段的真机验证项，不是能在纯代码审查阶段确定的事）。

### 5.14 `src/shared/ipc-protocol/protocol.hpp`
`kProtocolVersion`（42 行）：`8` -> `9`。

### 5.15 数据资产
- 新增 `assets/data/wubi86.txt`：由一次性转换脚本（不进产品运行时代码，可以是
  临时 Python/Bash 脚本，用完不需要保留在仓库里，只保留输出文件——参照
  `bihuoma.txt` 的既有做法，转换脚本本身不是交付物）从 `wubi86.dict.yaml` 转换而来：
  1. 只保留 `text` 字段全部字符落在基本 CJK 统一表意文字区（`U+4E00`-`U+9FFF`）的行
     （排除 CJK 扩展 B 区及以上的罕见字，如尾部出现的 `𫚖`/`𬭯` 等——这些字符编码
     成 UTF-16 代理对，会给 `CandidateItem::text`/IPC JSON 编解码引入不必要的复杂度，
     且用户几乎不可能需要打这些字）。
  2. 丢弃 `text` 含非汉字符号的行（如实测发现的 `#°\tya\t0` 这类度数符号噪声行）。
  3. 输出列重排为 `code\ttext\tweight`（Tab 分隔），文件头写清楚数据来源/License/
     转换规则/上游 Changelog 归属（格式仿 `assets/data/bihuoma.txt` 文件头）。
  4. 转换后的行数需要在提交时如实记录在 `assets/README.md`（不用提前在计划里编造
     一个精确数字——§3.2 的原始行数统计已经给出量级参考：单字 32,680 行、双字词
     48,453 行、三字词 4,518 行、四字词 7,934 行，过滤扩展区字符后会显著减少但
     仍应在大几万行量级）。
- `assets/README.md`：仿照 20-25 行 `bihuoma.txt` 条目的格式，新增 `wubi86.txt` 条目，
  完整记录 rime/rime-wubi 仓库地址、LGPL-3.0、原始 Changelog 归属链路
  （Wozy 原始码表 -> Yu Yuwei/Chen Xing/Gong Chen 转 Rime 格式 -> myabc 二次过滤转换）、
  转换规则、最终收录字/词数。
- `scripts/stage.ps1`：紧跟 `bihuoma.txt` 那一行（77/80 行附近）新增
  `Copy-IfExists (Join-Path $MyabcRepoRoot 'assets\data\wubi86.txt') $dataOut`。

### 5.16 单元测试
- 新增 `tests/unit/wubi_table_test.cpp`（仿 `bihuoma_filter_test.cpp` 风格：合成小表，
  `LoadFromText` 装载，断言：精确码命中返回正确排序候选；未收录码返回空 vector；
  空表所有查询返回空 vector 不崩溃）。
- `tests/unit/CMakeLists.txt`：仿 39-46 行 `bihuoma_filter_test` 的注册模式（同样只在
  `MYABC_TOOLCHAIN STREQUAL "mingw" AND TARGET myabc::engine-core` 时构建）新增
  `wubi_table_test`。

### 5.17 集成/端到端回归测试（新建，填补现有空目录）
`tests/integration/` 与 `tests/e2e/` 目前是空目录，历史上"真实 IPC 端到端测试"都是
未提交仓库的一次性脚本（debt-log 里反复提到但仓库里找不到对应文件）。鉴于本次
改动的回归红线是项目里优先级最高的不变量（空格两段式确认），**本计划要求新增一个
提交进仓库、可重复运行的集成测试**，而不是再做一次"验证完就丢"的一次性脚本：
- 新增 `tests/integration/scheme_switch_regression_test.cpp`：一个独立可执行文件，
  用真实命名管道连接一个由测试自己拉起的 `myabc-engine.exe --pipe <临时管道名>
  --sid test --model-dir <既有测试词库目录> --user-dir <临时目录>` 子进程（参照
  `engine_conversion_test.cpp` 已有的"MinGW 侧、需要 `LibPinyin_DATA_DIR`"构建
  条件），驱动一套真实 `processKey`/`selectCandidate`/`setConfig` 请求序列，
  断言响应内容，覆盖 §7 全部验收判据。用完终止子进程。
- `tests/unit/CMakeLists.txt`（或新建 `tests/integration/CMakeLists.txt`，若项目决定
  区分单元/集成测试的 CMake 注册文件——与现状"`tests/unit/CMakeLists.txt` 已是唯一
  注册点"保持一致即可，不强求拆分）：注册为 `add_test`，同样限定 MinGW 侧 +
  `LibPinyin_DATA_DIR` 就绪。

## 6. 不改动清单

- `CandidateSource`/`InputContext` 除新增 `scheme` 字段外的既有结构、`SourceRegistry::
  Resolve()` 的"第一个 `Handles()==true` 独占胜出"分发算法本身——不新增第二种分发
  策略。
- `Session::SelectCandidate()`、`Session::CommitComposition()`、
  `Session::PageCandidates()`、`Session::CancelComposition()`、`Session::FocusOut()`、
  `Session::MaybeTrain()`、`Session::ResetToIdle()`——五笔完全复用现有实现，一行不改
  （§4.1/§2.4 已论证）。
- `ProcessKey()` 里 VK_BACK / VK_ESCAPE / VK_SPACE（43-72 行）——两段式确认逻辑本身
  逐字保持不变，这是最高优先级的红线。
- `PunctuationSource`、`EnglishPassthroughSource`、`ExtensionCandidateSource`——三者
  `Handles()` 均不需要感知 scheme（§2.3 已论证无冲突），不加任何 scheme 判断。
- `LibPinyinEngine` 类本身、`bihuoma_table.hpp/.cpp`（笔形辅助码逻辑）、
  `number_currency_source.cpp` 的 `Produce()`（只在 `Handles()` 开头加一行守卫，
  `Produce()` 内部逻辑不动）——五笔模式下这些代码路径根本不会被触达，不需要碰。
- `config.input.scheme`（quanpin/jianpin/hunpin）、`config.input.fuzzy`、
  `config.input.number_lead_key`、`config.input.bihuo_enabled` 四个既有字段的默认值
  与既有语义——完全不变，只是在 `method==wubi` 时被结构性地"不使用"（因为
  WubiCandidateSource 根本不读它们），不需要在代码里加"如果是 wubi 就忽略这些
  字段"的判断（§5.5 已论证）。
- `docs/decisions/input-engine/20260911-space-key-two-step-confirm.md` 记录的全部规格
  不做任何修订——本计划视其为跨方案通用的既定规格，只是新增了"数字键在五笔模式下
  不受 `space_armed_` 门槛限制"这一条**新增而非修订**的补充规则（应在实施后作为
  新的决策文档追加记录，见 §7 验收判据里的留痕要求）。
- 五笔拆字算法、98 版数据、字根图 UI、五笔专属自学习/动态调频（§3.3/§4.1 已论证
  排除）、词库前缀提示/联想（§5.5 已论证只做精确码匹配）、语言栏按钮 UI
  （§4.2 已论证否决/延后）、`config.toml` 真正的 TOML 解析器（§2.6 已论证不依赖它）
  ——均明确不在本计划范围内，不应在实施过程中"顺手"扩展。
- `docs/plan/roadmap.md`——不在本次授权修改范围内（任务要求只新建 `docs/plan/` 下
  的新文件 + 更新 `docs/plan/INDEX.md`），本计划的落地时机由用户/主会话后续自行
  决定是否登记进 roadmap。

## 7. 可执行验收判据

以下判据全部基于 §3.2 已验证的真实数据行，不使用编造数据。前 6 条是**回归判据**
（拼音既有行为零变化），后续是**新增功能判据**。

### 7.1 拼音回归（最高优先级，任何一条失败都必须视为本次改动的阻塞性缺陷）

| # | 场景 | 操作序列 | 期望 |
|---|---|---|---|
| R1 | 空格两段式确认 | `processKey` 拼出 "nihao"（多候选）-> 第一次 `VK_SPACE` | `composing:true`，无 `commit`，`armedIndex:0`（经 `uiShow`） |
| R2 | 同上，第二次空格 | 紧接 R1 再一次 `VK_SPACE` | 提交候选[0]（"你好"），`composing:false` |
| R3 | 数字键选字不受两段式影响 | 拼 "nihao" -> 数字键 "1" | 一步直接提交，无需先按空格 |
| R4 | 数字模式 | 拼 "i2025" | 候选含"二〇二五"；`space_armed_` 前数字续写到 raw_，架住后才选字（跟 R1/R2 一致的两段式） |
| R5 | 笔形辅助码 | 拼 "wo" 再按 "3" | raw_ 变成 "wo3"，候选被笔形过滤（不当作选字键） |
| R6 | Ctrl+数字直选 | 拼 "nihao"（未架住）-> `ctrl+"1"` | 立即选中候选[0]，不需要先架住 |

验证方式：`tests/integration/scheme_switch_regression_test.cpp`（§5.17）覆盖 R1-R6，
`ctest -R scheme_switch_regression` 全部通过；另需人工/既有回归脚本对照
`docs/decisions/_debt-log.md` 2026-09-11/12 各条目描述的场景逐条真机复测一遍
（这是项目历史上每次改动 `Session` 后的一贯做法）。

### 7.2 五笔新功能

| # | 场景 | 输入（`raw`） | 期望（基于 §3.2 真实数据行） |
|---|---|---|---|
| W1 | 单键字根，唯一候选 | `"a"` | 候选 `["工"]`，因 `candidates_.size()==1`，无需按键直接可选中（复用既有"只剩一个候选自动选中"规则，`Recompute()` 250 行既有逻辑，五笔天然复用） |
| W2 | 双字词，4 键编码 | `"aaad"` | 候选含 `"工期"` |
| W3 | 真实重码（2 候选） | `"aet"` | 候选 `["散", "㣉"]`（按权重降序，"散" 在前） |
| W4 | 数字键立即选字（不需要先按空格） | 拼 `"aet"` -> 数字键 `"2"`（未架住） | 立即提交 `"㣉"`——**这是与拼音模式最核心的行为差异**，必须单独断言 |
| W5 | 空格两段式在五笔重码下仍生效 | 拼 `"aet"` -> `VK_SPACE` -> `VK_SPACE` | 第一次架住候选[0]（"散"）不提交，第二次提交"散" |
| W6 | 编码长度上限 | 拼满 4 键 `"aaaa"` 后再按第 5 个字母 | `raw_` 保持 4 字符不变，候选不受影响，多余字母被吃掉不追加 |
| W7 | 未命中编码 | `"zz"`（86 版 z 键不参与编码，见 §3.2 encoder `exclude_patterns: '^z.*$'`） | 候选为空，组字继续（不崩溃、不误报错误） |
| W8 | 退格/取消 | 拼 `"aet"` -> `VK_BACK` | 回到 `"ae"`，重新查表 |

### 7.3 切换机制

| # | 场景 | 期望 |
|---|---|---|
| S1 | `setConfig` 生效 | 发送 `{"method":"setConfig","params":{"input":{"method":"wubi"}}}`，之后同一 `sessionId` 的 `processKey("a")` 返回五笔候选（"工"）而非拼音候选 |
| S2 | 切回拼音 | 再次 `setConfig` 为 `"pinyin"`，`processKey("a")` 恢复拼音候选（声母 "a" 的整句猜测） |
| S3 | 切换时正在组字 | 拼 "nihao" 到一半（未提交）-> `setConfig` 切五笔 -> 该 session 收到 `uiHide`（经 `ui_bridge_->PushHide`），半成品不上屏、不训练 |
| S4 | 未知 method 值 | `setConfig` 传 `{"input":{"method":"cangjie"}}` | 返回 `Response::Err`，不改变当前 scheme |
| S5（TIP 侧，需真机） | 热键触发 | 在记事本按 `Ctrl+Shift+W`，之后打字走五笔候选；再按一次切回拼音候选 |
| S6（TIP 侧，需真机） | 热键无宿主冲突 | 在 Word/Excel/Chrome 地址栏分别测试 `Ctrl+Shift+W`，确认没有被宿主应用先吃掉、没有触发意外的宿主功能 |

### 7.4 静态门禁

| # | 命令 | 期望 |
|---|---|---|
| G1 | `bash tests/review/run_all.sh` | 全绿（`check_structure.sh`/`check_hardcode.sh` 均 PASS；本计划新增的 `wubi_table.cpp`/`wubi_candidate_source.cpp` 不含裸路径/裸 IP/裸端口等违规模式） |
| G2 | 协议版本 | `grep -n "kProtocolVersion" src/shared/ipc-protocol/protocol.hpp` 输出 `9` |
| G3 | ctest（MinGW 侧） | `ctest` 新增的 `wubi_table_test`、`scheme_switch_regression_test` 通过，既有 `cn_number`/`bihuoma_filter`/`learning_policy`/`ipc_roundtrip`/`engine_conversion` 全部依旧通过（零回归） |
| G4 | 反硬编码扫描 | `assets/data/wubi86.txt` 本身不受 `check_hardcode.sh` 扫描（数据文件非 `.cpp/.hpp`），但 `engine_main.cpp` 里 `SelfDir() + "\data\wubi86.txt"` 这种相对路径拼接方式（同现有 `bihuo_data_path` 写法）不应触发"裸 Windows 绝对路径"规则（因为不是绝对路径字面量） |

## 8. 需求文档更新建议

`docs/requirements/smart-abc-ime-requirements.md` 需要更新（属于对项目定位的实质
扩展，任务背景条目 8 已预判）：

- §4 非目标：当前第 49 行 `"五笔、双拼方案（架构预留，不实现）"` 需要拆分——
  **删除"五笔"**（本计划把它从"预留不实现"变成"实现"），**保留"双拼"**（本次
  任务范围不含双拼，双拼仍属于拼音家族内部的方案，见 `InputConfig::scheme` 的
  `shuangpin`（预留）字段，跟本计划的 `method` 维度无关，不应被本次改动的措辞
  连带影响）。
- §3 新增 "3.3 输入方案切换（新增）" 小节，简述：拼音/五笔切换（热键触发，默认
  `Ctrl+Shift+W`）；五笔字型 86 版，数据来自开源码表（单字+词组），支持重码数字键
  直选；不含拆字算法、不含 98 版、不含语言栏 UI（v1）、不含五笔专属自学习。
- 文档头部"更新日期"/"来源"字段相应更新为本次日期与"用户对话 + deep-planner 规划"。
- 这些改动应作为本计划实施的一部分（属于文档同步义务，不需要单独立项），但**不在
  本次 deep-planner 授权范围内**（本次任务明确要求"只新建 docs/plan/ 下文件 +
  更新 INDEX.md"）——留给后续执行 Sonnet 在落地代码的同一个改动集里一并完成，
  本节只给出需要改的具体位置和措辞方向，不代为直接编辑。

## 9. 风险与未决问题（需要用户拍板，不由本计划代为决定）

1. **热键组合与最终 UX**：本计划推荐默认 `Ctrl+Shift+W`，但未做穷举式真机冲突
   测试（§5.13 已列为实施阶段的必做真机验证项）。如果用户对这个组合键有既定偏好
   （或已知会跟常用软件冲突），需要在开工前给出替代组合。
2. **语言栏图标/菜单是否要在 v1 就做**：本计划推荐否决（§4.2），只做"纯热键切换、
   无可见状态指示"的 v1。如果用户认为"用户根本不知道现在是拼音还是五笔"这个体验
   缺口不可接受，需要追加一个独立的、聚焦在 TSF 语言栏 COM 实现上的后续计划
   （工作量与本计划核心的五笔功能本身相当，建议拆成独立里程碑，不建议塞进本计划
   一起做）。
3. **方案选择是否需要跨进程重启持久化**：本计划默认**不持久化**（`myabc-engine.exe`
   每次重启/空闲自退出后回到 `method=pinyin`，默认 `idle_exit_minutes=30`，见
   `config_defaults.hpp:59`，意味着用户如果连续 30 分钟没打字，五笔选择就会在不知
   不觉中丢失）。若用户认为这个体验不可接受，需要追加一个轻量持久化方案（**推荐
   方向**：不为此实现完整 TOML 解析器，改为一个独立的小状态文件，如
   `%APPDATA%\myabc\state.json` 只存 `{"method":"wubi"}`，`setConfig` 处理时顺手写一次，
   `engine_main.cpp` 启动时读一次覆盖 `cfg.input.method`——这个方向需要用户确认后
   才补进本计划或开独立子任务，本计划不代为拍板）。
4. **五笔数据表最终收录规模**：§5.15 给出的是过滤规则而非精确行数，最终收录多少
   字/词由转换脚本实际运行结果决定。如果用户希望有一个明确的"至少覆盖 N 个常用字"
   验收门槛（类比笔形辅助码当初"覆盖约 6939 字"的量化验收），需要在开工前明确
   这个数字，本计划暂不设置武断的门槛值。
5. **五笔编码里 `z` 键的处理**：86 版标准里 `z` 键通常保留给"万能键/简码补全"这类
   高级特性（§3.2 数据源的 `encoder.exclude_patterns: '^z.*$'` 也印证了这一点——码表
   本身不含 `z` 开头的编码）。本计划的 MVP 范围**不实现** `z` 键的任何特殊语义
   （既不当万能匹配，也不当拆字提示），`z` 键在五笔模式下就是一个"查无编码"的
   普通字母键（W7 判据）。如果用户后续认为这是必须补的功能，需要另开子任务设计
   `z` 键语义（真实五笔的 `z` 键行为本身流派不一，需要额外调研）。

## 10. 补充修订（2026-09-13，用户澄清：三种输入模式而非两种，§4.2/§4.3 判断被推翻）

用户在 §9 决策点确认完之后，进一步纠正了 §4.3 的核心判断：**空格两段式确认
本身是"智能ABC"这个特定风格的产物（因为它需要用数字键身兼"笔形码/数字模式
续写"和"选字"两种含义，才需要"架住"这个中间态来消歧），不是"拼音输入法"
这个大类本身必须具备的东西**。用户要求的实际是三种可选输入模式，不是本计划
原来设计的两种：

1. **智能ABC**（原有默认行为，**完全不变**）：空格两段式确认 + 笔形辅助码 +
   `i` 数字金额模式，数字键在 `space_armed_` 之前是续写、之后才是选字——即
   `docs/decisions/input-engine/20260911-space-key-two-step-confirm.md` 记录的
   全部既有规格，一行不改。
2. **普通拼音**（新增，本次修订新增的第三种模式，§4.2/§4.3 原文里没有设想到）：
   跟"智能ABC"用**同一个** libpinyin 引擎 + `PinyinCandidateSource`（候选生成
   完全一样，全拼/简拼/混拼/模糊音这些能力照常可用），但**按键路由完全不同**：
   数字键任何时候都直接选字（不需要先按空格架住），空格键任何时候都立即选中
   候选[0]（不架住、不等第二次）。因为数字键从不会被当"续写"追加进 `raw_`，
   笔形辅助码和 `i` 数字模式在这个模式下**没有触发条件**，不需要另外加开关
   关闭——路由改变本身就让这两个功能自然失效，不是刻意禁用。
3. **五笔**：跟原计划一致，候选来源换成 `WubiCandidateSource`，按键路由跟
   "普通拼音"共用同一套"数字直选、空格直选"的简单逻辑（两者的路由函数应该是
   同一个，不要为五笔单独抄一份）。

### 对 §4.2/§4.3/§5 各节的具体影响

- `InputScheme` 枚举从 `{kPinyin, kWubi}` 改为 **`{kSmartAbc, kPlainPinyin, kWubi}`**
  三值。
- `PinyinCandidateSource::Handles()` 的 scheme 守卫改为
  `ctx.scheme == kSmartAbc || ctx.scheme == kPlainPinyin`（两种模式都用它产出候选，
  只是候选产出后的按键路由不同）。
- `NumberCurrencySource::Handles()` 的 scheme 守卫改为 `ctx.scheme == kSmartAbc`
  （不变，原计划已经是"只有智能ABC模式能用"，这条判断本身仍然对，无需改）。
- `Session::ProcessKey` 的数字键分派从"两路（智能ABC/五笔）"改为"两路但分组
  不同"：`kSmartAbc` 走原有 `RouteDigitKeyPinyin`（一字不改）；`kPlainPinyin`
  **和** `kWubi` 共用同一个函数（原计划 §5.8 命名的 `RouteDigitKeyWubi`
  建议改名为更中性的 `RouteDigitKeyDirect`，因为它不再是"五笔专属"，是"数字
  直选"这一类路由策略的实现，五笔和普通拼音两个 scheme 都调它）。
- `VK_SPACE` 分支同样要按 scheme 分叉：`kSmartAbc` 走原有两段式逻辑（原文
  57-72 行，一字不改）；`kPlainPinyin`/`kWubi` 统一为"不论候选数多少，直接
  `return SelectCandidate(0);`"，不再检查 `candidates_.size()`/`space_armed_`。
- §5.12 `InputConfig::method` 的取值从 `"pinyin"/"wubi"` 改为三值
  `"smartabc"（默认）/"pinyin"/"wubi"`，前端 UI/热键切换逻辑需要支持三态循环
  切换，不是原来设想的二态。
- §7 验收判据需要新增"普通拼音"模式的判据组（结构参照 §7.2 五笔判据，改用
  拼音候选断言：数字键立即选字、空格立即选中候选[0]、笔形码/数字模式在这个
  scheme 下确认不触发），并且 §7.1 拼音回归判据组明确只针对 `kSmartAbc`
  这一个 scheme（避免误解成"所有拼音模式都要有两段式"）。
- §9 决策点 1/2/3（热键、语言栏可见性、持久化）已经过用户确认（分别为：默认
  `Ctrl+Shift+W`、v1 需要语言栏可见状态提示、需要持久化到状态文件）——这三条
  在三态模型下同样适用，只是热键需要支持三态循环（或者一个热键"循环切下一个"
  +语言栏菜单支持直接跳选任意一态，具体交互在实现时定，不需要用户为此再拍板
  一次，属于工程实现细节）。

这份补充没有推翻 §3（数据来源）、§5.4-5.6（`WubiTable`/`WubiCandidateSource`
本身的设计不变）、§5.9-5.11（`SessionManager`/`Dispatcher`/`engine_main` 的
改造模式不变，只是 scheme 值域从 2 个变 3 个）——只是 §4.2/§4.3 和它们在 §5
里对应的具体代码位置需要按上面的三态模型重新实现，原文这些段落的 pseudocode
按"只增不删"原则保留在上面不做删除，但**以本节为准**，实现时不应再按原
§4.2/§4.3/§5.7/§5.8/§5.12 的两态版本执行。
