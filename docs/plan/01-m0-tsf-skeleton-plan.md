# 01 · M0 最小链路计划（TSF 骨架 + IPC echo）

- 状态：生效
- 更新日期：2026-09-09
- 前置：00 号计划全部验收判据（B1-B5）通过
- 对应需求：smart-abc-ime-requirements.md 第3.3节"最小链路验证"
- 回滚锚点：完成后打 tag m0-skeleton

## 1. 背景与目标

问题定位：空仓，无任何 src 代码。需要一个能注册、能被系统选中、能拦截按键、能上屏的
最小 TSF TIP，外加一个能跑 IPC echo 的引擎进程骨架，验证整条链路的每个接缝。

完成后可观察效果：
- myabc-deployer.exe --register 后，记事本右下角输入法列表出现"智能ABC (myabc)"。
- 在记事本选中该输入法，按 a 键 -> 文档里出现"啊"（写死），按其它键正常。
- 引擎骨架：myabc-tip.dll 连接命名管道，发 hello，引擎回 hello，日志两侧可见。
- myabc-deployer.exe --unregister 后列表恢复，注册表残留清零。

## 2. 现状分析

- 无 src/ 代码。
- docs/architecture/system-overview.md 已定义领域划分、IPC 协议、不变量。
- 可参考：微软 SampleIME（github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME，MIT）
  的 TSF 骨架、注册、类工厂代码，允许按 MIT 拷贝片段（保留版权头，debt-log 登记）。

## 3. 文件级改造点（全部新建）

### 3.1 src/shared/ipc-protocol/

| 文件 | 内容 |
|---|---|
| protocol.hpp | PROTOCOL_VERSION=1；enum class Method；struct Request/Response；namespace 常量（不含裸值） |
| frame.hpp/.cpp | 长度前缀帧：uint32 LE 长度 + UTF-8 JSON body；read_frame/write_frame（对 HANDLE 抽象） |
| json_codec.hpp/.cpp | Request/Response 与 JSON 互转；依赖 header-only JSON（vendored 到 third_party/json/） |
| CMakeLists.txt | add_library(myabc-ipc STATIC ...)；两套工具链都能编（只用标准库） |

逻辑：M0 只实现 hello 与 processKey 两个 Method 的编解码，其余留枚举位。

### 3.2 src/config/

| 文件 | 内容 |
|---|---|
| config_defaults.hpp | struct Config { IpcConfig ipc; CandidatesConfig candidates; ... }；constexpr 默认值。M0 只需 ipc.pipe_name_template、ipc.connect_timeout_ms、engine.exe_path、langid |
| config_loader.hpp/.cpp | 从 %APPDATA%\myabc\config.toml 覆盖（M0 可仅返回默认值，留 TODO） |
| guids.hpp | extern const CLSID CLSID_MyabcTextService; extern const GUID GUID_MyabcProfile;（定义放 deployment/guids.cpp，DLL 侧也链接同一 .cpp 或复制常量到单一 translation unit） |
| CMakeLists.txt | add_library(myabc-config INTERFACE) |

用 uuidgen 生成两个新 GUID，写死在 guids.cpp，并加 DECISION 注释（这是设计常量非配置）。

### 3.3 src/domains/tsf-service/ （MSVC，产出 myabc-tip.dll）

