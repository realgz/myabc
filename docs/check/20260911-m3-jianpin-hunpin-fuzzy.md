# M3 质检报告 · 简拼 / 混拼 / 模糊音

- 日期：2026-09-11
- 里程碑：M3（docs/plan/04-m3-jianpin-hunpin-syllable-plan.md）
- 执行方：主会话（Sonnet）
- 结论：**核心转换正确性全部通过**（简拼/混拼/v→ü/模糊音）。切分歧义的展示层
  （xi'an 隔音号、`segments` 字段、协议 v3）有意不做，见 §3 决策；对应地
  PROTOCOL_VERSION 维持 2。

## 1. 本轮交付

- **`LibPinyinEngine::ApplyInputOptions(incomplete_enabled, fuzzy_names)`**
  （`backend/libpinyin_wrapper.{hpp,cpp}`）：按 `config.input.scheme` 切
  `PINYIN_INCOMPLETE`（quanpin 关/jianpin·hunpin 开），按 `config.input.fuzzy`
  叠加 `PinyinAmbiguity2` 位（`c_ch/s_sh/z_zh/f_h/g_k/l_n/l_r/an_ang/en_eng/in_ing/all`）。
- `config_defaults.hpp`：`input.scheme` 默认改 `"hunpin"`（对标智能ABC习惯）。
- `engine_main.cpp`：`ApplySchemeAndFuzzy()` 在 `--selftest` 与正常启动都调用一次
  （engine 全局设置，非按键级）。
- `tests/unit/engine_conversion_test.cpp`：扩充 M3 golden 用例（简拼/混拼/切分歧义/
  v→ü/模糊音），沿用 M1 的无框架 assert 风格。

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| M3-1 bj | 候选命中"北京"（非 top1，见 §3 决策） | golden test PASS |
| M3-2 bjing | top-of-list 附近命中"北京" | golden test PASS |
| M3-3 beij | 候选命中"北京" | golden test PASS |
| M3-4 xian | 候选同时含"西安"与"现" | golden test PASS（两条断言） |
| M3-5 nver/nvhai | top1 分别是"女儿"/"女孩" | golden test PASS |
| M3-7 模糊音 z_zh | 开启后 zongguo 候选命中"中国" | golden test PASS |
| M3-6 ctest | 全过 | `engine_conversion` PASS |
| M3-8 `tests/review/run_all` | GREEN | check_hardcode 扫描 66 源文件 0 违规 |
| 回归：M1/M2 判据 | 未见回归 | `ipc_roundtrip` 仍过；quanpin 显式关 incomplete 后 nihao/woshizhongguoren/beijing 仍正确（golden test 前 3 条） |
| IPC 端到端（真实 processKey，默认 hunpin） | PASS | 输入 `beij`（混拼），`composing:true`，未报错 |

## 3. 计划偏差 / 决策（详见 docs/decisions/_debt-log.md 2026-09-11 置顶各条）

1. **不建 `jianpin_parser`/`hunpin_parser` 两个文件**：探查后发现 libpinyin 对两者用
   同一个 `PINYIN_INCOMPLETE` 开关处理，没有需要分支/适配的行为差异——建两个内容相同
   的类违反"不为不存在的差异过度设计"。
2. **模糊音真正接线**（`config.input.fuzzy` 从 M1 起就存在但未使用，本轮补上）。
3. **`bj` 不是 top1"北京"**：libpinyin 默认语料对孤立简拼的统计排序如此，如实记录，
   golden test 按"候选命中"断言，不用取巧手段凑一个假 top1。
4. **切分歧义展示层（segments/隔音号/协议 v3）未做**：核心转换正确性不依赖它
   （libpinyin 已经在候选列表里正确给出多种切分的候选），纯 UI 可读性锦上添花，
   工作量与收益不匹配，留到有实际反馈再评估。

## 4. 回滚锚点

`git tag m3-jianpin`（若后续追加，先确认无残留测试进程/临时目录）。
