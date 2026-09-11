# myabc 系统架构总览（system-overview）

- 状态：生效
- 更新日期：2026-09-09
- 来源：deep-planner 规划（基于 docs/requirements/smart-abc-ime-requirements.md、docs/reference/windows-ime-framework-reference.md）
- 关联决策：
  - docs/decisions/ime-framework/20260909-adopt-tsf-over-imm32.md
  - docs/decisions/pinyin-engine/20260909-adopt-libpinyin-mingw-engine-process.md
  - docs/decisions/project-setup/20260909-cpp-cmake-split-toolchain.md

---

## 1. 一句话定位

薄 TSF TIP DLL（MSVC 构建，注入每个宿主进程，仅做按键状态机 + IPC 转发 + 候选窗协调）
＋ 独立引擎进程 myabc-engine.exe（MinGW-w64 UCRT64 构建，libpinyin + 词库 + 用户学习 + 候选生成）
＋ 候选窗 UI（M1 在 DLL 内，M2 起迁到独立 UI 宿主进程）
＋ 部署注册工具 myabc-deployer.exe，
四者通过命名管道 IPC（长度前缀 + JSON 消息）解耦。

## 2. 进程 / 模块拆分

| 组件 | 产物 | 工具链 | 架构 | 进驻位置 | 职责 |
|---|---|---|---|---|---|
| TSF TIP | myabc-tip.dll | MSVC v143 / CMake / vcpkg | x86 + x64 | 每个有文本输入的宿主进程（含 AppContainer） | 实现 TSF COM 接口；按键预判（本地状态机）；把需要转换的按键经 IPC 发给引擎；驱动候选窗；把 commit 文本写回文档（EditSession） |
| 引擎进程 | myabc-engine.exe + 依赖 DLL | MinGW-w64 UCRT64 / CMake | x64（单一） | 每用户会话一个，首次连接时由 TIP 拉起，命名互斥量保证单实例 | libpinyin 全拼/简拼/混拼转换；音节切分；笔形辅助码筛选；数字/金额；GBK 扩展；用户词库自学习；命名管道服务端 |
| 候选窗 UI | M1：静态库链入 DLL；**M2：myabc-ui.exe（已落地）** | MSVC v143 | 独立进程 | 每用户会话一个，首次需要显示时由引擎拉起 | GDI 绘制候选列表（M1 起的决策，非 Direct2D，见 docs/decisions/_debt-log.md）；直接连引擎的 `myabc-ui-{sid}` 管道接收 uiShow/uiHide 单向推送，不经 TIP；跟随光标定位（TIP 经 `setCaretRect` 转交引擎，引擎随 uiShow 一起推） |
| 部署工具 | myabc-deployer.exe | MSVC v143 | x86 + x64 | 独立运行（安装/卸载时） | ITfInputProcessorProfiles 注册、Category 注册、COM 自注册/反注册、文件布局、启用输入法配置 |
| IPC 协议 | 头文件 + 微型序列化库 | 两套工具链都编译 | 无 | 消息结构定义与编解码，无业务逻辑 |

为什么引擎必须独立进程：TSF 的 TIP DLL 会被加载进每一个文本输入进程。若把 libpinyin +
glib + DB + 数百 MB 词库放 DLL 内，等于给每个 App 进程注入重依赖（还有 MinGW 运行时 DLL），
不可接受。Weasel、PIME、fcitx5-windows 均采用薄 DLL + 引擎进程 + IPC。

为什么引擎用 MinGW、DLL 用 MSVC：libpinyin 上游只有 autotools/CMake，代码依赖 POSIX
（unistd.h、mmap、getline、fsync）和 glib-2.0；2024-2025 上游合并的一批 Windows 移植补丁全部针对
MinGW-w64 UCRT64（见 libpinyin issue #182/#183，PR #164/#184/#186/#187/#188/#189），
没有 MSVC 支持证据。把引擎整体交给 MinGW，DLL 侧完全不碰 glib/libpinyin，
两者只在字节级管道协议处相接——既绕开构建难题，又用构建体系强制了薄 DLL 不变量。
详见 pinyin-engine ADR 与 docs/plan/00-toolchain-and-build-plan.md。

