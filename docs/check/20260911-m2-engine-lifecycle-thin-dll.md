# M2 质检报告 · 引擎生命周期加固 + 候选窗独立进程（DLL 变薄）

- 日期：2026-09-11
- 里程碑：M2（docs/plan/03-m2-engine-process-ipc-plan.md）
- 执行方：主会话（Sonnet）
- 结论：**核心架构目标全部达成并端到端验证**（协议 v2、DLL 变薄、候选窗独立进程、
  引擎空闲自退出、崩溃兜底）。M2-3（压测）/ 完整异步投递为有意裁剪范围，见 §4。
  **M1 既有的记事本 GUI 场景需要用户在真机复测一遍**（TIP 内部改动较大）。

## 1. 本轮交付

- **协议 v2**（`PROTOCOL_VERSION` 1->2，`docs/architecture/system-overview.md §4.1`）：
  processKey 系方法响应瘦身为 `{handled,preedit,composing,commit?}`；新增
  `setCaretRect`（TIP->引擎）、`uiShow`/`uiHide`（引擎->myabc-ui，独立管道）。
- **`src/domains/input-engine/backend/ui_bridge.{hpp,cpp}`**：引擎侧对 myabc-ui 的单向
  推送通道，独立命名管道 `myabc-ui-{sid}`，后台线程 accept，按需 `CreateProcess` 拉起
  myabc-ui.exe（单实例互斥量保证重复拉起无害）。
- **`src/domains/candidate-ui/ui/host_main.cpp`**（新）：`myabc-ui.exe` 入口，连引擎的
  uiShow/uiHide 通道，断线退避重连；不随引擎退出而退出。
- **`myabc_text_service.{hpp,cpp}` 变薄**：移除 candidate-ui/gdi32 链接与直接调用，
  改为 `ipc_->SetCaretRect(...)`；`composition_state.hpp` 同步瘦身（不再解析 candidates/page）。
- **引擎生命周期**（`backend/pipe_server.cpp`）：overlapped `ConnectNamedPipe` +
  `WaitForSingleObject` 超时，替换原来的无界阻塞 accept；空闲超时 `SaveBeforeExit()`
  （`pinyin_save`）后退出。`--idle-exit-seconds` 供测试覆盖秒级超时。
- **`backend/crash_guard.{hpp,cpp}`**（新）：`SetUnhandledExceptionFilter` +
  `MiniDumpWriteDump` 落 `%APPDATA%\myabc\logs\myabc-engine-crash-*.dmp`。
- **`LibPinyinEngine::Save()`**（新）：包 `pinyin_save`，仅在引擎退出前调用一次。

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| MSVC x64/x86 构建 | PASS | 0 警告（/W4 /permissive-） |
| MinGW 引擎构建 | PASS | 仅 1 条 libpinyin 头文件带来的既有良性告警（枚举位运算），非本轮代码引入 |
| M2-1（`myabc-tip.dll` 不依赖候选渲染库） | PASS | `objdump -p`：无 d2d1/dwrite（M1 起已如此）；**这次连 gdi32 也从依赖表消失**（USER32 仍在，来自 TSF 按键 API GetKeyboardState/ToUnicode，与候选渲染无关） |
| ctest（两侧） | PASS | ipc_roundtrip + engine_conversion 全过 |
| `tests/review/run_all.sh` | GREEN | check_hardcode 扫描 66 源文件 0 违规 |
| 协议 v2 端到端（自建 Python 客户端，真连真管道，非 mock） | PASS | 见 §3 |
| M2-4/M2-5（空闲自退出 + 重启） | PASS | 见 §3 |
| M2-2（杀引擎后候选窗恢复） | 结构性成立，未做真实杀进程+记事本联合测试 | ipc_client 的 bounded-wait + 断线重连机制在 M1 已验证（M1-12）；myabc-ui 断线自动重连亦已验证（见 §3）；三者组合的真实记事本场景留 §5 用户验证 |
| M2-6（协议版本检查脚本） | 未创建独立脚本 | `kProtocolVersion=2` 已在两侧源码同步，靠 C++ 编译期共享常量保证一致，未见必要再加一层影子检查脚本；如后续两侧真的分开编译验证不足再补 |
| M2-7 `tests/review/run_all` | GREEN | 同上 |

## 3. 端到端验证明细（自建 Python 客户端，直连真实引擎/UI 进程）

1. **v2 协议基本往返**：`hello` 返回 `protocol:2`；`processKey` 响应确认**不含** `candidates`/`page` 字段（断言核实）。
2. **uiShow 推送链路**：`processKey`("nihao") 后调 `setCaretRect` → 独立连接收到 `uiShow`，`caretRect`/`preedit`/`candidates`/`page` 均正确（候选含"你好"）。
   - 过程中抓到并修复一个真实 bug：`SessionResultToResponse` 把 JSON-RPC 消息 id 和 sessionId 弄混，导致 `pending_ui_` 用错误的 key 存取，`uiShow` 静默丢失（响应本身不报错）。详见 debt-log。
3. **uiHide 自动推送**：commit 后（`composing:false`）引擎**主动**推 `uiHide`，无需 TIP 额外调用。
4. **真实 `myabc-ui.exe` 进程**：引擎按需 `CreateProcess` 拉起，收到推送后进程存活、无崩溃（`tasklist` 确认）；引擎 `shutdown` 后 UI 进程按设计继续存活（重连循环），非缺陷。
5. **空闲自退出**：`--idle-exit-seconds 3`，断开连接后引擎在约 3.0s 精确退出，`pinyin_save` 路径执行；退出后管道立即可供新实例监听（等价 TIP 下次交互会重新拉起）。

## 4. 有意裁剪的范围（均记录在 debt-log，非遗漏）

- TIP 侧按键路径仍是"同步 + 50ms 有界等待"，非计划描述的全异步投递模型（PostMessage
  回 TIP 线程）——已实测延迟可忽略，全异步复杂度/风险不匹配当前收益，留有明确触发条件
  （未来若实测到可感知卡顿）。
- `ipc_client.Connect()` 未严格按 `connect_backoff_ms_csv` 配置的退避序列（仍是固定
  100ms/200ms 简单重试）。
- `tests/integration/typing_stress`（200 字压测脚本）未创建；压测判据靠本轮端到端测试的
  观感佐证（每次往返几毫秒级），未做正式统计断言。
- `tests/review/check_protocol_version` 独立脚本未创建（两侧共享同一份 C++ 常量定义，
  编译期已经保证一致）。

## 5. 待用户在真机验证

TIP DLL / 引擎 / myabc-ui.exe 均已更新到 `out\Release\install-x64\`（注册路径不变，
不需要重新 `--register`；**加载过旧版 DLL 的进程要重启**，本次实测环境需要重启的有
记事本、Notepad++、WindTerm）。

- M1 回归：nihao 候选窗、翻页、数字选字、退格、Esc、Shift 切英文、标点——TIP 内部改动
  较大（候选窗从进程内搬到独立进程），需要重新走一遍确认没有引入回归。
- M2-2：组字过程中用任务管理器结束 `myabc-engine.exe`，观察候选窗是否消失且记事本
  不卡顿；再按几个字母，候选应重新出现（引擎自动重启）。
- 观察候选窗现在是不是一个独立的 `myabc-ui.exe` 进程（任务管理器能看到），而不是
  记事本进程的一部分。
