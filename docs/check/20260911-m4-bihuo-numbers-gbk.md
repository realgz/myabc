# M4 质检报告 · 笔形辅助码 / i 中文数字·金额 / GBK 生僻字

- 日期：2026-09-11
- 里程碑：M4（docs/plan/05-m4-bihuo-numbers-gbk-plan.md）
- 执行方：主会话（Sonnet）
- 结论：**三项核心能力全部通过端到端验证**。笔形辅助码触发键因跟既有
  `select_keys` 行为冲突改用独立触发键（反引号），协议/展示层扩展
  （IPC `tag` 字段、PROTOCOL_VERSION -> 4、UI 角标）有意不做，见 §3 决策。

## 1. 本轮交付

- **`src/shared/cn-number`**（纯函数，无业务/无副作用，符合 code-quality-standards
  §1）：`PlaceValueChinese`/`DigitByDigitChinese`/`UppercaseCurrency`/
  `ParseDecimalToCents`/`ParseInteger`。`DigitByDigitChinese` 用"〇"（年份读法），
  `PlaceValueChinese` 遵循"两/二"惯例。
- **`logic/candidate/number_currency_source.cpp`**：识别 `config.input.number_lead_key`
  （默认 `i`）引导的数字/金额输入，产出小写中文/大写金额/逐位读法/原样阿拉伯数字
  多种候选；插入 `source_registry` 的 punctuation 之前。
- **`backend/bihuoma_table.{hpp,cpp}`** + **`logic/candidate/bihuo_filter.{hpp,cpp}`**：
  字->笔形码查表（fail-open：未收录字符不参与筛选）、拼音+笔形码拆分与过滤。
- **笔形触发键改独立按键**（`config.input.bihuo_lead_key`，默认反引号 `` ` ``）：
  plan 字面示例"拼音后直接接数字"（`wo3`）与既有 `select_keys`（默认全体数字）
  的既有行为无法共存，改为先按触发键进入笔形态（`` wo` ``），见 §3 决策 1。
- **`assets/data/bihuoma.txt`**：8 字高置信度种子表（一二三十/人八入/大），见 §3 决策 2。
- **GBK 生僻字**：确认无需新代码——libpinyin 默认词库自 M1 起已含 `gbk_char.bin`，
  见 §3 决策 3。
- `session.cpp`：组字态按键路由新增 `in_number_mode`/笔形触发键两条独立分支，均先于
  `select_keys` 判断，互斥（由 `raw_` 首字符区分）、且不影响原有 select_keys 行为
  （回归验证见下）。

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| M4-1/M4-2 笔形过滤（`` wo`3 ``/`` ji`3 `` 类，触发键改动见决策 1） | 过滤生效，未收录字符 fail-open | `ctest bihuoma_filter`（合成表）+ 真实 IPC 端到端：`` yi`1 ``保留"一"（表中码=1），`` yi`3 `` 过滤掉"一"（不匹配），`ren` 系列同理，`` nihao`3 `` fail-open 不清空候选 |
| 回归：select_keys 未受影响 | PASS | 端到端：`ni`+`1` 仍直接选字上屏"你"（未受笔形改动影响） |
| M4-3 `i2025` | 候选含"二〇二五"、"两千零二十五" | 真实 IPC 端到端 PASS |
| M4-4 `i1234.56` | 候选含"壹仟贰佰叁拾肆圆伍角陆分" | 真实 IPC 端到端 PASS |
| M4-5 `i100` | 候选含"一百"、"壹佰圆整" | 真实 IPC 端到端 PASS |
| M4-6 GBK 生僻字"喆" | 出现在候选中（`zhe` 第 2 页）且可正常选中上屏 | 真实 IPC 端到端 PASS（分页查找 + selectCandidate 验证 commit=="喆"） |
| M4-7 `ctest cn_number` | 全过（0/10/100/1005/20000/1.5/负数/金额边界） | PASS |
| M4-8 `ctest bihuoma_filter` | 已知字笔形码断言通过（含空后缀、未收录字符、前缀不匹配三类） | PASS |
| M4-9 `tests/review/run_all` + 协议版本 | GREEN，PROTOCOL_VERSION 维持 2（见决策 3，未升级） | check_hardcode 扫描 74 源文件 0 违规；check_structure PASS |
| 回归：M1-M3 判据 | 未见回归 | `ipc_roundtrip`/`engine_conversion` 仍过；`i2025`/数字模式与笔形模式互斥未互相干扰 |

## 3. 计划偏差 / 决策（详见 docs/decisions/_debt-log.md 2026-09-11 置顶各条）

1. **笔形辅助码触发键改用独立按键（反引号），不是 plan 字面示例的"拼音后直接接
   数字"**：`select_keys` 默认就是全体数字（M1 起既有行为），跟"拼音后数字=笔形码"
   永久无法共存（不是加个模式判断能解决的，两者都要用裸数字表达不同意图）。改为
   触发键方案后，笔形过滤能力本身与 plan 设计的过滤语义（前缀匹配、fail-open）
   完全一致，只是"怎么进入笔形输入态"变了。**经验**：`bihuo_filter` 的纯函数单测
   一直是绿的，因为它只测"给定一个带数字后缀的字符串，拆分/过滤对不对"，没测过
   "真实按键状态机能不能产生这样的字符串"——这类"数据到达性"问题只有端到端测试
   才能发现，是本项目第三次遇到同类教训（M1 CWD bug、M2 id 冲突 bug）。
2. **笔形数据表只有 8 个字**：没有可靠、可验证 License 的权威笔顺数据源；错误的
   笔形码会把正确候选筛掉，比没有这个功能更糟。先证明链路是通的，覆盖高频字
   留待数据源确定后补（补数据不需要改代码）。
3. **GBK 生僻字未新增 `gbk_filter`/IPC `tag` 字段/协议版本升级**：libpinyin 默认
   词库自 M1 起已含 GBK 字集，验收判据指定的"喆"本来就能正常输入，没有实际功能
   依赖 tag 字段（纯展示层锦上添花）。PROTOCOL_VERSION 维持 2。

## 4. 回滚锚点

`git tag m4-classic`（回滚到 M3 用 `git checkout m3-jianpin`）。
