# 代码复核清单（code-review-checklist）

- 状态：生效
- 更新日期：2026-09-09
- 覆盖 AGENTS.md §3.3 通用项 + myabc IME 专属红线
- 标注"[自动]"的由 tests/review/ 校验；其余人工

## A. 通用（每次 diff 必查）

### A1 决策留痕
- [ ] [自动] 决策承载文件（含 GUID 定义、IPC 协议、工具链分派、DB 后端选择、适配器注册表、
      libpinyin 封装、笔形/数字规则）均有 `DECISION: docs/decisions/...md` 注释。（check_decision_comments）
- [ ] 新的、无法从代码推断的设计选择，已在 docs/decisions/ 建 ADR（按 decision-protocol 模板）。
- [ ] 无 `// NOTE:` / `// 见讨论` 等不可追溯写法。

### A2 反硬编码
- [ ] [自动] 无裸的管道名（`\.\pipe\`）、超时数字、页大小、选字/翻页键、langid(0x0804)、
      字体名、模型/词库路径散落在业务代码。（check_hardcode）
- [ ] 所有上述值来自 src/config 的 Config 对象或 assets/config/*.toml。
- [ ] 环境相关值按需分层，代码只 import 配置对象。

### A3 适配器 / 反多分支
- [ ] InputSchemeParser / DbBackend / CandidateSource / CharsetFilter / IpcTransport / IpcCodec
      为统一接口；新增变体 = 新类 + 注册表登记，未修改既有分发链。
- [ ] 无覆盖 3+ 情况且在增长的 if/elif 或 switch 分发。

### A4 领域分层
- [ ] [自动] ui/ 未 import 同域 backend/。（check_structure）
- [ ] [自动] 跨域未直接 import 对方 backend/；跨域只经对方 logic 接口。（check_structure）
- [ ] 每个 domain 目录只有 ui/ logic/ backend/ 三层，无散放文件。
- [ ] 纯技术无业务函数在 src/shared，且无副作用（cn_number、frame 编解码等）。

### A5 影响范围 / 回滚
- [ ] 改既有代码前存在 .ag_state/{session}/05_IMPACT.md（Hook 硬拦截）。
- [ ] 每个里程碑完成打了 git tag（回滚锚点）。
- [ ] 破坏性动作（注册表写、文件删）可逆；deployer --unregister 能完全还原。

### A6 License
- [ ] [自动] 每个源文件头有 `SPDX-License-Identifier: GPL-3.0-or-later`。（check_spdx）
- [ ] 从 SampleIME(MIT)/PIME(LGPL)/其它拷贝的片段保留原版权头，并在 docs/decisions/_debt-log.md 登记。
- [ ] 未把 GPL 代码链入需非 GPL 分发的目标。

## B. IME 专属红线（违反即打回）

### B1 薄 DLL 隔离
- [ ] [自动] src/domains/tsf-service/** 与 src/domains/candidate-ui/**（当其链入 DLL 时）
      不 include：pinyin.h、glib.h、glib-object.h、db.h、kchashdb.h，不 include input-engine/backend/**。
      （check_tip_isolation）
- [ ] [自动] dumpbin/objdump 检查 myabc-tip.dll 不依赖 d2d1.dll、dwrite.dll、libpinyin*、libglib*。
      （check_dll_deps，M2 起启用）
- [ ] TIP DLL 进程内不加载词库/语言模型/笔形表等大数据文件。

### B2 UI 线程不阻塞
- [ ] OnTestKeyDown 只用本地状态机同步判定，无 IPC 往返。
- [ ] OnKeyDown 的 IPC 等待有界（<= ipc.request_timeout_ms）或全异步；无无限 WaitForSingleObject
      / 同步阻塞读管道在宿主 UI 线程。
- [ ] 候选窗绘制在独立线程（M1）或独立进程（M2+），不在 TIP 输入线程做 Direct2D。

### B3 COM 正确性
- [ ] 每个接口实现 AddRef/Release 配平；对象析构由 Release 触发，无裸 delete this 误用。
- [ ] QueryInterface：支持的 IID 返回 S_OK 并 AddRef，其余返回 E_NOINTERFACE 且 *ppv=nullptr。
- [ ] DllCanUnloadNow 依据全局对象计数 + 服务器锁计数。
- [ ] DllGetClassObject 仅对已知 CLSID 返回工厂。
- [ ] Advise/Unadvise、CoInitialize/CoUninitialize、每个 AddRef 在 Deactivate/析构对称释放。
- [ ] 用 wil::com_ptr / ATL CComPtr 管理，不裸指针跨作用域。

### B4 TSF 用法
- [ ] 文档修改只在 ITfEditSession::DoEditSession 内；RequestEditSession 传对的 flags。
- [ ] composition 生命周期完整：Start/Update/End 配对；处理 OnCompositionTerminated。
- [ ] 回调在 TSF 指定线程上执行；跨线程用 PostMessage 到该线程。
- [ ] 只读上下文（TS_SD_READONLY）不尝试写，放行原键。

### B5 IPC 协议
- [ ] [自动] 若 src/shared/ipc-protocol 的 schema 文件相对基线有改动，PROTOCOL_VERSION 常量必须变，
      且编码端与解码端、docs/architecture/system-overview.md §4.1 三处同步。（check_protocol_version）
- [ ] 跨 MinGW/MSVC 边界的结构体无指针、无 long（用 int32/int64）、无 STL 容器直传、无异常跨界。
- [ ] 帧读写处理短读/连接中断/超大长度（上限校验）。

### B6 引擎稳健
- [ ] 引擎顶层有崩溃保护（SetUnhandledExceptionFilter + minidump），崩前尽力 pinyin_save。
- [ ] 用户库原子写（临时文件 + g_rename），中断不损坏原库。
- [ ] libpinyin 调用返回值检查；gchar* 及时 g_free，不外泄到 wrapper 之外。
- [ ] 单实例互斥；多宿主并发时用户库写串行化。

## C. 提交前
- [ ] tests/review/run_all 全绿。
- [ ] 对应里程碑 acceptance-criteria 勾选完成。
- [ ] 不可逆操作（commit/push/删除/注册表）前已经用户确认。

---

## D. tests/review/ 与 src/review/ 应固化的可执行检查

| 检查脚本（tests/review/） | 依据条目 | 判定逻辑 |
|---|---|---|
| check_structure.py | A4 | 遍历 src/domains/*，确认只有 ui/logic/backend；解析 include/import，ui 不引 backend，跨域不引 backend |
| check_tip_isolation.py | B1 | grep src/domains/tsf-service、candidate-ui 源码的 #include，命中禁列（pinyin.h/glib*/db.h/kchashdb.h/input-engine/backend）即 fail |
| check_dll_deps.py | B1 | 对 out/*/myabc-tip.dll 跑 dumpbin /dependents（或 llvm-readobj），断言不含 d2d1/dwrite/libpinyin/libglib |
| check_hardcode.py | A2 | 正则扫 src/**（排除 src/config、tests）：`\\\.\pipe\`、`0x0804`、疑似超时魔数、页大小字面量、常见字体名字符串 |
| check_decision_comments.py | A1 | 对一份"决策承载文件 glob 清单"（config/guids.cpp、shared/ipc-protocol/protocol.hpp、顶层 CMakeLists、scheme_registry、*_backend、libpinyin_wrapper、bihuoma_table、cn_number、charset filter registry）逐个确认含 `DECISION:` |
| check_spdx.py | A6 | 每个 *.cpp/*.h/*.hpp/CMakeLists.txt 首 10 行含 SPDX 标识 |
| check_protocol_version.py | B5 | git diff 基线 vs HEAD：若 ipc-protocol/*.hpp 变更，则 PROTOCOL_VERSION 的值必须变；grep 三处（协议头、编码器、架构文档）版本号一致 |
| run_all.(sh\|ps1) | — | 依次跑上述 + ctest（tests/unit）；任一 fail 则非零退出 |

src/review/ 放这些检查复用的库代码（include 解析器、依赖清单常量、决策承载文件 glob 清单），
每个检查器源码头部加注释指回本文件对应行。本文件对应条目在"依据条目"列注明由哪个脚本校验。
