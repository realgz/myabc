# M1 R1 质检报告 · 引擎侧接入 libpinyin（全拼整句 + processKey 全链路）

- 日期：2026-09-11
- 里程碑：M1（plan 02-m1-libpinyin-quanpin-plan.md §3.1-3.3；§3.4/§3.5 TSF 侧留 R2）
- 执行方：主会话（Sonnet）
- 结论：**引擎侧全部通过**。libpinyin 真实接入验证成功（Plan A 对 M1 同样成立）；
  `processKey`/`selectCandidate`/`pageCandidates`/`commitComposition`/`cancelComposition`
  经真实管道客户端端到端跑通，行为与 plan §5 验收判据逐条吻合。

## 1. 本轮交付

- **libpinyin 词库二进制生成**：新增 `third_party/libpinyin/build-data`（静态构建，仅用于跑
  `data` 目标）+ `cmake/FindLibPinyin.cmake`。产出 ~90MB 二进制词库（gb/gbk/opengram/merged +
  12 个领域附加词库 + bigram.db）。原因见 debt-log（DLL 导入库 + 静态库同时链接的 Windows 特有
  multiple-definition 问题）。
- `src/domains/input-engine/backend/libpinyin_wrapper.{hpp,cpp}`：`LibPinyinEngine` 封装
  `pinyin_init/parse_more_full_pinyins/guess_sentence/guess_candidates/choose_candidate/
  get_sentence/train/reset`，对外只暴露 `std::string`/`CandidateItem`，不泄漏 glib 类型。
- `logic/candidate/`：`CandidateSource` 适配器接口 + 3 个实现
  （`PinyinCandidateSource`/`EnglishPassthroughSource`/`PunctuationSource`）+ `SourceRegistry`
  （顺序 english -> punctuation -> pinyin）。
- `logic/charset/charset_filter.{hpp,cpp}`：`UnicodeFilter`（透传）+ `GbkFilter`（codepage 936
  可表示性判定，`Gb2312Filter` 暂复用同一实现，见 debt-log）。
- `logic/session/session.{hpp,cpp}` + `session_manager.{hpp,cpp}`：组字状态机
  （raw 拼音累积、翻页、选字、部分确认继续组剩余、退格/ESC/标点自动上屏）。
- `logic/dispatcher.cpp` 重写：initSession/processKey/selectCandidate/pageCandidates/
  commitComposition/cancelComposition/focusIn/focusOut 全部实现，`hello` 增加
  `pinyinReady` 字段。
- `logic/engine_main.cpp`：真 `--selftest`（`pinyin_init` + 解析 "nihao" + 断言含"你好"）；
  正常模式加载 libpinyin 后跑 `Dispatcher`。
- `src/config/config_defaults.hpp`：补 `CandidatesConfig`/`InputConfig`/`OutputConfig`。
- `tests/unit/engine_conversion_test.cpp`：golden 测试（M1-2），MinGW + libpinyin 就绪时才编译。
- `assets/config/punctuation.toml`：标点映射数据草稿（TOML loader 未接，见 debt-log）。

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| M1-1 `myabc-engine.exe --selftest` | PASS | `selftest: sentence[0] = 你好` / `PASS. exit 0` |
| M1-2 ctest golden（nihao/woshizhongguoren/beijing） | PASS | `engine_conversion_test`：三例全过（含非 C locale `Chinese_China.936` 前置，覆盖 PR #187 回归判据） |
| `ipc_roundtrip` | PASS | 不受影响 |
| 端到端管道冒烟（外部 Python 客户端，模拟 TIP） | PASS | 见 §3 逐场景 |
| `tests/review/run_all.sh` | GREEN | check_hardcode 扫描 51 源文件 0 违规 |
| MSVC 侧（tsf-service/deployment）无回归 | PASS | `build.ps1 -Arch x64` 重新构建成功，未改动这些域 |

## 3. 端到端冒烟场景（外部管道客户端，等价 TIP 的 processKey 序列）

| 场景 | 对应判据 | 结果 |
|---|---|---|
| `hello` | — | `pinyinReady: true` |
| 逐字母 n/i/h/a/o，每步候选刷新 | M1-3 铺垫 | 候选合理演进（n→"年/你/内容..."，nihao→"你好"top1） |
| 空格上屏 | M1-3 | `commit: "你好"` |
| woshizhongguoren + 数字 `1` | M1-4/M1-6（简化路径） | `commit: "我是中国人"` |
| shiyan + `pageCandidates delta=1` | M1-5 | `page.index: 1`，候选换页 |
| 逗号 `,`（非组字态） | M1-10 | `commit: "，"`（全角） |
| nih + 退格 | M1-7 | `rawInput: "ni"`，候选刷新 |
| n + ESC | M1-8 | `handled:true`，无 `commit` |

## 4. 计划偏差 / 决策（详见 docs/decisions/_debt-log.md 2026-09-11 各条）

1. libpinyin 词库二进制生成走独立静态 build（`build-data`），避免 Windows 下 DLL 导入库 +
   静态库重复符号定义问题。
2. 候选模型直接用 libpinyin 内建"整句优先"列表，未额外实现"整句候选拼装"逻辑。
3. M1 单一全局 `LibPinyinEngine`（session 间共享），多窗口并发组字会互相干扰——M2 处理。
4. 组字中追加字母/退格 = 整串重新猜测，不保留既有分段确认约束（M1-6 单场景不受影响）。
5. Gb2312Filter 近似复用 GbkFilter（Windows 无独立 GB2312 代码页）。
6. config.toml 真解析仍未做（M0 起的已知 TODO，M1 验收不依赖它）。
7. scheme_registry（quanpin/jianpin/hunpin 适配器）未落地——M1 只有 1 种方案，未达 3+ 变体
   门槛，M3 再补齐。
8. 观察到 libpinyin 两条良性 stderr 告警（user.conf 缺失 / BDB1565 sync），不影响功能。

## 5. 未覆盖（M1 R2）

- plan 02 §3.4 candidate-ui（候选窗；**决策**：用 GDI 而非 Direct2D/DirectWrite 实现，
  详见 R2 报告）。
- plan 02 §3.5 tsf-service 改造：composition_state 状态机接管预编辑显示、
  key_event_sink 组字态按键路由、`composition.cpp`（ITfComposition）、
  `ipc_client` 全量请求 + 有界超时降级（M1-12）、`mode_manager`（Shift 切英文）。
- M1-3 ~ M1-13 里涉及记事本 GUI 交互的部分（R2 完成实现后由用户实机验证）。
