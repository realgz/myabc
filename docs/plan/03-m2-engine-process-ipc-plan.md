# 03 · M2 计划（引擎生命周期加固 + 异步按键 + 候选窗独立进程，DLL 变薄）

- 状态：生效
- 更新日期：2026-09-09
- 前置：M1（tag m1-quanpin）
- 回滚锚点：tag m2-thin-dll

## 1. 背景与目标

M1 的候选窗在 TIP 进程内、按键路径可能同步阻塞、引擎生命周期粗糙。M2 把这些收敛到"薄 DLL"
目标：候选渲染迁到 myabc-ui.exe，按键全异步，引擎自动拉起/单实例/崩溃恢复/空闲退出。

完成后可观察：
- Process Explorer 看 notepad.exe 加载的 myabc-tip.dll 不再依赖 d2d1/dwrite（候选绘制已移出）。
- kill 引擎进程，继续打字，候选自动恢复，无宿主卡顿 > request_timeout_ms。
- 无输入活动 idle_exit_minutes 后引擎自退出；再输入自动重启。

## 2. 现状分析

- M1：candidate-ui 静态库链入 myabc-tip.dll，窗口在 TIP 线程创建的 UI 线程。
- M1：ipc_client 有界等待但仍在 OnKeyDown 内串行往返。
- M1：引擎拉起逻辑在 ipc_client.Connect 里较简单，无健康检查/重连退避完善。

## 3. 文件级改造点

### 3.1 新建 src/domains/candidate-ui 的宿主：myabc-ui.exe
- ui/host_main.cpp：CoInitialize；连引擎命名管道（复用协议，新增 ui 通道方法 uiShow/uiUpdate/uiHide/uiSelect）；
  或 UI 直接由引擎驱动（引擎 -> UI 单向 push），TIP 只把 caret rect 发引擎。
- 迁移 M1 的 backend/candidate_window、ui/renderer_d2d 到此 exe；candidate-ui 不再链入 tip dll。
- CMakeLists：add_executable(myabc-ui)。

### 3.2 IPC 协议扩展（PROTOCOL_VERSION -> 2）
- 新增方法：setCaretRect{sessionId,x,y,w,h}；引擎聚合候选后直接通知 myabc-ui 定位与绘制。
- processKey 的 result 不再回候选明细给 TIP（TIP 不画），只回 {handled, commit?, composing:bool}；
  候选明细走引擎 -> UI 通道。
- 更新 src/shared/ipc-protocol 编解码两侧 + docs/architecture/system-overview.md §4.1。

### 3.3 tsf-service 变薄
- 删除对 candidate-ui、d2d1、dwrite 的链接（CMakeLists）。
- backend/ipc_client：改全异步——OnKeyDown 投递请求即返回 S_OK（先吃键），响应到达后经
  PostMessage 到 TIP 线程再跑 EditSession 应用 commit/preedit。加请求队列 + 序号匹配 + 超时清理。
- backend/engine_supervisor.cpp/.h（新增）：连接健康检查（心跳 hello）、断线指数退避重连、
  CreateProcess 拉起、失败计数熔断（连续 N 次失败则通知用户并暂停）。

### 3.4 引擎生命周期
- backend/single_instance：已在 M0；补充命名事件 ready 信号，TIP 等 ready 再发首请求。
- backend/idle_timer.cpp/.h：无活动会话且无连接超过 config.engine.idle_exit_minutes -> pinyin_save + 退出。
- backend/crash_guard：顶层 __try/__except 或 SetUnhandledExceptionFilter -> 落 minidump 到 logs/，pinyin_save 尽力。
- logic/dispatcher：会话状态可从 TIP 侧重放恢复（TIP 缓存当前 raw 拼音，重连后 replayComposition）。

## 4. 不改动清单
- 转换质量、方案范围不变（仍只全拼）。
- 不引入简拼/笔形/数字（M3/M4）。
- GUID、注册流程不变。
- 不改候选窗视觉设计（仅换承载进程）。

## 5. 可执行验收判据

| # | 步骤 | 期望 |
|---|---|---|
| M2-1 | dumpbin /dependents out\...\myabc-tip.dll | 不含 d2d1.dll / dwrite.dll |
| M2-2 | 组字过程中 taskkill /f /im myabc-engine.exe | <=1s 内候选窗恢复工作；记事本无 ANR |
| M2-3 | 连打 200 字压测脚本 tests/integration/typing_stress | 无丢键、无死锁；p99 按键处理 < 30ms（TIP 侧测量，不含引擎异步） |
| M2-4 | 空闲 idle_exit_minutes（测试用 0.1） | 引擎进程自动退出；日志有 pinyin_save |
| M2-5 | 退出后按键 | 引擎自动重启，首候选正确 |
| M2-6 | 协议版本检查 tests/review/check_protocol_version | 协议文件改了则 PROTOCOL_VERSION==2，两侧一致 |
| M2-7 | tests/review/run_all | 全绿 |

回归判据：M1-3..M1-12 全部仍通过（回归套件重跑）。

## 6. 风险与回滚
- 异步 EditSession 时序更复杂：用 TIP 线程串行化响应处理，composition 版本号丢弃过期响应。
- UI 独立进程增加一个崩溃面：myabc-ui 崩溃时 TIP/引擎不受影响，引擎监测 UI 断开则重拉。
- 回滚：git checkout m1-quanpin（协议 v1）。