## 3. 领域划分（src/domains/<domain>/{ui,logic,backend}）

依赖方向恒为 ui -> logic -> backend；跨领域只能调对方 logic 暴露的接口。

### 3.1 domain: tsf-service（MSVC，链入 myabc-tip.dll）
- backend/：TSF/COM 管道——ITfTextInputProcessorEx、ITfThreadMgrEventSink、
  ITfKeyEventSink、ITfCompositionSink、ITfEditSession 实现；DLL 入口
  （DllGetClassObject / DllCanUnloadNow / DllMain）；类工厂；IPC 客户端（命名管道，overlapped IO）。
- logic/：按键状态机（是否正在组字、该键是否属于当前方案的有效输入、何时提交/翻页/回退）；
  中英文模式；标点映射决策。不做拼音转换，转换请求交给 IPC 客户端。
- ui/：语言栏按钮（ITfLangBarItem）等 TIP 自身的极简 UI。不 import backend/。

### 3.2 domain: candidate-ui（MSVC）
- backend/：分层窗创建、DPI 感知、依据 caret 矩形定位、窗口消息泵。
- logic/：候选分页 / 高亮 / 选中索引状态机（纯状态，无绘制）。
- ui/：Direct2D/DirectWrite 绘制候选列表、翻页指示、拼音串显示。
- M1：backend/ + ui/ 编成静态库由 tsf-service 链接并在 TIP 线程建窗；
  M2：改由 myabc-ui.exe 承载，引擎经 IPC 驱动，TIP 只转发 caret 位置。

### 3.3 domain: input-engine（MinGW，链入 myabc-engine.exe）
- backend/：libpinyin 封装（pinyin_init / pinyin_alloc_instance / pinyin_parse /
  pinyin_guess_sentence 等）；DbBackend 适配器实现（BerkeleyDB / KyotoCabinet）；glib 生命周期。
- logic/：转换编排与候选排序；InputSchemeParser 适配器（全拼/简拼/混拼/预留双拼）；
  音节切分歧义策略；CandidateSource 适配器注册表（拼音 / i-数字金额 / 标点 / 英文透传）；
  CharsetFilter 适配器（GB2312 / GBK / Unicode）；笔形辅助码二级筛选。
- ui/：无（headless）。
- 命名管道服务端在 backend/（传输），消息分发到 logic/。

### 3.4 domain: user-dict（MinGW，链入 myabc-engine.exe）
- backend/：libpinyin 用户 bigram/unigram 持久化 + 自建新词存储；原子写（g_rename）。
- logic/：学习策略（新短语何时晋升为词条、词频提升与衰减、去重、上限）。
- ui/：无。

### 3.5 domain: deployment（MSVC，myabc-deployer.exe）
- backend/：ITfInputProcessorProfiles::Register / AddLanguageProfile、
  ITfCategoryMgr::RegisterCategory、注册表 InprocServer32 写入、COM (un)register、文件布局落盘。
- logic/：注册 / 反注册 / 升级 编排；校验注册结果。
- ui/：命令行（--register / --unregister / --status），可选极简对话框。

### 3.6 shared: src/shared/ipc-protocol/
- 纯数据：消息结构体、方法枚举、PROTOCOL_VERSION 常量、长度前缀帧编解码、JSON 与 struct 互转。
- 无业务逻辑、无副作用；两套工具链都能编译（仅依赖标准库 + 一个 header-only JSON）。

