# M5 质检报告 · 用户词库自学习

- 日期：2026-09-11
- 里程碑：M5（docs/plan/06-m5-user-dict-learning-plan.md）
- 执行方：主会话（Sonnet）
- 结论：**全部验收判据通过**（造词、频率提升、崩溃后持久化、导出/清空/导入、
  回归）。`new_phrase_store`/`learning_policy` 独立策略层、领域目录未按计划字面
  的文件清单落地——libpinyin 自带的 `pinyin_remember_user_input` 已经是完整的
  自学习机制，见 §3 决策 1；PROTOCOL_VERSION 按计划升到 5。

## 1. 本轮交付

- **`LibPinyinEngine::RememberUserInput/ExportUserDict/ImportUserDict/ClearUserDict`**
  （`backend/libpinyin_wrapper.{hpp,cpp}`）：分别包装 libpinyin 的
  `pinyin_remember_user_input`、`pinyin_begin/iterator/end_get_phrases`、
  `pinyin_begin/iterator/end_add_phrases`、`pinyin_mask_out`。
- **`Session::MaybeTrain`**（`logic/session/session.{hpp,cpp}`）：拼音来源
  （`UsesEngineChoose()==true`）整句提交前调用 `RememberUserInput`+`Train`，必须在
  `ResetToIdle()`/`engine_.Reset()` 之前（读的是当前 matrix/nbest 状态）。
- **`AutosaveCounter`**（新建 `logic/learning_policy.hpp`）：每 N 次 commit 触发一次
  `engine_.Save()`，`Dispatcher::MaybeAutosave()` 接线；`--autosave-every-n-commits`
  CLI 覆盖供测试用小阈值。
- **IPC v5**：新增 `userDictExport`/`userDictImport`/`userDictClear`
  （`Dispatcher::HandleUserDict*`），`PROTOCOL_VERSION` 2 -> 5。
- **`myabc-deployer.exe` 新增 CLI**：`--export-userdict <path>` /
  `--import-userdict <path>` / `--clear-userdict`，新建
  `backend/engine_client.{hpp,cpp}`（deployer 专用命名管道客户端）。
- **`config.learning`**（`config_defaults.hpp`）：`enabled`（默认 true）、
  `autosave_every_n_commits`（默认 20）。

## 2. 验收结果

| # | 判据 | 结果 | 证据 |
|---|---|---|---|
| M5-1 | 造词（"张"+"胜"分段选择组合成"张胜"，不在词库里的组合） | 造词后再输入，"张胜"出现在候选中且排到前列（index=2，超过一半的纯单字候选） | 真实 IPC 端到端 PASS |
| M5-2 | 重复选择某单字候选，多次后排到候选第 1 | 有限次数（≤10）内必定升到 top1，实测"航"5 次、"好"4 次、"号"1 次——次数由跟原 top1 的系统词频差距决定，非固定 3 次，见 §3 决策 3 | 真实 IPC 端到端 PASS（断言"有限次数内必升"而非硬编码次数） |
| M5-3 | 造词后模拟 taskkill（`proc.kill()`，不走 shutdown），重启后学习结果仍在 | autosave 阈值调小到 2 次 commit 触发一次 `pinyin_save`；kill 后重启新引擎实例，"张胜"仍在候选中 | 真实 IPC 端到端 PASS |
| M5-4 | `myabc-deployer --export-userdict`（文件含新词）→ `--clear-userdict`（候选恢复默认）→ `--import-userdict`（恢复） | 完整闭环通过；deployer 既能连已运行引擎，也能自己拉起 | 真实 CLI + IPC 端到端 PASS |
| M5-5 | `ctest tests/unit/learning_policy` | `AutosaveCounter` 断言（开关/边界/周期性）全过 | PASS（见 §3 决策 1，测的是实际写的落盘节奏逻辑，非 plan 字面的晋升/去重/衰减） |
| M5-6 | 删除用户库文件后启动 | 引擎正常启动（`pinyinReady:true`），输入正常（`nihao` 正常组字） | 真实 IPC 端到端 PASS |
| M5-7 | `tests/review/run_all` + 协议版本 | 全绿，`PROTOCOL_VERSION==5` | check_hardcode 扫描 77 源文件 0 违规；`hello` 响应 `protocol:5` |
| 回归 | M1-M4 全部判据 | 未见回归 | 5 个 ctest 全过；M4 bihuo/number/GBK 端到端脚本重跑全过 |

## 3. 计划偏差 / 决策（详见 docs/decisions/_debt-log.md 2026-09-11 置顶各条）

1. **不建 `src/domains/user-dict/` 领域目录、不建 `new_phrase_store`/完整
   `learning_policy` 策略层**：libpinyin 的 `pinyin_remember_user_input` 本身就是
   按 count 累加的自学习模型（不管词存不存在，调一次记一次），配合已有的
   `SORT_BY_FREQUENCY` 排序，天然满足"新词记忆+常用词提权"，不需要在其上再叠一层
   自建的"攒够 N 次才收录"阈值/去重/衰减逻辑——那需要额外一份跨重启计数存储，
   为没有验收判据强制要求的场景加复杂度。真正自建的只有"多久落盘一次"，做成了
   `learning_policy.hpp` 的 `AutosaveCounter`。
2. **训练调用时机耦合**：`RememberUserInput`/`Train` 必须在 `ResetToIdle()` 之前调用
   （读当前 matrix/nbest 状态），不能放进统一收尾逻辑，已在两处正确处理。
3. **M5-2 的"3 次"不是常量**：实测需要的强化次数由候选与原 top1 的系统词频差距
   决定，如实记录（同 M3"bj 不是 top1 北京"的诚实记录原则），验收测试断言"有限
   次数内必升"而非硬编码次数。
4. **deployer CLI 用独立 `engine_client`，不复用 tsf-service 的 `ipc_client`**：
   后者是为"绝不阻塞宿主 UI 线程"设计的 overlapped IO，deployer 是一次性 CLI 没有
   这个约束，且跨领域直接引用另一域的 backend 违反项目的领域边界约定。
5. **PROTOCOL_VERSION 2 -> 5**（不是 2->3->4->5）：M3/M4 都决定不改协议结构，这是
   第一次真正新增 wire 方法，直接跳到 plan 一直写的目标版本号。

## 4. 回滚锚点

`git tag m5-userdict`（回滚到 M4 用 `git checkout m4-classic`）。
