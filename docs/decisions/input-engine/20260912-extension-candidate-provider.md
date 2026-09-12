# 外部候选源扩展协议

- 日期：2026-09-12
- 领域：input-engine
- 状态：生效中
- 触发来源：用户对话（"开始实现企业能力"，见 `E:\work\inputserver` 需求文档）

## 背景

用户在设计一套企业级智能补全能力（工作机上接入企业知识库，如通讯录：拼音输入
姓名、上屏对应电话号码），明确要求这部分企业能力（登录、需求上报、跟内部服务器
对接）不开源，独立成一个私有仓库 `inputserver`——myabc（GPL-3.0-or-later）只
交付一份"对接规范"，不交付企业能力本身，避免企业代码被拖进 GPL 范围（详细的
项目边界讨论、被否决方案对比见 `inputserver` 仓库的
`docs/requirements/smart-complete-requirements.md`，那份文档才是完整需求，
本文档只记录 myabc 这一侧要交付的协议本身的技术决策）。

本文档记录的是这份"对接规范"在 myabc 引擎侧的具体实现：一条独立的命名管道
协议，让一个可选的、独立分发的外部进程能给引擎的候选流程提供补充候选。

## 决策内容

### 1. 独立命名管道，不复用现有两条

新增 `myabc-extension-{sid}`（跟 `myabc-engine-{sid}`、`myabc-ui-{sid}` 都
不共用）。第三方提供者连接后，第一条消息必须是 `registerExtension`
（`{tags: ["phone", ...]}`），引擎收到后记录这些 tag 并把这条连接发布为
"当前提供者"（同一时刻只支持一个提供者，新连接直接替换旧的，跟 `UiBridge`
的既有惯例一致）。之后引擎可以在同一条连接上反向发起 `queryCandidates`
（`{tag, raw}`），提供者同步返回 `{candidates: [{text, commitText?}, ...]}`。

### 2. 有界超时、fail-open，绝不阻塞按键热路径

`ExtensionBridge::Query()` 用 overlapped IO + `CancelIoEx` 做真正的有界读写
（跟 TIP 侧 `IpcClient::BoundedRead`/`BoundedWrite` 同一套手法，不能共享代码
——MinGW/MSVC 两侧工具链独立编译，见协议头不变量 6）。任何失败（没有提供者、
tag 不匹配、连接断开、超时、格式不对）都当"没有扩展候选"处理，正常拼音候选
完全不受影响——这是跟项目其它外部依赖（引擎连接异步化、UI 推送失败即放弃）
一致的既有原则。

### 3. 接入方式：`CandidateSource` 适配器，而不是特殊分支

新增 `ExtensionCandidateSource`，复用既有的 `CandidateSource`/`SourceRegistry`
适配器架构（不是在 `Session::ProcessKey`/`Recompute()` 里加 if/else 特殊
判断）——新来源=新增一个类+在 `BuildDefaultSourceRegistry` 里登记，符合项目
"3 个以上实现触发适配器强制线"的既有约定（现在已经是第 5 个来源：english/
number/punctuation/pinyin/extension）。放在列表最前面，因为它只在真查到候选
时才接管（见下一条），查不到会自然让路，不会打乱既有优先级。

### 4. 实现时发现并修复的真问题：Handles() 必须真的做查询

最初的设计是 `Handles()` 只检查"有没有提供者注册了这个 tag"（不管这次查询
有没有结果），`Produce()` 才真正发起查询。这会导致一个真实回归：提供者注册了
tag 但对当前输入（比如姓名只打了一半）查不到匹配时，`Produce()` 返回空列表，
而 `SourceRegistry::Resolve()` 是"第一个 `Handles()==true` 的来源独占胜出"，
候选窗会因此整个清空，用户连正常拼音候选都看不到——比没有这个功能还糟。

修法：把实际查询挪到 `Handles()` 里做（用 `mutable` 缓存 + raw+field_hint
拼接串当缓存键，避免 `Resolve()->Produce()` 这组固定调用顺序里对同一个
`InputContext` 重复查询一次），`Handles()` 返回值本身就等于"这次真的有候选
可给"，查不到就老老实实返回 false，交回给后面的 `PinyinCandidateSource`。
真实 IPC 端到端测试验证：中间态（如 "zh"/"zha"/"zhan"）查不到时正常拼音候选
完全不受影响，只有打满完整拼音（"zhang"）且提供者真返回候选时才接管。

### 5. `CandidateItem` 新增 `commit_text`，显示≠上屏

现有所有候选来源都是"显示什么就上屏什么"，这次需要"显示联系人姓名、上屏
电话号码"，两者必须能分开。`commit_text` 默认为空（沿用 `text`），非空时
`Session::SelectCandidate`/`CommitComposition` 改用 `EffectiveCommitText()`
辅助函数取实际上屏文本，不再直接读 `.text`。

### 6. `field_hint`：现在只搭好通路，没有真实自动检测

