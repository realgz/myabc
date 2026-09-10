# 06 · M5 计划（用户词库自学习）

- 状态：生效
- 更新日期：2026-09-09
- 前置：M4（tag m4-classic）
- 对应需求：3.1 用户词库自学习（记忆新词、调整词频）
- 回滚锚点：tag m5-userdict

## 1. 背景与目标
让引擎记住用户选择：新词永久收录、常用词词频上升、跨重启保留、可导出/清空。

## 2. 现状分析
- M1 起 pinyin_choose_candidate 已让本会话自适应，但未确认 pinyin_train + pinyin_save 的持久化策略。
- user-dict domain 目录在架构里定义但未建。
- config.engine.user_data_dir、learning.enabled、learning.min_uses_to_promote 已在 config 清单。

## 3. 文件级改造点

### 3.1 src/domains/user-dict/（新建，MinGW）
| 文件 | 内容 |
|---|---|
| backend/user_store.cpp/.h | 封装 libpinyin 用户 bigram/unigram：pinyin_train(instance) 后 pinyin_mask_out / pinyin_save；
  用户库文件路径 = config.engine.user_data_dir；原子保存（写临时文件 + g_rename，见上游 PR #189） |
| backend/new_phrase_store.cpp/.h | 自建新词表（拼音串 -> 词面 -> 使用次数），SQLite? 否——用 libpinyin 的 addon phrase 或简单二进制；
  DbBackend 适配器（BerkeleyDbBackend / KyotoCabinetBackend）在此使用 |
| logic/learning_policy.cpp/.h | 何时 commit 学习：整句上屏后调 pinyin_train；未在词库、被选 >= min_uses_to_promote 次的连续串
  提升为新词条；衰减：长期未用词频缓慢下调（可选，配置开关）|
| logic/user_dict_admin.cpp/.h | 导出（文本：拼音 词 频次）、导入、清空；供 deployer 或 CLI 调用 |

### 3.2 input-engine 集成
- dispatcher.commitComposition / selectCandidate 成功后 -> learning_policy.OnCommit(context)。
- initSession 时加载用户库；idle_exit / shutdown 时 user_store.Save()。
- 定时自动保存（每 N 次 commit 或每 M 分钟），防崩溃丢失。

### 3.3 IPC 协议
- 新增方法：userDictExport / userDictImport / userDictClear（管理用，非热路径）。PROTOCOL_VERSION -> 5。

### 3.4 deployment / CLI
- myabc-deployer.exe 增 --export-userdict <path> / --import-userdict <path> / --clear-userdict
  （或独立 myabc-userdict.exe）。

### 3.5 config
- learning.enabled、learning.min_uses_to_promote、learning.autosave_every_n_commits、
  learning.decay_enabled。

## 4. 不改动清单
- 不做云同步。
- 不改转换算法本身（只喂训练数据）。
- 不改 M1-M4 判据。
- 用户库损坏时静默重建，不影响输入（降级为无学习）。

## 5. 可执行验收判据
| # | 步骤 | 期望 |
|---|---|---|
| M5-1 | 输入并上屏一个不在词库的人名（如 "zhang" + 自造名"甡"组合 zhangsheng -> 选字造词） | 再次输入 zhangsheng，该词进入候选前列 |
| M5-2 | 重复输入某词 3 次选它 | 第 4 次它排到候选第 1 |
| M5-3 | 造词后 taskkill 引擎（触发 autosave 或 shutdown save），重启，再输入 | 学习结果仍在 |
| M5-4 | myabc-deployer --export-userdict d:\tmp\ud.txt | 文件含刚学的词；--clear-userdict 后候选恢复默认；--import 回来恢复 |
| M5-5 | ctest tests/unit/learning_policy | 晋升阈值、去重、衰减逻辑断言通过 |
| M5-6 | 回归：删除用户库文件后启动 | 引擎正常启动，输入正常（无学习） |
| M5-7 | tests/review/run_all + 协议版本 | 全绿，PROTOCOL_VERSION==5 |

回归判据（历史真实坑）：
- 上游 PR #188/#189（O_BINARY、g_rename）：用户库 save/reload round-trip 在 Windows 数据一致，
  且保存中断（模拟 kill）不损坏原库（临时文件 + rename 语义）。

回归：M1-M4 全过。

## 6. 风险与回滚
- 并发：多个宿主进程同一引擎单实例，用户库写串行化（引擎内单写线程）。
- 学习污染：误选导致坏词上升 -> 提供 clear + 单词删除（管理 CLI）。
- 回滚：git checkout m4-classic；用户库格式若变，导出为文本再回退。
