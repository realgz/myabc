# 04 · M3 计划（简拼 / 混拼 / 音节切分歧义）

- 状态：生效
- 更新日期：2026-09-09
- 前置：M2（tag m2-thin-dll）
- 对应需求：3.2 简拼/混拼、音节自动切分
- 回滚锚点：tag m3-jianpin

## 1. 背景与目标
M1/M2 仅全拼。M3 加：bj->北京（简拼）、beij/bjing（混拼）、xian 的 xi'an 与 xian 歧义处理与提示。

## 2. 现状分析
- input-engine/logic/scheme：只有 QuanpinParser。
- libpinyin 原生支持 incomplete pinyin（简拼）与 full/incomplete 混合，通过 pinyin_option_t
  的 PINYIN_INCOMPLETE / USE_DIVIDED_TABLE / USE_RESPLIT_TABLE 等开关；无需自己写切分器，
  但需要把开关接到 config 与方案适配器。

## 3. 文件级改造点

### 3.1 input-engine/logic/scheme
| 文件 | 内容 |
|---|---|
| jianpin_parser.cpp/.h | 开 PINYIN_INCOMPLETE；ParseResult 标注 incomplete 音节 |
| hunpin_parser.cpp/.h | 全拼+简拼混合；本质是 incomplete 开关 + 完整解析，合并候选 |
| scheme_registry | 注册 jianpin/hunpin；config.input.scheme 可选 quanpin/jianpin/hunpin |
| syllable_segmenter.cpp/.h | 用 pinyin_parse 的分词结果 + resplit/divided 表；对存在多切分（xian = xi+an 或 xian）
  的输入，产出 segmentation 候选列表并标记歧义 |

### 3.2 input-engine/logic/session
- Session.preedit 显示带隔音号：xi'an（在歧义点插入 '）。
- 候选列表：歧义时同时给"西安"（xi'an）与"仙/闲/咸..."（xian）两组，注释标切分方式。
- 新增 result 字段 segments:[{text,start,len}] 供 UI 显示切分。

### 3.3 IPC 协议
- result 增 segments、preeditDisplay（带隔音号）。PROTOCOL_VERSION -> 3。更新两侧 + 架构文档。

### 3.4 candidate-ui
- renderer：预编辑行按 segments 渲染，歧义分隔点显示浅色 '。

### 3.5 config
- input.scheme 默认改为 hunpin（对标智能ABC习惯），可配。
- input.fuzzy 增加常见模糊音开关映射表（zh/z、ch/c、sh/s、an/ang、in/ing、l/n...）。

## 4. 不改动清单
- 不做双拼（预留 ShuangpinParser 接口即可，不实现）。
- 不做笔形/数字/GBK（M4）。
- 不改全拼路径已通过的判据。
- 排序模型不换（仍 libpinyin 默认）。

## 5. 可执行验收判据
| # | 输入 | 期望 top 候选 |
|---|---|---|
| M3-1 | bj + 空格 | 北京 |
| M3-2 | bjing | 北京 命中候选 |
| M3-3 | beij | 北京 命中候选 |
| M3-4 | xian | 候选含 西安 与 现/县/先 等；预编辑可显示 xi'an 切分选项 |
| M3-5 | nvhai / nver | 女孩 / 女儿（含 v->ü 处理） |
| M3-6 | ctest tests/unit/engine_segmentation | 上述全部断言通过，含 fangan(方案) vs fang'an 分隔 |
| M3-7 | 模糊音开启 zh=z 后输入 zongguo | 候选含 中国 |
| M3-8 | tests/review/run_all + 协议版本检查 | 全绿，PROTOCOL_VERSION==3 |

回归：M1/M2 判据全部重跑通过。

## 6. 风险与回滚
- 简拼候选爆炸导致性能/噪音：限制 incomplete 仅在音节数 <= N 时启用；候选数上限截断。
- 隔音号显示与用户输入 ' 冲突：用户输入的 ' 作显式分隔，UI 用不同样式区分自动/手动。
- 回滚：git checkout m2-thin-dll。