| 文件 | 现状 | 目标 |
|---|---|---|
| backend/dll_main.cpp | 无 | DllMain（记录 hinst，禁用 thread lib calls）；DllGetClassObject（返回 CClassFactory for CLSID_MyabcTextService）；DllCanUnloadNow（依全局对象计数 g_cRefDll + g_cRefServer）；DllRegisterServer/DllUnregisterServer（写 HKCR\CLSID\{..}\InprocServer32 = dll 路径, ThreadingModel=Apartment；然后调 deployment 的 RegisterProfiles/RegisterCategories） |
| backend/class_factory.cpp/.h | 无 | CClassFactory : IClassFactory；CreateInstance 造 CMyabcTextService；LockServer 维护计数 |
| backend/myabc_text_service.cpp/.h | 无 | CMyabcTextService : ITfTextInputProcessorEx, ITfThreadMgrEventSink, ITfKeyEventSink（M0 够用）。Activate/ActivateEx：AdviseSinks，注册 KeyEventSink（ITfKeyystrokeMgr::AdviseKeyEventSink）。Deactivate：对称 Unadvise。持有 ITfThreadMgr、client id |
| backend/key_event_sink.cpp/.h | 无 | OnSetFocus/OnTestKeyDown/OnKeyDown/OnTestKeyUp/OnKeyUp/OnPreservedKey。M0：OnTestKeyDown 对 VK 'A' 返回 TRUE（eaten），其它 FALSE；OnKeyDown 对 'A' 发起 CEditSession 插入 U+554A，返回 TRUE |
| backend/edit_session.cpp/.h | 无 | CEditSession : ITfEditSession；DoEditSession：GetSelection -> SetText 插入给定字符串 -> 收拢 selection。经 ITfContext::RequestEditSession(TF_ES_ASYNCDONTCARE \| TF_ES_READWRITE) |
| backend/ipc_client.cpp/.h | 无 | NamedPipeTransport：Connect（CreateFile 管道名，超时内重试，失败则 CreateProcess engine.exe_path 再重试）；SendRequest/RecvResponse（overlapped）。M0：Activate 时连一次，发 hello，日志打印响应；不接入按键路径 |
| logic/composition_state.cpp/.h | 无 | 骨架：enum State { Idle, Composing }；M0 只有 Idle。预留 Feed(key) 接口 |
| logic/key_router.cpp/.h | 无 | M0：IsInterestedKey(vk) -> vk==='A'。预留：组字态下的字母/数字/翻页键判定 |
| ui/lang_bar_button.cpp/.h | 无 | 可选，M0 可跳过（留 TODO），或最简 ITfLangBarItemButton 显示"中" |
| tsf-service.def | 无 | 导出 DllGetClassObject/DllCanUnloadNow/DllRegisterServer/DllUnregisterServer |
| CMakeLists.txt | 无 | add_library(myabc-tip SHARED ...)；链接 myabc-ipc、myabc-config、wil、msctf/ole32/oleaut32/uuid；仅 MSVC |

不变量校验：本目录不得 include glib/pinyin/db 或 input-engine/backend。

### 3.4 src/domains/deployment/ （MSVC，产出 myabc-deployer.exe）

| 文件 | 目标 |
|---|---|
| backend/guids.cpp | 定义 CLSID_MyabcTextService、GUID_MyabcProfile（唯一 translation unit） |
| backend/tsf_register.cpp/.h | RegisterProfiles：ITfInputProcessorProfiles::Register(clsid) + AddLanguageProfile(clsid, MAKELANGID(zh,PRC)=0x0804, GUID_MyabcProfile, L"智能ABC (myabc)", -1, iconPath, idx)。UnregisterProfiles 对称。RegisterCategories：ITfCategoryMgr::RegisterCategory 对 GUID_TFCAT_TIP_KEYBOARD 及 Win8 TIPCAP 集合（SECUREMODE、UIELEMENTENABLED、COMLESS、INPUTMODECOMPARTMENT、IMMERSIVESUPPORT、SYSTRAYSUPPORT）。UnregisterCategories 对称 |
| backend/com_register.cpp/.h | 写/删 HKCR\CLSID\{clsid}\InprocServer32（指向已安装的 myabc-tip.dll，双架构分别处理 WOW6432Node），ThreadingModel=Apartment |
| logic/deploy_flow.cpp/.h | Register()/Unregister()/Status()：编排上面步骤，校验（重新枚举 profile 确认存在），打印结果 |
| ui/main.cpp | 解析 --register / --unregister / --status；CoInitializeEx(APARTMENTTHREADED) |
| CMakeLists.txt | add_executable(myabc-deployer ...)；x86+x64 |

参考 SampleIME 的 RegisterProfiles / RegisterCategories / RegisterServer 逐函数对照移植。

### 3.5 src/domains/input-engine/ （MinGW，产出 myabc-engine.exe）—— M0 只做骨架

