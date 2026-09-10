# 02 · M1 计划（libpinyin 全拼智能整句 + 候选窗）

- 状态：生效
- 更新日期：2026-09-09
- 前置：M0（tag m0-skeleton）通过
- 对应需求：3.1 MVP 智能拼音（全拼、整句转换、候选翻页、数字选字、中英切换、标点、词间/部分上屏/回退）
- 回滚锚点：完成后打 tag m1-quanpin

## 1. 背景与目标

问题定位：M0 链路通但引擎不会转换，DLL 只会上屏写死的字。M1 让引擎接 libpinyin 做全拼整句，
TIP 走完整 processKey 往返，候选窗显示候选，用户能翻页选字上屏。

完成后可观察效果（记事本）：
- 输入 nihao -> 预编辑串显示，候选首项"你好"，按 1/空格上屏。
- 输入 woshizhongguoren -> 候选整句"我是中国人"。
- - / = 翻页，1-9 选字，退格删拼音，ESC 取消，Shift 切英文直接上屏，逗号句号映射中文标点。

## 2. 现状分析

- M0：input-engine/backend/libpinyin_wrapper.* 是空壳；logic/dispatcher processKey 回 handled:false。
- M0：tsf-service/logic/composition_state 只有 Idle；key_event_sink 只认 'A'。
- M0：无候选窗（candidate-ui 目录不存在）。
- libpinyin API（已知形态，实现时以 third_party/libpinyin/src/pinyin.h 为准）：
  pinyin_init / pinyin_load_phrase_library / pinyin_alloc_instance / pinyin_parse_more_full_pinyins /
  pinyin_guess_sentence / pinyin_guess_candidates / pinyin_choose_candidate /
  pinyin_train / pinyin_save / pinyin_free_instance / pinyin_fini。

## 3. 文件级改造点

### 3.1 input-engine/backend/libpinyin_wrapper.cpp/.h（填实现）

- LibPinyinEngine 类：
  - ctor(model_dir, user_dir)：pinyin_init；加载系统词库；pinyin_alloc_instance。
  - SetFuzzyOptions(flags)：映射 config.input.fuzzy -> pinyin_option_t，pinyin_set_options。
  - ParseFull(const std::string& pinyin)：pinyin_parse_more_full_pinyins + 更新 instance。
  - GuessSentence()：pinyin_guess_sentence -> 取 nbest 整句串。
  - Candidates(size_t offset)：pinyin_guess_candidates(offset) -> 遍历 token 取词面 + 频率信息。
  - Choose(index)：pinyin_choose_candidate -> 返回新的已确定前缀 + 剩余拼音。
  - Reset()：清 instance。
- DECISION 注释指向 pinyin-engine ADR。
- 所有 glib 字符串转 std::string（UTF-8）在此层完成，不外泄 gchar*。

### 3.2 input-engine/logic/ 新增

| 文件 | 内容 |
|---|---|
| scheme/input_scheme_parser.h | 接口 InputSchemeParser { ParseResult Parse(const std::string& raw); }；ParseResult{ syllables, valid_prefix_len } |
| scheme/quanpin_parser.cpp/.h | 用 wrapper.ParseFull；M1 只此一个实现 |
| scheme/scheme_registry.cpp/.h | resolve(config.input.scheme) -> InputSchemeParser*；M1 只注册 quanpin |
| candidate/candidate_source.h | 接口 CandidateSource { bool Handles(const InputContext&); CandidateList Produce(const InputContext&, Page); } |
| candidate/pinyin_candidate_source.cpp/.h | 包 LibPinyinEngine：整句候选置顶 + 逐词候选 |
| candidate/english_passthrough_source.cpp/.h | 英文模式：raw 原样为唯一候选 |
| candidate/punctuation_source.cpp/.h | 中文模式标点：, 。 ； 等映射表（映射表在 assets/config/punctuation.toml，不硬编码） |
| candidate/source_registry.cpp/.h | 按 InputContext（模式、首字符、是否组字中）resolve；顺序：english -> number(占位,M4) -> punctuation -> pinyin |
| charset/charset_filter.h + gb2312/gbk/unicode_filter.cpp | M1 先实现 UnicodeFilter（透传）与 Gb2312Filter；GbkFilter 占位到 M4。按 config.output.charset resolve |
| session/session.cpp/.h | 每 sessionId 一份：当前 raw 拼音、已确定前缀、page、scheme、mode；Feed(vk,ch,mods) -> 更新并产候选 |
| session/session_manager.cpp/.h | sessionId -> Session；initSession/focusOut 清理 |

