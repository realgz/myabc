# M0 R2 质检报告 · 共享层（IPC 协议 + 配置）+ 构建脚本

- 日期：2026-09-10
- 里程碑：M0（plan 01 §3.1 + §3.2 落地；plan 00 §5 脚本补齐）
- 对照计划：docs/plan/01-m0-tsf-skeleton-plan.md、docs/plan/00-toolchain-and-build-plan.md
- 执行方：主会话（Sonnet）
- 结论：**通过**。两套工具链均能编译共享静态库并跑通单测；构建脚本 M0-1/M0-2 路径可用。

## 1. 本轮交付

| 区域 | 文件 | 说明 |
|---|---|---|
| IPC 协议 | `src/shared/ipc-protocol/{protocol,frame,json_codec}.{hpp,cpp}` + `CMakeLists.txt` | `myabc::ipc` STATIC；仅标准库 + nlohmann/json（不 include windows.h，不变量 6）。M0 实现 hello/processKey 名映射、长度前缀帧读写、Request/Response ↔ JSON 编解码、协议版本校验（不变量 7） |
| 配置 | `src/config/{config_defaults.hpp,config_loader.{hpp,cpp},guids.{hpp,cpp}}` + `CMakeLists.txt` | `myabc::config` STATIC（默认值结构体 + 占位符展开，M0 不读 TOML）；`myabc::guids` STATIC（仅 MSVC，`DEFINE_GUID` 单 TU） |
| 顶层 | `CMakeLists.txt` 增 `MYABC_BUILD_TESTS` + `tests/unit` | |
| 第三方 | `third_party/json/nlohmann/json.hpp`（v3.11.3，MIT）+ `README.md` | vendored 单头，不纳入构建依赖 |
| 构建脚本 | `scripts/build.ps1`、`scripts/build-engine.sh`、`scripts/stage.ps1` | plan 00 §5 |
| 资源 | `assets/config/config.sample.toml` | plan 01 §3.6 |
| 测试 | `tests/unit/ipc_roundtrip_test.cpp` + `CMakeLists.txt` | 无框架，assert + 非零退出，CTest 判定 |
| 工程 | `.gitattributes` | `*.sh`/`*.cmake` 强制 LF，`*.ps1` CRLF；避免 autocrlf 破坏 MSYS 脚本 |

## 2. 验收结果

| 判据 | 结果 | 证据 |
|---|---|---|
| MSVC 侧 `scripts/build.ps1 -Arch x64 -Config Release` | PASS | `myabc-ipc.lib` / `myabc-config.lib` / `myabc-guids.lib` 生成；cl /W4 /permissive- 无警告无错 |
| MinGW 侧 `scripts/build-engine.sh` | PASS | libpinyin 复用已构建；`mingw-engine` 预设产出 `libmyabc-ipc.a` / `libmyabc-config.a`；gcc -Wall -Wextra -Wpedantic 无警告 |
| `ctest`（两侧各一次） | PASS | `ipc_roundtrip` 5 组用例：方法名映射、Request/Response 往返、协议版本不匹配检测、坏 JSON 检测、帧读写 + EOF |
| `tests/review/run_all.sh` | GREEN | check_structure PASS；check_hardcode 扫描 6 个 ipc 源文件无违规（config 目录按设计豁免） |
| plan 00 §8 B1–B5 | 仍 PASS（未回归） | — |

## 3. 计划偏差 / 决策（登记 _debt-log 2026-09-10）

1. **nlohmann/json 3.11.3 vendored** 到 `third_party/json/nlohmann/json.hpp`（MIT，兼容 GPL-3.0-or-later）。
   plan 01 §3.1 已指明"header-only JSON（vendored 到 third_party/json/）"。
2. **两个设计常量 GUID 已生成**（PowerShell New-Guid，2026-09-10）：
   `CLSID_MyabcTextService {8BA238DD-B6B4-42B4-95C1-8062D25B3652}`、
   `GUID_MyabcProfile {C4FBBA94-26BC-4C7A-B9F1-4F91A723C60A}`。定义在 `src/config/guids.cpp`。
3. **PowerShell 脚本 BOM**：`build.ps1` / `stage.ps1` 同 R1 需补 UTF-8 BOM（已按 R1 约定处理）。
4. **`scripts/build.ps1` 必须在仓库根执行**：`cmake --preset` 从 cwd 找 `CMakePresets.json`；
   脚本已 `Push-Location $MyabcRepoRoot`。vcvarsall 内部对 vswhere 的 stderr 告警不影响初始化，忽略。

## 4. 未覆盖 / 遗留（进入 M0 R3）

- plan 01 §3.3 `src/domains/tsf-service`（myabc-tip.dll，TSF COM 骨架）—— R3 主体。
- plan 01 §3.4 `src/domains/deployment`（myabc-deployer.exe，profile/category/COM 注册）—— R3。
- plan 01 §3.5 `src/domains/input-engine`（myabc-engine.exe echo 骨架 + `--selftest` 占位）—— R3/R4。
- `scripts/stage.ps1` / `register.ps1`：stage.ps1 已成形但未端到端验证（缺 DLL/exe 产物）；register.ps1 未写。
- `json_codec.hpp` 直接暴露 `nlohmann::json`（编译期把 24k 行单头带入依赖它的 TU）。可接受（项目规模小）；
  若 M2 后编译时间成问题，再评估 pimpl / 只暴露 typed 字段。
