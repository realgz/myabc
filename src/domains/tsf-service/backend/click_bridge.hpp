// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/tsf-service/backend/click_bridge.hpp --- 接收候选窗鼠标点击的隐藏窗口
//
// 依据：src/shared/ipc-protocol/click_bridge.hpp（跨进程契约）
//       docs/decisions/tsf-service/20260912-mouse-candidate-select.md
//
// 生命周期跟"当前是否正在组字"同步（Create() 在组字开始时调，Destroy() 在组字
// 结束/取消/被外部终止/TIP 停用时调）——见共享头文件里关于"全系统同一时刻最多一个"
// 的说明，这保证了候选窗用类名查找时不需要处理二义性。
//
// 直接复用宿主应用 UI 线程本来就有的消息泵（TSF 的按键回调本身就跑在这个线程上），
// 不像 candidate-ui 的 CandidateWindow 那样需要自己另起线程/消息循环。

#ifndef MYABC_TSF_CLICK_BRIDGE_HPP
#define MYABC_TSF_CLICK_BRIDGE_HPP

#include <windows.h>

#include <functional>

namespace myabc::tsf {

class ClickBridge {
public:
    ClickBridge() = default;
    ~ClickBridge();

    ClickBridge(const ClickBridge&) = delete;
    ClickBridge& operator=(const ClickBridge&) = delete;

    // on_select：候选窗点击第 index 项（0-based，当前页内位置）时的回调，在
    // WndProc 所在线程（即调用 Create() 的这个线程）上同步调用。
    bool Create(std::function<void(int index)> on_select);
    void Destroy();
    bool active() const noexcept { return hwnd_ != nullptr; }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    HWND hwnd_ = nullptr;
    std::function<void(int)> on_select_;
};

}  // namespace myabc::tsf

#endif  // MYABC_TSF_CLICK_BRIDGE_HPP
