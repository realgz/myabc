// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/shared/click-bridge-protocol/click_bridge_protocol.hpp
// --- 候选窗鼠标点击 -> TIP 的跨进程通知契约
//
// 依据：docs/decisions/tsf-service/20260912-mouse-candidate-select.md
//
// 背景：myabc-ui.exe（候选窗）和 TIP（myabc-tip.dll，加载在宿主应用进程里）是两个
// 独立进程；候选明细/上屏（ITfComposition）必须由 TIP 在自己的宿主进程里、通过
// TSF 的 EditSession 完成——引擎命名管道（src/shared/ipc-protocol 那套长度前缀
// JSON 帧）是"TIP 主动问、引擎答"的单向请求通道，反过来引擎/UI 没有办法主动让
// 某个具体的 TIP 实例去改文档。鼠标点击发生在 UI 进程里，需要一条能"从 UI 进程
// 通知回 TIP 所在宿主进程"的路——这里不新开一条命名管道，而是用 Win32 窗口消息：
//
//   1. TIP 只在"当前正在组字"（composing=true）期间维护一个隐藏的 message-only
//      窗口，类名固定为 kClickBridgeWindowClass；组字结束（commit/cancel）立即
//      销毁。系统同一时刻只有一个文档能处于"正在组字"（键盘事件天然只送到当前
//      焦点控件），所以任意时刻全系统最多只存在一个这样的窗口——不需要按
//      sessionId 区分，也不会有"点到哪个 TIP 实例"的二义性。
//   2. 候选窗（myabc-ui.exe）收到鼠标点击、命中某一行候选后，用
//      FindWindowExW(HWND_MESSAGE, nullptr, kClickBridgeWindowClass, nullptr)
//      找到这个窗口，PostMessageW(hwnd, kWmSelectCandidateByClick, index, 0)。
//   3. TIP 的 WndProc 收到消息后，直接走跟数字键选字完全相同的代码路径：调
//      IpcClient::SelectCandidate(kSessionId, index, ...) 再 ApplyEngineResponse(...)。
//      两个进程都是同一用户会话下 myabc 自己的进程，不做额外鉴权——跟现有命名
//      管道（默认信任同用户会话内的其它 myabc 进程）是同一个假设。
//
// 只用 Win32 消息、不用命名管道：这条通道只传一个整数（候选下标），双方还都已经
// 有各自的消息泵（TIP 复用宿主应用 UI 线程本来就有的消息循环；myabc-ui.exe 的
// CandidateWindow 本来就跑在自己的窗口线程里）——新开一条管道纯属过度设计。
//
// DECISION: 独立成 src/shared/click-bridge-protocol/（而不是塞进
// src/shared/ipc-protocol/），因为后者声明"只用标准库，不碰 windows.h"（供 MinGW
// 引擎侧直接编译，见 docs/architecture/system-overview.md §7 不变量 6）——本文件
// 依赖 <windows.h>（UINT/WM_APP/HWND 消息常量），只给 candidate-ui 和 tsf-service
// 两个纯 Windows 域用，不应该混进那份"引擎也要编"的协议库。

#ifndef MYABC_SHARED_CLICK_BRIDGE_PROTOCOL_HPP
#define MYABC_SHARED_CLICK_BRIDGE_PROTOCOL_HPP

#include <windows.h>

namespace myabc::ipc {

inline constexpr wchar_t kClickBridgeWindowClass[] = L"MyabcTipClickBridge";

// wParam = 候选下标（int，当前页内的 0-based 位置，跟 select_keys/index 语义一致）。
// lParam 未使用。
inline constexpr UINT kWmSelectCandidateByClick = WM_APP + 100;

}  // namespace myabc::ipc

#endif  // MYABC_SHARED_CLICK_BRIDGE_PROTOCOL_HPP
