# M1 R2 质检报告 · TSF 侧组字显示 + 候选窗 + 中英切换

- 日期：2026-09-11
- 里程碑：M1（plan 02-m1-libpinyin-quanpin-plan.md §3.4/§3.5）
- 执行方：主会话（Sonnet）
- 结论：**构建/静态验证通过**（x64+x86，0 警告）。**M1-3~M1-13 的记事本 GUI 交互部分
  待用户在真机执行**（需要肉眼确认候选窗外观、按键效果，我无法用现有工具驱动 Win32
  桌面 GUI）。M1-12（引擎超时降级）也在此列——那是"杀掉引擎进程后在记事本按键"的
  交互场景，无法脱离真实宿主验证。

## 1. 本轮交付

| 文件 | 内容 |
|---|---|
| `backend/composition.{hpp,cpp}` | `CompositionController`：`StartOrUpdate`/`EndWithText`/`Cancel`，各起一次 `ITfEditSession`（不变量 5）；`StartOrUpdate` 顺带用 `ITfContextView::GetTextExt` 取光标屏幕矩形供候选窗定位 |
| `logic/composition_state.{hpp,cpp}` | 纯数据：把引擎 `Response.result` 解析成 `{composing, preedit, candidates, page, commit}` |
| `logic/mode_manager.{hpp,cpp}` | Shift 单击切换中/英文（本地判定，不经 IPC，见 M1-9） |
| `logic/key_router.{hpp,cpp}`（重写） | 按 `(vk, ch, composing, mode)` 判定要不要吃键；英文模式从不吃；标点 ASCII 集合与引擎侧 `punctuation_source.cpp` 手动保持一致（跨工具链边界，见 debt-log） |
| `backend/ipc_client.{hpp,cpp}`（重写） | overlapped IO + 有界等待（`BoundedRead`/`BoundedWrite`，`CancelIoEx` 超时收尾）；`InitSession`/`ProcessKey`/`SelectCandidate`/`PageCandidates`/`CommitComposition`/`CancelComposition`/`FocusOut` 全量方法 |
| `backend/text_convert.{hpp,cpp}` | UTF-8/UTF-16 互转公共小工具（原分散在 3 个文件里的重复代码收拢到一处） |
| `backend/myabc_text_service.{hpp,cpp}`（重写） | `ActivateEx` 建候选窗 + `InitSession`；`OnKeyDown`/`OnTestKeyDown`/`OnKeyUp` 全量按键路由 + IPC 往返 + 应用结果；引擎未接住/超时时原样插入字符兜底，不丢键；`ITfCompositionSink::OnCompositionTerminated` 处理外部中止 |
| `src/domains/candidate-ui/`（新域） | `backend/candidate_window.{hpp,cpp}`：GDI 候选窗，独立线程+消息泵；`logic/candidate_view_model.hpp`：纯数据 |
| `src/config/config_defaults.hpp` | 加 `UiConfig{font, font_size_pt}` |

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| MSVC x64 构建 | PASS | `cl /W4 /permissive-`：**0 警告 0 错误**（含 COM/GDI/overlapped IO 全部新代码） |
| MSVC x86 构建 | PASS | 同上 |
| 不变量 1（DLL 不链 libpinyin/glib） | PASS | `objdump -p myabc-tip.dll`：依赖新增仅 `GDI32.dll`/`USER32.dll`（候选窗），仍无 libpinyin/glib |
| 不变量 5（文档改动只在 EditSession 内） | PASS（代码审查） | `composition.cpp` 的三个操作全部经 `RequestEditSession(... TF_ES_SYNC \| TF_ES_READWRITE ...)` |
| `ctest` / `tests/review/run_all.sh` | PASS/GREEN | 无回归；check_hardcode 扫描 61 源文件 0 违规 |
| M0 已验收的注册/记事本基本上屏 | 未回归（结构性保证） | DLL 换新后重新拷到 `install-x64/`；`myabc-deployer --status` 仍报 registered，无需重新注册（InprocServer32 路径不变） |
| M1-3 ~ M1-13（记事本交互） | **待用户执行** | 见 §3 |

## 3. 待用户在真机验证的场景（TIP DLL 已更新到 `out\Release\install-x64\myabc-tip.dll`）

> 前置：已加载旧版 `myabc-tip.dll` 的进程（本次实测环境里是一个记事本 + WindTerm）需要
> **重启**才能用上新代码——DLL 文件已换新，但已在跑的进程还留着旧版在内存里。

| # | 操作 | 期望 |
|---|---|---|
| M1-3 | 记事本输入 `nihao`，按空格 | 候选窗出现，首项"你好"；空格后文档写入"你好"，候选窗和预编辑消失 |
| M1-4 | 输入 `woshizhongguoren` | 候选窗首项"我是中国人" |
| M1-5 | 输入 `shiyan`，按 `-`/`=` | 候选翻页，候选窗内容换页 |
| M1-6 | 组字中按数字键（如 `3`） | 上屏对应候选，或确定该词后继续组剩余拼音（两种都算通过，见 plan 原文） |
| M1-7 | 组字中按退格 | 删最后一个拼音字母，候选窗刷新 |
| M1-8 | 组字中按 Esc | 预编辑清空，候选窗消失，无上屏 |
| M1-9 | 单击 Shift 后输入 `abc` | 直接上屏 `abc`（英文字母），不弹候选窗 |
| M1-10 | 中文模式按 `,` 和 `.` | 上屏中文逗号"，"和句号"。" |
| M1-11 | 观察候选窗位置 | 紧邻文本插入点（光标处），随光标移动/换行更新 |
| M1-12 | 用任务管理器结束 `myabc-engine.exe`，回记事本按字母键 | 几十毫秒内应有反应（不长时间卡住/无响应），字符原样上屏（兜底路径），DebugView 应看到 `processKey timeout/IO error` 日志 |

回归判据（历史坑，随手确认）：
- 候选窗不应抢焦点（`WS_EX_NOACTIVATE`），记事本光标应始终保持在编辑区闪烁。
- 结束组字/上屏后候选窗必须真正消失（不留空壳窗口）。

## 4. 计划偏差 / 决策（详见 docs/decisions/_debt-log.md 2026-09-11 置顶各条）

1. 候选窗用 GDI，非 Direct2D/DirectWrite。
2. `ToUnicode` 做字符映射，有死键副作用的已知局限（非西欧布局，M6 评估）。
3. `OnTestKeyUp` 独立调用与 `OnTestKeyDown` 相同判定（非严格配对同一次按键）。
4. 候选窗字号按 96 DPI 近似换算，无 Per-Monitor-V2 感知（M2 补）。
5. composition/session 仍是"单一活跃焦点"简化（承接 M1 R1 同类简化）。

## 5. M1 收尾清单

- [ ] 用户完成 §3 全部场景并反馈结果
- [ ] 若有失败项，回到相应文件修 + 重新构建验证
- [ ] 全部通过后打 tag `m1-quanpin`
