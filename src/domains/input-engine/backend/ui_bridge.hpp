// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/ui_bridge.hpp --- 引擎 -> myabc-ui 单向推送通道
//
// 依据：docs/plan/03-m2-engine-process-ipc-plan.md §3.1/§3.2
//       docs/architecture/system-overview.md §4.1（uiShow/uiHide，独立管道 myabc-ui-{sid}）
//
// 引擎在自己的命名管道 `\\.\pipe\myabc-ui-{sid}` 上做服务端；myabc-ui.exe 连上来（由本类
// 按需 CreateProcess 拉起，myabc-ui 自己的命名互斥量保证单实例，engine 端重复拉起无害）。
// 只推不收：引擎从不读这条管道；写失败即视为断线，下次推送时才重新尝试拉起 + 等新连接。
//
// 线程模型：接受连接在后台线程里跑（ConnectNamedPipe 是阻塞调用），PushShow/PushHide
// 从 dispatcher 所在线程（pipe_server 的 ServeConnection 线程）调用，用临界区保护
// 当前连接句柄；不做无界等待——WriteFile 失败就地放弃这次推送，不阻塞按键响应路径。

#ifndef MYABC_ENGINE_UI_BRIDGE_HPP
#define MYABC_ENGINE_UI_BRIDGE_HPP

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "libpinyin_wrapper.hpp"

namespace myabc::engine {

struct CaretRect {
    int x = 0, y = 0, w = 0, h = 0;
};

class UiBridge {
public:
    // pipe_name：本进程要监听的 UI 管道名（已展开 {sid}）。
    // ui_exe_path：myabc-ui.exe 绝对路径，为空则不自动拉起（仅供测试）。
    UiBridge(std::string pipe_name, std::string ui_exe_path);
    ~UiBridge();

    UiBridge(const UiBridge&) = delete;
    UiBridge& operator=(const UiBridge&) = delete;

    // armed_index：智能ABC 风格空格两段式确认（v6，见 protocol.hpp DECISION）。
    // -1 = 无；>=0 = 当前页这个下标的候选被"架住"，UI 应高亮但不当作已选中。
    void PushShow(std::uint32_t session_id, const CaretRect& rect, const std::string& preedit,
                 const std::vector<CandidateItem>& candidates, int page_index, int page_size,
                 int page_total, int armed_index);
    void PushHide(std::uint32_t session_id);

private:
    static DWORD WINAPI AcceptThreadProc(LPVOID param);
    void RunAcceptLoop();
    void EnsureUiSpawned();
    bool WriteJson(const std::string& utf8_json);

    std::string pipe_name_;
    std::string ui_exe_path_;

    HANDLE accept_thread_ = nullptr;
    std::atomic<bool> stop_{false};

    CRITICAL_SECTION lock_{};
    HANDLE current_pipe_ = INVALID_HANDLE_VALUE;   // 受 lock_ 保护
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_UI_BRIDGE_HPP
