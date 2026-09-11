# 空格键智能ABC 风格两段式确认

- 日期：2026-09-11
- 领域：input-engine（跨 candidate-ui/shared/ipc-protocol）
- 状态：生效中
- 触发来源：用户对话（2026-09-11）——用户明确表示"这个功能是我做这个输入法的唯一目标"

## 背景

M1 起，空格键的行为一直是"无条件选中当前候选[0]"（`Session::ProcessKey` 里
`if (vk == VK_SPACE) return SelectCandidate(0);`），不管候选是 1 个还是 9 个。用户
在 M5 完成、准备real-machine验证时提出：这不是他想要的智能ABC体验——智能ABC的空格键
在候选有歧义时不应该"手滑"就把候选[0]直接拍上屏，而应该先让用户看一眼、确认一下。

经过两轮 `AskUserQuestion` 澄清（详细的按键时序 transcript 逐个确认），最终精确到：

> 打 nihao（多候选：你好/你号/你毫...）
> 按 SPACE -> 候选框里"你好"被高亮，但还没上屏，组字继续
> 再按 SPACE 或按数字键"1" -> 上屏"你好"，组字结束

即：**候选数 <= 1 时空格直接选中上屏（无歧义，不需要确认）；候选数 >= 2 时第一次
空格只把候选[0]"架住"（UI 高亮，不上屏），第二次空格或直接按数字键才真正选中**。

## 决策内容

1. `Session` 加 `space_armed_` 状态位。`ProcessKey` 组字态下 `VK_SPACE` 分支：
   - `candidates_.size() <= 1`：直接 `SelectCandidate(0)`（原有行为不变）。
   - 未架住：置位 `space_armed_ = true`，返回 `composing:true`、无 `commit`。
   - 已架住：清位，`SelectCandidate(0)` 真正选中上屏。
2. `raw_` 发生任何变化（追加字母/退格触发 `Recompute()`、笔形/数字模式追加字符、
   翻页 `PageCandidates`、换到下一段的 `SelectCandidate` 部分确认分支）都清掉
   `space_armed_`——"架住的是哪份候选"这个语义只在候选列表原地不变时才有效。
3. **不区分候选来源**：拼音候选（`PinyinCandidateSource`）、数字/金额候选
   （`NumberCurrencySource`）统一走这套两段式逻辑，不做特殊豁免——保持行为一致、
   实现简单；真人测试确认数字模式下这套逻辑同样自然（`i2025` 候选数>1，第一次
   空格架住"两千零二十五"，第二次空格才提交）。
4. **数字键（select_keys）完全不受影响**：任何时候按数字键都是一步直接选中，
   不需要先按空格"预热"——两段式确认只是空格这一个键的"防手滑"机制，数字键本来
   就是显式指名要选第几个，没有"误触"的顾虑。
5. 协议新增字段：`uiShow` 消息加 `armedIndex`（-1=无，>=0=当前页这个下标被架住）。
   `PROTOCOL_VERSION` 5 -> 6（不变量 7：结构变更必须递增版本号）。
6. 候选窗渲染（`candidate_window.cpp`）：`armedIndex` 对应的候选行画一个高亮底色
   （`RGB(0xCC,0xE5,0xFF)`），区分于普通候选行，让用户看清"再按一下就是它"。

## 被否决的备选方案

- **空格第一次按下时"什么都不做"（候选框原地不动），必须用数字键才能选中**：
  用户在澄清问题里明确否决了这个方案——空格应该有视觉反馈（高亮），不是纯粹的
  "无效按键"。
- **按候选来源做差异化处理（拼音候选两段式，数字/金额候选保持一步直选）**：
  没有必要——真机测试确认数字模式下两段式确认体验同样自然、不违和，维持统一
  逻辑更简单、不给未来新增的 `CandidateSource` 留特殊豁免的口子。
- **只在 UI 层做"高亮但不提交"，Session 逻辑不变**：不可行——"上屏与否"是
  Session 的核心状态判断（`has_commit`/`composing`），UI 层无权决定是否提交，
  必须在 Session 里真正拦一次。

## 影响范围

- `src/domains/input-engine/logic/session/session.{hpp,cpp}`：`space_armed_`
  状态位、`ProcessKey` 的 `VK_SPACE` 分支、`SessionResult::armed_index`。
- `src/domains/input-engine/logic/dispatcher.cpp`：`HandleSetCaretRect` 把
  `armed_index` 传给 `UiBridge::PushShow`。
- `src/domains/input-engine/backend/ui_bridge.{hpp,cpp}`：`PushShow` 新增
  `armed_index` 参数，写入 `uiShow` 的 `armedIndex` 字段。
- `src/shared/ipc-protocol/protocol.hpp`：`PROTOCOL_VERSION` 6。
- `src/domains/candidate-ui/logic/candidate_view_model.hpp`：`armed_index` 字段。
- `src/domains/candidate-ui/ui/host_main.cpp`：解析 `armedIndex`。
- `src/domains/candidate-ui/backend/candidate_window.cpp`：高亮渲染。
- 不影响 `src/domains/tsf-service/`（TIP 侧按键路由已经是"把 vk/ch 转发给引擎，
  按响应里的 composing/commit 决定要不要维持 ITfComposition"的通用逻辑，不需要
  知道空格具体做了什么）。

## 验证

真实 IPC 端到端测试（`space_arm_test.py`、`space_arm_number_mode_test.py`）：
- 多候选（`nihao`）：第一次空格 `composing:true` 无 `commit`，`armedIndex:0`；
  第二次空格提交候选[0]。
- 数字键（`nihao`+"1"）：一步直接提交，不受两段式影响。
- 架住后继续打字（`nihao`+"a"）：`armedIndex` 清回 -1。
- 数字模式（`i2025`）：跟拼音候选同一套两段式行为，验证一致性。
- M4/M5 既有回归测试（bihuo/number/GBK/造词/自学习/导出导入）全部重跑通过，
  确认这次改动没有破坏已有功能。

## 关联

- `docs/decisions/_debt-log.md` 2026-09-11 置顶条目（简短版，指回本文档）。
- 回归判据：`docs/check/`（本次未走独立里程碑质检流程，纳入下一次质检报告的
  回归判据集合）。