`InputContext` 新增 `field_hint` 字段，新增 IPC 方法 `setFieldHint`
（TIP -> 引擎，`{sessionId, hint}`），`Session::SetFieldHint()` 存下来，
`Recompute()` 时带进 `InputContext` 给候选来源判断。**目前没有任何自动检测
机制**——TIP 侧要用 UI Automation 查当前聚焦控件、推导出"这是电话字段"这类
提示，是完全独立的 TIP 侧工作，尚未开始（见 `inputserver` 需求文档 §未决
问题）。现在这个字段只能靠直接调 `setFieldHint`（比如测试脚本、未来的
deployer CLI）手动设置，验证了协议本身没问题，不代表"自动识别字段类型"这个
功能已经完成。

### 7. 协议版本 v7 -> v8

新增 3 个方法：`setFieldHint`（TIP -> 引擎，跟既有方法同一条连接）、
`registerExtension`/`queryCandidates`（走独立的 extension 管道，不影响
TIP<->引擎那条连接的协议兼容性——一台没有安装扩展客户端的机器，引擎行为
跟 v7 完全一致，只是多监听了一条没人连的管道）。

## 被否决的备选方案

- **直接 DLL 加载（`LoadLibrary`/`GetProcAddress`）**：讨论过命名管道 vs DLL
  两种方案的完整优劣对比（崩溃隔离、GPL 边界确定性、性能、实现复杂度等），
  用户明确选择维持命名管道——崩溃隔离（外部提供者崩溃不牵连引擎进程）和
  GPL 协议边界的确定性（独立进程通信是 FSF 自己认可的"更像是聚合而不是衍生
  作品"的信号）比 DLL 方案的性能优势更重要。
- **`Handles()`只做"有没有提供者"这种轻量判断**：见上面第 4 条，实现时发现
  会导致真实的候选清空回归，改成"查询结果就是判断依据"。
- **跟拼音候选混排展示**：`SourceRegistry::Resolve()` 现在是"单来源独占胜出"，
  真正的混排需要改成"可叠加多来源"，是更大的架构改动。当前范围收窄成"有匹配
  结果就接管，没有就完全让路"，混排留待真实使用反馈后再决定要不要做（见
  `inputserver` 需求文档 §9 未决问题）。

## 影响范围

- `src/shared/ipc-protocol/protocol.{hpp,cpp}`：协议版本 v7->v8，新增 3 个
  Method。
- `src/domains/input-engine/backend/libpinyin_wrapper.hpp`：`CandidateItem`
  新增 `commit_text` + `EffectiveCommitText()` 辅助函数。
- `src/domains/input-engine/backend/extension_bridge.{hpp,cpp}`（新增）。
- `src/domains/input-engine/logic/candidate/extension_candidate_source.{hpp,cpp}`
  （新增）。
- `src/domains/input-engine/logic/candidate/candidate_source.hpp`：
  `InputContext` 新增 `field_hint`。
- `src/domains/input-engine/logic/candidate/source_registry.{hpp,cpp}`：
  `BuildDefaultSourceRegistry` 新增 `extension_bridge` 参数，注册顺序最前。
- `src/domains/input-engine/logic/session/session.{hpp,cpp}`：新增
  `SetFieldHint()`，`SelectCandidate`/`CommitComposition`/`Recompute` 的
  提交路径改用 `EffectiveCommitText()`。
- `src/domains/input-engine/logic/dispatcher.{hpp,cpp}`：新增
  `HandleSetFieldHint`，构造函数新增 `extension_bridge` 参数。
- `src/domains/input-engine/logic/engine_main.cpp`：构造 `ExtensionBridge`
  （新增 `--extension-pipe` 命令行覆盖），传给 `Dispatcher`。
- `src/config/config_defaults.hpp`/`config_loader.cpp`：新增
  `extension_pipe_name_template`。
- MSVC 侧（TIP/candidate-ui/deployer）：只受协议版本号影响（v8），本轮不涉及
  `setFieldHint` 的 TIP 侧实际调用方（UIA 检测），TIP 目前完全不知道这个
  新方法的存在。

## 验证

- 新增真实 IPC 端到端测试（独立的假冒外部提供者，用一个后台线程模拟
  inputserver 的行为：注册 tag、按拼音返回候选/查不到就返回空）：验证了
  (1) 中间态（拼音没打满，通讯录查不到）正常拼音候选完全不受影响；(2) 打满
  拼音、提供者真返回候选时，候选窗显示提供者给的多个候选（姓名+姓名，验证
  多候选正常展示，呼应 inputserver 需求 §4.4 的"多字段"设想）；(3) 选中后
  上屏的是 `commitText`（电话号码），不是显示文本（姓名）；(4) 没有设置
  `field_hint` 的普通 session 完全不受这套新协议影响（回归）。
- MinGW ctest 5/5、MSVC ctest 3/3、`tests/review/run_all.sh` GREEN。
- 既有 e2e 回归脚本（space-arm/bihua/ctrl-digit）在协议版本升到 v8 后重跑
  全部通过，确认新协议没有破坏任何既有功能。

## 关联

- `E:\work\inputserver\docs\requirements\smart-complete-requirements.md`：
  完整的企业级能力需求（这份 ADR 只覆盖 myabc 这一侧的协议实现）。
- `docs/decisions/_debt-log.md` 2026-09-12 置顶条目。