### 3.3 input-engine/logic/dispatcher.cpp（扩展）

- initSession：建 Session，应用 config。
- processKey：Session.Feed -> 组装 result{preedit, rawInput, candidates(当前页), page{index,size,total}, commit?}。
  - 字母 -> 追加拼音，重算候选。
  - 数字 1-9 -> 选当前页第 n 个候选；若整句被完全确定 -> commit 该串并 Reset。
  - 空格 -> 选第 1 候选（同上）。
  - config.candidates.page_prev/next 键 -> 翻页。
  - 退格 -> 删末位拼音；空则 handled:false。
  - ESC -> 取消，清 Session，handled:true 无 commit。
  - 非组字态收到字母且中文模式 -> 起组字。
- selectCandidate / pageCandidates / commitComposition / cancelComposition：对应 Session 操作。
- engine_main --selftest：真实现——pinyin_init(model_dir) -> ParseFull("nihao") -> GuessSentence
  -> 断言含"你好"，打印 sentence[0]，退出码 0/1。

### 3.4 src/domains/candidate-ui/（新建，MSVC，M1 静态库链入 DLL）

| 文件 | 内容 |
|---|---|
| backend/candidate_window.cpp/.h | WS_EX_LAYERED\|WS_EX_TOOLWINDOW\|WS_EX_NOACTIVATE 无焦点顶层窗；Per-Monitor-V2 DPI；Show(rect near caret)/Hide/Update(model) |
| backend/window_thread.cpp/.h | 候选窗自己的线程 + 消息泵（不占 TIP 线程） |
| ui/renderer_d2d.cpp/.h | Direct2D + DirectWrite 绘制：拼音行 + 候选项（序号 文本 注释）+ 翻页箭头；字体/颜色来自 config.ui |
| logic/candidate_view_model.cpp/.h | 纯数据：items、highlightIndex、pageIndex、pageCount、preedit；由 TIP 从 IPC result 填充 |
| CMakeLists.txt | add_library(myabc-candidate-ui STATIC ...)；链 d2d1/dwrite/dcomp/windowscodecs |

### 3.5 tsf-service 改造

| 文件 | 现状 | 目标 |
|---|---|---|
| logic/composition_state.cpp/.h | 只有 Idle | 状态机：Idle/Composing；持有当前 sessionId、preedit、候选 view model；处理 IPC result 决定：更新预编辑（ITfComposition + ITfRange::SetText）、显示/更新/隐藏候选窗、执行 commit（EditSession 写文本 + 结束 composition） |
| backend/key_event_sink.cpp | 只认 'A' | OnTestKeyDown：查 logic/key_router，组字态吃掉字母/数字/翻页/退格/ESC/空格；非组字态且中文模式，字母键预吃。OnKeyDown：组织 processKey/selectCandidate/pageCandidates 请求 -> IPC（有界等待 request_timeout_ms）-> 交 composition_state 应用。超时降级：结束组字、放行原键 |
| backend/composition.cpp/.h（新增） | — | ITfComposition 管理：StartComposition（EditSession 内）、更新 range 文本、EndComposition；ITfCompositionSink::OnCompositionTerminated 处理外部中止 |
| backend/ipc_client.cpp | 只发 hello | 完整请求/响应；连接丢失重连；请求 id 递增匹配 |
| backend/display_attribute.cpp/.h（新增，可选 M1 末或 M2） | — | 注册 GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER + ITfDisplayAttributeProvider，给预编辑串加下划线 |
| logic/mode_manager.cpp/.h（新增） | — | 中/英模式；Shift 单击切换（OnKeyUp 检测无组合的 Shift）；英文模式字母直接放行 |
| logic/punctuation_map.cpp/.h（新增） | — | 从 IPC 侧拿？否——标点在中文模式直接由引擎 punctuation_source 处理，TIP 只转发。TIP 本地只保留全角/半角开关状态 |

