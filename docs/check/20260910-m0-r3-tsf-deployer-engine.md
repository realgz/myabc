# M0 R3 质检报告 · 三领域骨架（TIP DLL / deployer / engine）

- 日期：2026-09-10
- 里程碑：M0（plan 01 §3.3 + §3.4 + §3.5）
- 对照计划：docs/plan/01-m0-tsf-skeleton-plan.md
- 执行方：主会话（Sonnet）
- 结论：**构建/静态验证通过**。三个产物 x64+x86 编译链接干净，引擎 IPC 端到端冒烟通过。
  **实机注册 + 记事本上屏（M0-5/6/7/8/9/10）待用户在真机执行**（需管理员 + GUI 交互）。

## 1. 本轮交付

### src/domains/input-engine → myabc-engine.exe（MinGW UCRT64）
| 文件 | 内容 |
|---|---|
| logic/engine_main.cpp | `--selftest`（M0 占位打印，退 0）/ 正常模式进管道循环；`--pipe` `--sid` `--model-dir` 参数；取当前用户 SID |
| logic/dispatcher.{hpp,cpp} | 纯逻辑：hello→{engineVersion,protocol}；processKey→{handled:false}；shutdown→置位；未知/未实现方法→结构化 error(-3) |
| backend/pipe_server.{hpp,cpp} | 同步 accept 循环 + `CreateNamedPipe`；帧读写走 myabc::ipc；断开回到 accept |
| backend/single_instance.{hpp,cpp} | `CreateMutex(Local\myabc-engine-<sid>)` |
| backend/libpinyin_wrapper.hpp | 空占位 + TODO(M1) |

### src/domains/deployment → myabc-deployer.exe + myabc-deploy-core.lib（MSVC）
| 文件 | 内容 |
|---|---|
| backend/com_register.{hpp,cpp} | `HKCR\CLSID\{clsid}\InprocServer32` 写/删/查，ThreadingModel=Apartment |
| backend/tsf_register.{hpp,cpp} | `ITfInputProcessorProfiles::Register`+`AddLanguageProfile`；`ITfCategoryMgr` 注册 TIP_KEYBOARD + 6 项 Win8 TIPCAP；`IsProfileRegistered` 重枚举校验 |
| logic/deploy_flow.{hpp,cpp} | Register/Unregister/Status 编排 + 逐步结果打印 + 反注册残留校验 |
| ui/main.cpp | `--register`/`--unregister`/`--status` `[--langid <hex>]`；`CoInitializeEx(APARTMENTTHREADED)` |

### src/domains/tsf-service → myabc-tip.dll（MSVC，PREFIX 空）
| 文件 | 内容 |
|---|---|
| backend/dll_main.cpp | `DllMain`(记 hinst + DisableThreadLibraryCalls)；`DllGetClassObject`/`DllCanUnloadNow`(依对象计数)/`DllRegisterServer`/`DllUnregisterServer`(复用 deploy-core) |
| backend/class_factory.{hpp,cpp} | `CClassFactory : IClassFactory`，进程内静态单例，`LockServer`→DllLock 计数 |
| backend/dll_refcount.{hpp,cpp} | 全局对象/锁计数，`DllCanUnload()` |
| backend/myabc_text_service.{hpp,cpp} | `CMyabcTextService : ITfTextInputProcessorEx, ITfThreadMgrEventSink, ITfKeyEventSink`；Activate/ActivateEx→AdviseKeyEventSink + AdviseSink，Deactivate 对称 Unadvise；OnTestKeyDown 对 'A' 本地同步吃键；OnKeyDown 对 'A' 起 EditSession 上屏"啊"；Activate 时 IPC hello 一次 |
| backend/edit_session.{hpp,cpp} | `CInsertTextEditSession : ITfEditSession`，`InsertTextAtSelection` + Collapse |
| backend/ipc_client.{hpp,cpp} | `CreateFile` 管道 + 超时重试 + 失败拉起引擎；同步 `Call`；`Hello()` |
| logic/key_router.{hpp,cpp} | M0：`IsInterestedKey(vk)` → vk=='A' |
| logic/composition_state.hpp | `enum CompositionState { Idle, Composing }`（M0 只 Idle） |
| tsf-service.def | 导出 4 个 Dll* |