| 文件 | 目标（M0） |
|---|---|
| backend/pipe_server.cpp/.h | 命名管道服务端：CreateNamedPipe(pipe_name)，Accept，对每连接起线程；read_frame/write_frame（复用 myabc-ipc） |
| backend/single_instance.cpp/.h | CreateMutex(Local\myabc-engine-<sid>)；已存在则退出 |
| logic/dispatcher.cpp/.h | 收 Request：hello -> 回 {engineVersion, protocol}；processKey -> M0 回 {handled:false}（占位）；shutdown -> 优雅退出 |
| logic/engine_main.cpp | main：解析 --selftest（M0 打印 "selftest: libpinyin not wired yet, exit 0"，M1 再实现真逻辑）、正常模式进 pipe_server 循环 |
| backend/libpinyin_wrapper.* | M0 建空文件 + TODO（M1 填） |
| CMakeLists.txt | add_executable(myabc-engine ...)；链接 myabc-ipc；MinGW only；-static-libgcc -static-libstdc++ |

### 3.6 assets/

- assets/icons/myabc.ico（语言栏/profile 图标，16/20/24/32 多尺寸）
- assets/config/config.sample.toml（配置样例，注释每项）

## 4. 不改动清单

- 不接入 libpinyin（M1 才做）。
- 按键路径 M0 只处理 'A' -> 上屏"啊"；不要顺手实现拼音缓冲、候选窗、模式切换。
- IPC 客户端 M0 不进按键热路径，只在 Activate 时做一次 hello 往返验证。
- 不实现 langbar 完整菜单、不实现 DisplayAttribute（下划线）——M1/M2。
- 不写安装包；注册仅经 deployer 或 regsvr32。

## 5. 可执行验收判据

| # | 步骤 | 期望输出 |
|---|---|---|
| M0-1 | scripts/build.ps1 -Arch x64,x86 | build/msvc/* 下生成 myabc-tip.dll（x64、x86）、myabc-deployer.exe；退出 0 |
| M0-2 | scripts/build-engine.sh | build/mingw/engine/myabc-engine.exe；退出 0 |
| M0-3 | scripts/stage.ps1 -Config RelWithDebInfo | out/RelWithDebInfo/ 下 tip dll(双架构)、deployer、engine + 依赖 DLL 齐全 |
| M0-4 | myabc-engine.exe --selftest | 打印 selftest 行，退出码 0 |
| M0-5 | out\...\myabc-deployer.exe --register 然后 --status | status 打印 profile 已注册、CLSID 已注册；退出 0 |
| M0-6 | 打开 notepad.exe，看输入法切换菜单（Win+Space 或托盘） | 列表含"智能ABC (myabc)" |
| M0-7 | 选中 myabc，在记事本按 a | 出现"啊"；按 b 出现 b（透传）；按数字/方向键正常 |
| M0-8 | 查看引擎日志（%APPDATA%\myabc\logs\engine.log） | 含一条 hello 请求 + 响应记录（来自 TIP 的 Activate） |
| M0-9 | myabc-deployer.exe --unregister 然后 --status | status 打印未注册；reg query "HKCU\SOFTWARE\Microsoft\CTF\TIP\{clsid}" 无残留 |
| M0-10 | 重开记事本 | 列表不再有 myabc |
| M0-11 | tests/review/run_all 中的 structure/isolation/hardcode 检查 | 全绿（见 04 review-standards 与 tests/review 说明） |

回归判据（历史真实坑，纳入 tests/review 或手测清单）：
- 崩溃/卸载不彻底：--unregister 后 HKCR\CLSID\{clsid} 与 CTF Assemblies 项清零（Weasel/PIME 常见坑）。
- 64/32 位：x86 记事本（C:\Windows\SysWOW64\notepad.exe 若有，或 32 位程序）也能加载 x86 tip dll。

## 6. 风险与回滚

- 风险：TIP DLL 注册后若崩溃，可能影响所有输入焦点。缓解：M0 逻辑极简、异常全 catch、
  按键异常时 return S_OK 放行原键。
- 风险：AppContainer 下 hello 往返失败（管道 ACL）。M0 不阻塞——hello 失败只记日志，不影响上屏"啊"。
- 回滚：myabc-deployer.exe --unregister；删除 out/；git checkout 到上一个 tag。
- 每个子步骤（3.1-3.6）可独立提交，便于二分定位。
