# 候选窗支持鼠标选择

- 日期：2026-09-12
- 领域：tsf-service（配合 candidate-ui）
- 状态：生效中
- 触发来源：用户对话（"支持鼠标选择"）

## 背景

候选窗（`myabc-ui.exe`）自 M2 起就是独立进程，跟 TIP（`myabc-tip.dll`，加载在
宿主应用如记事本/浏览器的进程里）完全分开：引擎经 `myabc-ui-{sid}` 管道把候选
明细单向推给 UI 进程渲染，UI 进程从不往回写。这条通道天然只解决"显示"，没有
"用户在候选窗上做了什么动作，需要让某个具体的 TIP 实例去改文档"这条反向路径。

真正把候选文字写进文档（`ITfComposition`/`ITfEditSession`）只能由持有那个
`ITfContext` 的 TIP 实例、在它所在的宿主进程里完成——这是 TSF 架构本身的约束，
不是本项目的设计选择。所以鼠标点击候选要生效，点击事件必须能从 UI 进程"传回"
正确的那个 TIP 实例。

## 决策内容

新增一条跨进程通知：不新开命名管道，用 Win32 窗口消息（`PostMessageW`）。

1. **TIP 只在正在组字（`composing=true`）期间维护一个隐藏的 message-only 窗口**
   （类名固定 `MyabcTipClickBridge`，`ClickBridge` 类，
   `src/domains/tsf-service/backend/click_bridge.{hpp,cpp}`），组字一结束
   （commit/cancel/被外部终止/TIP 停用）立即销毁。
2. **利用现有的单会话简化**（`kSessionId` 全局硬编码为 1，见
   `myabc_text_service.hpp` 类头 DECISION）：Windows 键盘事件天然只送到当前
   焦点控件，全系统同一时刻只有一个文档能处于"正在组字"，所以任意时刻全系统
   最多只存在一个这样的窗口——候选窗查找时不需要按 sessionId 区分，也不会有
   "点到哪个 TIP 实例"的二义性。
3. 候选窗（`CandidateWindow`，`src/domains/candidate-ui/backend/candidate_window.cpp`）
   收到鼠标事件后，自己用 `FindWindowExW(HWND_MESSAGE, nullptr,
   kClickBridgeWindowClass, nullptr)` 找到这个窗口，`PostMessageW` 发送
   `kWmSelectCandidateByClick`（`wParam` = 候选下标）。
4. TIP 的 `ClickBridge::WndProc` 收到消息后，直接走跟数字键选字**完全相同**的
   代码路径：`IpcClient::SelectCandidate(kSessionId, index, resp)` 再
   `ApplyEngineResponse(cached_context_, resp)`——不需要给引擎/协议做任何改动
   （`kSelectCandidate` 方法从 M1 起就有，鼠标点击只是换了个调用方）。
5. **`cached_context_`**（`CMyabcTextService` 新成员，手动 `AddRef`/`Release`）：
   鼠标点击是异步事件，没有 `OnKeyDown` 那样现成的 `pic` 参数，必须自己存一份
   组字所在的 `ITfContext`。每次 `ApplyEngineResponse` 进入 `composing=true`
   分支都刷新一次；组字结束（任意路径）清空。

两个进程都是同一用户会话下 myabc 自己的进程，不做额外鉴权——跟现有命名管道
（默认信任同用户会话内的其它 myabc 进程）是同一个假设。

候选窗视觉上新增鼠标悬停高亮（浅灰，跟"架住态"的浅蓝区分），命中整行宽度
（跟高亮同宽），不用精确点在文字上。

## 被否决的备选方案

- **新开一条命名管道（UI 进程 -> TIP）**：候选点击只需要传一个整数（下标），
  两边又都已经各自有消息泵在跑（TIP 复用宿主应用 UI 线程本来就有的消息循环；
  `CandidateWindow` 本来就跑在自己的窗口线程里）——专门起一条管道纯属过度设计，
  且管道两端谁监听谁连接又要重新设计一遍握手/重连逻辑，比复用 Win32 消息更重。