### 工程
- `cmake/myabc-common.cmake`：产物集中到 `<binaryDir>/bin`（DLL/exe/导入库）、`/lib`（静态库），便于 stage.ps1。
- `scripts/build.ps1` / `stage.ps1`：`-Arch` 接受逗号分隔。
- `src/config/config_defaults.hpp`：新增 `kDefaultLangId` 常量，deployer/dll_main 引用它（消除裸 `0x0804`）。

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| M0-1 `build.ps1 -Arch x64,x86` | PASS | `bin/` 下 myabc-tip.dll(x64,x86)、myabc-deployer.exe；cl /W4 /permissive- 0 警告 |
| M0-2 `build-engine.sh` | PASS | `bin/myabc-engine.exe`；gcc -Wall -Wextra -Wpedantic 0 警告；`ldd` 无非系统依赖（M0 全静态） |
| M0-3 `stage.ps1 -Config Release` | PASS | `out/Release/{x64,x86}/` + `out/Release/engine/`（engine.exe + libpinyin.dll + 8 MinGW 运行时 DLL）+ config.sample.toml |
| M0-4 `myabc-engine.exe --selftest` | PASS | 打印 `selftest: libpinyin not wired yet (M0)...`，退出 0 |
| 引擎 IPC 端到端冒烟 | PASS | 外部 Python 管道客户端：hello→{engineVersion:"0.0.0-m0",protocol:1}；processKey→{handled:false}；bogus→error{code:-3}；shutdown→ok 且引擎进程退出码 0 |
| myabc-tip.dll 导出 | PASS | `objdump -p`：DllCanUnloadNow / DllGetClassObject / DllRegisterServer / DllUnregisterServer |
| 不变量 1（DLL 不链 libpinyin/glib/db） | PASS | `objdump -p myabc-tip.dll` 依赖仅 ole32/advapi32/kernel32 + MSVC CRT，无 libpinyin/glib/libdb |
| `ctest`（x64 / mingw） | PASS | ipc_roundtrip 两侧绿 |
| `tests/review/run_all.sh` | GREEN | check_structure PASS；check_hardcode 扫描 35 源文件 0 违规 |
| M0-5/6/7/8/9/10（实机注册 + 记事本） | **待用户执行** | 见 §4 |

## 3. 计划偏差 / 决策（登记 _debt-log 2026-09-10）

1. **TSF/COM 代码来源**：按 TSF 文档 + SampleIME(MIT) 结构自行编写，无逐行拷贝，故未携带 SampleIME 版权头；
   源内 `# 参考微软 SampleIME` 注释仅指明结构来源。详见 _debt-log。
2. **`msctf.lib` 不存在**：TSF 是纯 COM，无导入库；CLSID/IID 来自 `uuid.lib`。已从链接项移除。
3. **myabc-tip.dll 用动态 CRT**（MSVCP140/VCRUNTIME140）：注入任意宿主进程时依赖 VC 运行时存在。
   Win10/11 多数已装；M6 兼容加固时评估改静态 CRT（`/MT`）或随包分发 redist。
4. **engine.exe 布局**：`ipc_client` 按 `<tip.dll 目录>\myabc-engine.exe` 解析引擎路径；
   但 stage.ps1 把 engine 放 `out/Release/engine/`。实机测试需把 engine 及其 DLL 放到与注册的
   tip.dll 同目录（见 §4），或 M0 R4 起在 config 里改 `engine.exe_path` 为相对 `..\engine\`。
5. **engine/ 目录的 libpinyin.dll 等 8 个 DLL**：M0 引擎全静态用不到，为 M1 预留；stage 先带上无害。
6. **idle_exit_minutes 未实现**：M0 引擎只响应 shutdown 退出；overlapped connect + 空闲自退出 + 多连接并发留 M2（plan 03）。

## 4. 实机验收步骤（M0-5 ~ M0-10，需管理员 + 交互，用户执行）

```powershell
# 1) 组织安装目录（tip.dll 与 engine 同目录，M0 布局）
$dst = "E:\work\myabc\out\Release\install-x64"
New-Item -ItemType Directory -Force $dst
Copy-Item E:\work\myabc\out\Release\x64\* $dst
Copy-Item E:\work\myabc\out\Release\engine\* $dst   # engine.exe + 运行时 DLL

# 2) 注册（管理员 PowerShell）
& "$dst\myabc-deployer.exe" --register      # 期望每步 [ok]，verify: profile present
& "$dst\myabc-deployer.exe" --status        # 期望 profile registered / InprocServer32 指向 $dst\myabc-tip.dll

# 3) 交互（M0-6/7/8）
#   打开 notepad.exe -> Win+Space 切到 "智能ABC (myabc)"
#   按 a -> 出现 "啊"；按 b -> 出现 b；数字/方向键正常
#   DebugView 或调试器看 OutputDebugString: "[myabc] engine hello -> \"0.0.0-m0\""（M0-8）

# 4) 反注册 + 残留检查（M0-9/10）
& "$dst\myabc-deployer.exe" --unregister
& "$dst\myabc-deployer.exe" --status        # 期望 not registered / not registered
reg query "HKCR\CLSID\{8BA238DD-B6B4-42B4-95C1-8062D25B3652}"   # 期望：找不到
#   重开 notepad -> 切换菜单无 myabc
```

回归判据（历史真实坑）：
- 卸载不彻底：`--unregister` 后 `HKCR\CLSID\{clsid}` 必须整键消失（deploy_flow 已 `RegDeleteTree` + 校验）。
- x86 宿主：用 32 位程序（或 SysWOW64\notepad.exe 若存在）验证 x86 tip.dll 也能加载（需另注册 x86 InprocServer32——M0 deployer 目前只按自身位数写一处，x86/x64 双注册留 M6）。

## 5. 遗留（M0 R4 / 后续）

- **M0 R4 = 实机跑通 §4 + 修实机暴露的问题**，然后打 tag `m0-skeleton`。
- deployer 只注册自身位数的 InprocServer32；x86+x64 双注册（WOW6432Node）留 M6（plan 07）。
- `register.ps1` 脚本（plan 00 §5）未写：deployer 已覆盖其功能，按需再加薄封装。
- candidate-ui 域：M1（plan 02）。
