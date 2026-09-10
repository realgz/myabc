# 05 · M4 计划（笔形辅助码 + i 中文数字/金额 + GBK 生僻字）

- 状态：生效
- 更新日期：2026-09-09
- 前置：M3（tag m3-jianpin）
- 对应需求：3.2 笔形辅助码、中文数字/金额大写、GBK 生僻字
- 回滚锚点：tag m4-classic

## 1. 背景与目标
补齐智能ABC三大标志特性：
- 笔形辅助码：拼音后接 1-5（横竖撇捺折）作二级筛选。
- i 引导：i2025 -> 二〇二五 / 两千零二十五；iRMB 或金额模式 -> 壹仟... 大写。
- GBK：候选覆盖 GBK 字集（生僻字可选可上屏）。

## 2. 现状分析
- input-engine/logic/candidate/source_registry：english/punctuation/pinyin 三源，number 是占位。
- charset：UnicodeFilter/Gb2312Filter 有，GbkFilter 占位。
- 无笔形数据表。

## 3. 文件级改造点

### 3.1 笔形辅助码
| 文件 | 内容 |
|---|---|
| assets/data/bihuoma.txt | 字 -> 笔形码序列（横1竖2撇3捺4折5）。来源：从开源五笔/笔画数据转换，或智能ABC笔形规则表；在 debt-log 记来源与 License |
| input-engine/backend/bihuoma_table.cpp/.h | 加载 bihuoma.txt 为 hash（char32 -> 笔形串） |
| input-engine/logic/candidate/bihuo_filter.cpp/.h | 在拼音候选产出后，若 raw 末尾有 1-5 数字段，按笔形码前缀过滤候选 |
| session：raw 拆成 pinyin 段 + bihuo 段；config.input.bihuo_enabled 控制 |

### 3.2 i 引导数字/金额
| 文件 | 内容 |
|---|---|
| input-engine/logic/candidate/number_currency_source.cpp/.h | Handles：raw 以 config.input.number_lead_key（默认 i）开头。Produce：解析后续数字/小数点，产出多种表示：
  小写中文（一二三/十百千万亿）、大写金额（壹贰叁/拾佰仟万圆整角分）、阿拉伯原样、〇年份式 |
| input-engine/shared/cn_number.cpp/.h | 纯函数库：long/double -> 各种中文表示；放 src/shared（无业务、纯技术） |
| source_registry：把 number_currency 插到 punctuation 之前 |

### 3.3 GBK 生僻字
| 文件 | 内容 |
|---|---|
| input-engine/logic/charset/gbk_filter.cpp/.h | 实现：允许 GBK 可编码字通过；标记非 GB2312 字为"生僻"（UI 可加标记）。用 Windows WideCharToMultiByte(936) 判定可编码性 |
| config.output.charset 默认 gbk |
| pinyin_candidate_source：确保 libpinyin 词库含 GBK 大字集词库（pinyin_load_phrase_library 加载扩展库；若词库仅 GB2312，需在 06/数据步骤补 CJK 扩展） |

### 3.4 IPC 协议
- candidate item 增 tag 字段（rare/uppercase/number 等）。PROTOCOL_VERSION -> 4。

### 3.5 candidate-ui
- renderer：rare 字加角标或变色；数字/金额候选整行显示。

## 4. 不改动清单
- 笔形只做辅助筛选，不做纯笔形输入（无拼音时不进笔形）。
- i 引导不做日期运算/大小写英文（仅数字/金额）。
- 不改 M1-M3 判据。
- GBK 之外字集（GB18030 四字节、生僻扩展 B 区以上）不强求。

## 5. 可执行验收判据
| # | 输入 | 期望 |
|---|---|---|
| M4-1 | ji3（几撇？取 jǐ + 笔形3） | 候选被笔形码筛选，结果都以撇起笔 |
| M4-2 | wo3 | 候选限笔形起笔为撇的字（我...） |
| M4-3 | i2025 | 候选含 二〇二五、两千零二十五 |
| M4-4 | i1234.56 | 候选含 壹仟贰佰叁拾肆圆伍角陆分 |
| M4-5 | i100 | 候选含 一百、壹佰圆整 |
| M4-6 | 输入某 GBK-only 字的拼音（如 duo "escape"字 tǎ 𰻝 用 biang 不行，用 "喆" zhe） | "喆"出现在候选中且可上屏，记事本显示正确 |
| M4-7 | ctest tests/unit/cn_number | 覆盖 0、10、100、1005、20000、1.5、负数、金额边界（分/角/整）全通过 |
| M4-8 | ctest tests/unit/bihuoma_filter | 已知字笔形码断言通过 |
| M4-9 | tests/review/run_all + 协议版本 | 全绿，PROTOCOL_VERSION==4 |

回归：M1-M3 判据全过。

## 6. 风险与回滚
- 笔形数据质量参差：先小表覆盖高频 3500 字，未覆盖字不参与笔形筛选（不报错、不丢候选）。
- 金额大写规则细节（零的处理、圆/元、整）：以国标 GB/T 15835 为准，写进 cn_number 单测。
- GBK 词库缺失：若 libpinyin 默认库不含，则 06 数据步骤补；M4 先保证 filter 逻辑正确。
- 回滚：git checkout m3-jianpin。
