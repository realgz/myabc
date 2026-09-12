// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/candidate-ui/backend/candidate_window.hpp --- 候选窗（GDI，独立线程）
//
// 依据：docs/plan/02-m1-libpinyin-quanpin-plan.md §3.4
//
// DECISION: 用 GDI（BeginPaint/TextOut）而非计划里写的 Direct2D/DirectWrite。
// 理由：候选列表窗就是几行文字 + 高亮矩形，D2D/DirectWrite 在 M1 这个复杂度下只增加
// 依赖面（d2d1/dwrite/dcomp）和样板代码，收益（子像素渲染、动画）在候选窗小窗口场景
// 不明显；M2 候选窗要迁到独立 UI 进程，届时如果要做视觉升级，届时一起重写渲染层成本
// 更低。见 docs/decisions/_debt-log.md 2026-09-11。
//
// "自己的线程 + 消息泵，不占 TIP 线程"（plan 原文）保留：Create() 内部起线程建窗口、
// 跑消息循环；Show/Hide/UpdateModel 从调用方线程投递消息，线程安全。

#ifndef MYABC_CANDIDATE_UI_CANDIDATE_WINDOW_HPP
#define MYABC_CANDIDATE_UI_CANDIDATE_WINDOW_HPP

#include <windows.h>

#include <string>

#include "candidate_view_model.hpp"

namespace myabc::ui {

class CandidateWindow {
public:
    CandidateWindow() = default;
    ~CandidateWindow();

    CandidateWindow(const CandidateWindow&) = delete;
    CandidateWindow& operator=(const CandidateWindow&) = delete;

    // 起窗口线程 + 建窗口（初始隐藏）。font_name/font_size_pt 来自 config.ui。
    bool Create(const std::wstring& font_name, unsigned font_size_pt);
    void Destroy();

    // anchor：屏幕坐标（通常是光标处的矩形，取其左下角），来自
    // ITfContextView::GetTextExt。跨线程安全。
    void Show(const RECT& anchor, const CandidateViewModel& model);
    void Hide();

private:
    struct ShowRequest {
        RECT anchor;
        CandidateViewModel model;
    };

    static DWORD WINAPI ThreadProc(LPVOID param);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

    void RunOnWindowThread();
    void HandleShow(ShowRequest* req);   // 取得所有权，用完 delete
    void Paint(HWND hwnd);

    // 鼠标支持（见 docs/decisions/tsf-service/20260912-mouse-candidate-select.md）：
    // 候选行的屏幕布局是从 model_.items.size() + 固定的 kPaddingPx/kLineHeightPx 常量
    // 纯计算出来的（Paint() 画的也是这份计算结果），不需要另存一份"上次画的矩形"，
    // 命中测试直接复用同一个函数，永远跟实际绘制一致。
    RECT ItemRectFor(std::size_t index) const;   // 相对客户区坐标，整行宽度（跟高亮同宽）
    int HitTest(POINT client_pt) const;          // 命中的候选下标；未命中 -1
    void HandleMouseMove(HWND hwnd, POINT client_pt);
    void HandleLButtonUp(POINT client_pt);
    void NotifyTipOfClick(int index) const;   // 见 shared click-bridge-protocol

    HANDLE thread_ = nullptr;
    HANDLE ready_event_ = nullptr;
    HWND hwnd_ = nullptr;
    HFONT font_ = nullptr;
    CandidateViewModel model_;   // 只在窗口线程读写
    int hover_index_ = -1;       // 鼠标悬停高亮，-1 = 无；WM_MOUSELEAVE 时清空
    bool tracking_mouse_ = false;   // TrackMouseEvent 是否已投递，避免重复调用
};

}  // namespace myabc::ui

#endif  // MYABC_CANDIDATE_UI_CANDIDATE_WINDOW_HPP