### 3.6 config 扩展

- 补全 config_defaults.hpp 的 candidates.*、input.scheme、input.fuzzy、output.charset、ui.*。
- config_loader 真正解析 config.toml（引入 header-only toml 到 third_party/）。
- assets/config/punctuation.toml、assets/config/config.sample.toml。

## 4. 不改动清单

- 不做简拼/混拼/音节切分歧义提示（M3）。
- 不做笔形辅助码、i 数字、GBK 扩展（M4）。
- 不做用户自学习的持久化策略（M5）；M1 可调用 pinyin_train 让本次会话内自适应，但不承诺跨重启。
- 候选窗 M1 在 TIP 进程内；不迁独立进程（M2）。
- 不改 M0 已通过的注册/卸载流程与 GUID。
- IPC 协议字段只增不改语义；如需改 -> 升 PROTOCOL_VERSION。

## 5. 可执行验收判据

| # | 步骤 | 期望 |
|---|---|---|
| M1-1 | myabc-engine.exe --selftest --model-dir out/.../data | stdout 含 sentence[0] = 你好；退出 0 |
| M1-2 | ctest 引擎 golden 测试（tests/unit/engine_conversion） | nihao->你好 在 top1；woshizhongguoren->我是中国人 在 top1；beijing->北京 命中；全通过 |
| M1-3 | 记事本输入 nihao 后按空格 | 文档出现"你好"，预编辑与候选窗消失 |
| M1-4 | 记事本输入 woshizhongguoren | 候选窗首项"我是中国人" |
| M1-5 | 输入 shiyan 后按 - / = | 候选翻页，页码变化 |
| M1-6 | 组字中按数字 3 | 上屏第 3 候选（或确定该词并继续组剩余） |
| M1-7 | 组字中按退格 | 删除最后一个拼音字母，候选刷新 |
| M1-8 | 组字中按 ESC | 预编辑清空，无上屏 |
| M1-9 | 按 Shift（单击）后输入 abc | 直接上屏 abc，无候选窗 |
| M1-10 | 中文模式按 , 和 . | 上屏 中文逗号 和 句号 |
| M1-11 | 候选窗位置 | 紧邻文本插入点；移动窗口后重新组字，候选窗跟随 |
| M1-12 | 关闭引擎进程后在记事本按键 | TIP 在 request_timeout_ms 内降级（放行/结束组字），记事本不卡死；引擎被重新拉起 |
| M1-13 | tests/review/run_all | 全绿（含 tip 隔离检查：tsf-service/candidate-ui 无 pinyin/glib include） |

回归判据（历史真实坑）：
- libpinyin locale 敏感解析（上游 PR #187）：ctest 用例在 setlocale 为非 C（如 Chinese_China.936）
  下加载模型并转换，仍需 M1-2 全通过。
- O_BINARY / MemoryChunk（上游 PR #188）：引擎在 Windows 保存并重新加载用户 db，round-trip 数据一致。

## 6. 风险与回滚

- 风险：libpinyin 候选 API 与上述假设不符 -> 以 third_party/libpinyin/src/pinyin.h 实际签名为准，
  必要时读 ibus-libpinyin 的 src/PYPPhoneticEditor.cc 作用法参考（GPLv3 兼容）。
- 风险：候选窗在多显示器/高 DPI 错位 -> Per-Monitor-V2 + 用 ITfContextView::GetTextExt 取 caret 矩形。
- 风险：EditSession 异步时序导致预编辑闪烁 -> 统一用 TF_ES_SYNC 尝试，失败回退 ASYNC 并合并更新。
- 回滚：git checkout m0-skeleton；引擎与 DLL 可分别回退（IPC 协议兼容则可混用旧引擎）。