### 3.7 依赖红线（由 tests/review 固化）
- tsf-service、candidate-ui 的任何源文件禁止 include pinyin.h、glib.h、db.h、
  kchashdb.h 或 input-engine/backend/**。
- ui/ 禁止 include 同域 backend/。
- 跨域禁止直接 include 对方 backend/。

## 4. IPC 方案选型

| 方案 | 取舍 |
|---|---|
| 命名管道 + 长度前缀 + JSON 消息（选定） | 跨架构（x86 TIP 与 x64 引擎）自然支持；可直接读日志；PIME 同款；实现成本低。JSON 编解码开销对单次按键可忽略（本地往返亚毫秒级）。编码方式藏在 IpcCodec 接口后，后续可换二进制。 |
| COM/LRPC 本地服务器 | 与 TSF 同为 COM、天然处理会话与安全，但样板代理/存根多、调试难、进程生命周期与激活语义复杂。备选。 |
| 共享内存 + 事件（Weasel 式） | 吞吐最好，但需自管环形缓冲与同步，跨架构对齐易错。留作 IpcTransport 适配器的后续实现。 |

通道命名：\.\pipe\myabc-engine-<用户SID>（从 src/config 取模板，不硬编码）。
生命周期：TIP 连接失败 -> 尝试启动 myabc-engine.exe -> 重试连接（带超时与退避）；
引擎用命名互斥量 Local\myabc-engine-<SID> 保证单实例；引擎空闲 N 分钟无连接可自退出。
UI 线程约束：OnTestKeyDown 必须本地同步答复（状态机即可判断要不要吃这个键）；
只有 OnKeyDown 需要引擎往返，使用 overlapped IO + 有界等待（默认 50ms，可配），
超时则降级（回显原键 / 结束组字），绝不阻塞宿主 UI 线程。

### 4.1 消息协议草案（PROTOCOL_VERSION = 2，M2 起）

请求： { "v":2, "id":<u32>, "method":"<name>", "params":{...} }
响应： { "v":2, "id":<u32>, "ok":true, "result":{...} }
错误响应： { "v":2, "id":<u32>, "ok":false, "error":{"code":<int>,"msg":"..."} }

两条独立的命名管道连接复用同一套编解码：
- **TIP <-> 引擎**（`\\.\pipe\myabc-engine-{sid}`）：下表除 uiShow/uiHide 外的方法。
- **引擎 -> myabc-ui**（`\\.\pipe\myabc-ui-{sid}`，M2 新增，见 §2/plan 03 §3.1）：
  引擎单向推 uiShow/uiHide，myabc-ui 只连接、不发业务请求（候选窗点选仍走键盘 -> TIP -> 引擎，
  M2 未做鼠标点选）。

**v2 变更（对比 v1）**：`processKey`/`selectCandidate`/`pageCandidates`/`commitComposition`/
`cancelComposition` 的 result 不再带 `candidates`/`page`——候选明细改由引擎经 uiShow 单向推给
`myabc-ui`，不再经 TIP 转发（TIP 变薄，见 plan 03 背景）。新增 `setCaretRect`（TIP 用
`ITfContextView::GetTextExt` 拿到光标屏幕矩形后调用，引擎收到后把"最近一次算好的候选 +
这个矩形"一起推给 myabc-ui；`composing=false` 时引擎直接推 uiHide，不必等 setCaretRect）。

方法集：
| method | 连接 | params | result |
|---|---|---|---|
| hello | TIP<->引擎 | clientVersion,pid,arch | engineVersion,protocol,pinyinReady |
| initSession | TIP<->引擎 | sessionId,config? | (空) |
| processKey | TIP<->引擎 | sessionId,vk,ch,mods | handled,preedit,composing,commit? |
| selectCandidate | TIP<->引擎 | sessionId,index | 同 processKey |
| pageCandidates | TIP<->引擎 | sessionId,delta | 同 processKey（composing 恒 true） |
| commitComposition | TIP<->引擎 | sessionId | handled,commit |
| cancelComposition | TIP<->引擎 | sessionId | handled |
| focusIn / focusOut | TIP<->引擎 | sessionId | (空) |
| setCaretRect | TIP<->引擎 | sessionId,x,y,w,h | (空) |
| setConfig | TIP<->引擎 | patch{...} | applied |
| uiShow | 引擎->myabc-ui | sessionId,caretRect{x,y,w,h},preedit,candidates[{text}],page{index,size,total} | — |
| uiHide | 引擎->myabc-ui | sessionId | — |
| shutdown | (空) | (空) |

sessionId 由 TIP 每个文档上下文分配，隔离多窗口并发组字。

## 5. src/config/ 配置化清单（反硬编码落点）

编译期默认在 src/config/config_defaults.hpp（结构体，非裸字面量），
运行期从 %APPDATA%\myabc\config.toml 覆盖；代码只读配置对象。

| 键 | 默认 | 说明 |
|---|---|---|
| engine.model_dir | <安装目录>\data | libpinyin 系统词库/模型目录 |
| engine.user_data_dir | %APPDATA%\myabc\userdata | 用户词库、学习数据 |
| engine.exe_path | <安装目录>\myabc-engine.exe | TIP 拉起引擎用 |
| engine.idle_exit_minutes | 10 | 引擎空闲自退出 |
| ipc.pipe_name_template | \.\pipe\myabc-engine-{sid} | 管道名模板 |
| ipc.connect_timeout_ms | 2000 | 首次连接（含拉起引擎）总超时 |
| ipc.request_timeout_ms | 50 | 单次按键请求超时，超时降级 |
| ipc.connect_backoff_ms | [50,100,200,400] | 重试退避序列 |
| candidates.page_size | 9 | 每页候选数 |
| candidates.page_prev_keys | ["-", ","] | 上一页键 |
| candidates.page_next_keys | ["=", "."] | 下一页键 |
| candidates.select_keys | "123456789" | 选字键 |
| input.scheme | "quanpin" | quanpin/jianpin/hunpin/(shuangpin 预留) |
| input.fuzzy | [] | 模糊音开关集合（映射 libpinyin pinyin_option_t） |
| input.bihuo_enabled | true | 笔形辅助码 |
| input.number_lead_key | "i" | i 引导数字/金额 |
| output.charset | "gbk" | gb2312/gbk/unicode，决定 CharsetFilter |
| ui.font | "Microsoft YaHei UI" | 候选窗字体 |
| ui.font_size_pt | 12 | |
| ui.colors | (主题结构) | 背景/前景/高亮 |
| ui.follow_caret | true | 跟随光标 |
| learning.enabled | true | 自学习总开关 |
| learning.min_uses_to_promote | 2 | 新短语晋升阈值 |
| db.backend | "berkeleydb" | berkeleydb/kyotocabinet（主要编译期，运行期只读展示） |
| langid | 0x0804 | zh-CN；注册用 |

## 6. 适配器点位（3+ 分支收敛为接口）

| 接口 | 实现 | 新增变体方式 |
|---|---|---|
| InputSchemeParser | QuanpinParser / JianpinParser / HunpinParser / (预留 ShuangpinParser) | 新增类 + 注册表登记，不改分发 |
| DbBackend | BerkeleyDbBackend / KyotoCabinetBackend | 同上（编译期择一，接口隔离） |
| CandidateSource | PinyinCandidateSource / NumberCurrencyCandidateSource / PunctuationSource / EnglishPassthroughSource | 新增类 + registry.resolve(context) |
| CharsetFilter | Gb2312Filter / GbkFilter / UnicodeFilter | 同上 |
| IpcTransport / IpcCodec | NamedPipeTransport / JsonCodec（预留 SharedMemoryTransport / BinaryCodec） | 同上 |

## 7. 关键设计不变量（改任何代码都要保持）

1. myabc-tip.dll 不链接、不 include libpinyin / glib / DB / 引擎 backend。
2. TIP 进程内不加载词库/语言模型文件。
3. 宿主 UI 线程上的按键处理不做无界同步等待。
4. COM 引用计数正确：AddRef/Release 配平，DllCanUnloadNow 依对象计数，QueryInterface 正确返回 E_NOINTERFACE。
5. 文档改动只在 ITfEditSession 内进行。
6. 跨 MinGW/MSVC 边界只传字节级管道协议，绝不传 C++ 对象 / STL 容器 / 异常。
7. IPC 协议结构变更必须递增 PROTOCOL_VERSION 并同步编解码两侧与本文件 4.1。
8. 所有换环境就要改的值来自 src/config，源码无裸字符串/裸数字。
9. 每个决策承载点有 DECISION: 注释指向 docs/decisions/...。