- **引擎主动推送一个"强制选中"指令到 TIP**：引擎/UI 都不持有能让 TIP 反向调用
  的连接（现有 `ipc_client.hpp` 是 TIP 主动连引擎，方向固定），要做反向推送
  等于要在 TIP 里再实现一个"服务端"角色去接引擎的连接——比直接让 UI 进程用
  窗口消息通知同一台机器上的 TIP 更复杂，且不必要（TIP 已经有复用中的消息泵）。
- **按 sessionId 精确路由（例如把 sessionId 编进窗口标题，`FindWindowEx` 按
  标题过滤）**：现状 `kSessionId` 全局固定为 1、且系统同一时刻只有一个组字中的
  文档，精确路由没有实际意义，徒增复杂度；等未来真的要支持多文档同时组字（当前
  已知限制，见 `myabc_text_service.hpp` 类头 DECISION）时再一并解决。

## 影响范围

- 新增共享契约：`src/shared/click-bridge-protocol/click_bridge_protocol.hpp`
  （独立于 `src/shared/ipc-protocol/`，因为后者声明"只用标准库、不碰
  windows.h"供 MinGW 引擎编译——这个新契约只给 candidate-ui/tsf-service 两个
  纯 Windows 域用，混进去会破坏那条不变量）。
- 新增：`src/domains/tsf-service/backend/click_bridge.{hpp,cpp}`。
- 修改：`myabc_text_service.{hpp,cpp}`（`cached_context_`/`SetCachedContext`/
  `OnCandidateClicked`，`ApplyEngineResponse`/`Deactivate`/
  `OnCompositionTerminated` 里管理 `click_bridge_` 生命周期）。
- 修改：`src/domains/candidate-ui/backend/candidate_window.{hpp,cpp}`（鼠标
  命中测试 `ItemRectFor`/`HitTest`、悬停高亮、`WM_MOUSEMOVE`/`WM_MOUSELEAVE`/
  `WM_LBUTTONUP` 处理、点击后通知 TIP）。
- CMake：新增 `myabc-click-bridge-protocol`（INTERFACE），
  `myabc-candidate-ui`/`myabc-tip` 都链接它。
- 引擎/协议层**零改动**——`kSelectCandidate` 是既有方法，鼠标点击只是多了一个
  调用方（TIP 自己），跟数字键选字用的是同一条路径、同一份 `Session` 逻辑。

## 验证

- 新增独立编译的最小复现 `click_bridge_test.cpp`（不依赖 TSF/COM，纯 Win32）：
  验证 `ClickBridge` 创建、`FindWindowExW` 跨线程发现、`PostMessageW` 消息
  投递、回调在正确线程同步触发、`Destroy()` 后窗口确实清除、重复 `Create()`
  幂等——全部通过。这证明了跨进程信令机制本身是对的。
- MSVC ctest 3/3、`tests/review/run_all.sh` GREEN，既有回归无影响（引擎/协议
  层零改动，`SelectCandidate` 早已被数字键选字路径覆盖验证过）。
- **未覆盖、需要真机验证**：真实在记事本/浏览器等宿主应用里用鼠标点击候选窗
  一行，确认文字真的上屏——这需要真实的 TSF 宿主环境 + 真实鼠标交互，当前
  开发环境无法用自动化脚本驱动（跟候选窗定位/高亮渲染这类视觉行为一样，此前
  都是靠真机测试兜底，不是本项目自动化测试的空白点）。请用户在装好新版本后
  实机验证：点击候选窗任意一行，文字应正确插入光标位置，行为应跟按对应数字键
  一致（含"点击后剩余候选还需要继续选字"的分段场景）。

## 关联

- `docs/decisions/_debt-log.md` 2026-09-12 置顶条目「候选窗支持鼠标选择」。
- `src/domains/tsf-service/backend/myabc_text_service.hpp` 类头 DECISION（单一
  session/单一活跃组合简化，本决策依赖这个前提）。
