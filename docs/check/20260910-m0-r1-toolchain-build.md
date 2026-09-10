# M0 R1 质检报告 · 工具链与 libpinyin 构建验收（plan 00 B1–B5）

- 日期：2026-09-10
- 里程碑：M0 之前的地基（docs/plan/00-toolchain-and-build-plan.md §8）
- 对照计划：docs/plan/00-toolchain-and-build-plan.md
- 执行方：主会话（Sonnet），非独立 qa-reviewer 轮（属 §5.1 机械执行 + 门禁复现）
- 结论：**B1–B5 全部通过**。Plan A（MSYS2 UCRT64 构建 libpinyin）成立，未触发 Plan B。

## 1. 验收判据逐条结果

| # | 判据 | 结果 | 证据 |
|---|---|---|---|
| B1 | VS 自带 cmake >= 3.25 | PASS | MSVC 侧 bundled cmake 3.31.6-msvc6；UCRT64 cmake 4.4.3 |
| B2 | UCRT64 `pkg-config --modversion glib-2.0` | PASS | 输出 `2.88.3`，退出 0；Berkeley DB 6.2 亦就位 |
| B3 | UCRT64 Release 构建 libpinyin 共享库 | PASS（打 2 处补丁） | `build/src/libpinyin.dll`(851 KB) + `libpinyin.dll.a`(330 KB)；`cmake --build` 退出 0 |
| B4 | `ldd libpinyin.dll` 依赖均在 `/ucrt64/bin` | PASS | 非系统 DLL：libgcc_s_seh-1 / libstdc++-6 / libwinpthread-1 / libglib-2.0-0 / libintl-8 / libiconv-2 / libdb-6.2 / libpcre2-8-0，全部解析到 `/ucrt64/bin` |
| B5 | `cmake --preset x64-release` 拉起 wil、配置无错 | PASS | vcpkg 安装 `wil 1.0.260126.7`；`find_package(wil CONFIG REQUIRED)` 通过；`Configuring done` 无 error |
| 附 | `tests/review/run_all.sh` | GREEN | structure / hardcode 检查通过（src/ 尚空） |

## 2. 计划偏差与补充动作（均已登记 _debt-log）

1. **libpinyin 需打补丁**（计划 §3.4 已预判"可能需要补丁"，§7 要求存 patches/*.patch）。
   - `patches/libpinyin-win32-shared-build.patch`：
     - `CMakeLists.txt if(WIN32)`：上游把库前缀强设为 `CMAKE_INSTALL_PREFIX`（Windows 默认
       `C:/Program Files/libpinyin`，含盘符冒号+空格）→ Ninja 输出路径 `src/C:/Program Files/...`
       非法报错。改为保留默认前缀 + `CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON`。
     - `src/CMakeLists.txt`：PE DLL 必须链接期解析 `db_create`，上游仅把 DB 库加到
       `CMAKE_CXX_LINK_EXECUTABLE`（且 `src/storage/CMakeLists.txt` 变量名拼错为
       `BERKELEY_DB_LIBRARIES`）。补丁在 WIN32 下对 `pinyin` target 显式链接 glib + DB。
   - 构建参数：`-DBUILD_UTILS=OFF -DBUILD_TESTING=OFF`（utils 需 Python + 联网下载 model.text，
     推迟到 M1 数据步骤）。**影响**：本轮未产出词库二进制，B3 的 selftest（§3.5，M0-4）留待引擎骨架落地。
2. **PowerShell 脚本编码**：`scripts/env.ps1` / `setup-deps.ps1` 无 BOM，PS 5.1 GBK 误解码
   中文注释导致解析失败。已加 UTF-8 BOM。约定：含非 ASCII 的 `.ps1` 一律 UTF-8 with BOM。
3. **顶层构建骨架**（plan 00 §4，本轮补齐）：新增 `CMakeLists.txt` / `CMakePresets.json` /
   `cmake/myabc-common.cmake`。`CMakePresets.json` 根对象不接受 `$comment`，改用 `vendor.myabc`。

## 3. 未覆盖 / 遗留（进入 M0 R2）

- plan 00 §5 脚本：`scripts/build.ps1`、`build-engine.sh`、`stage.ps1`、`register.ps1` 未实现。
- plan 00 §3.5 引擎 selftest（把 `nihao` 转出"你好"）：依赖词库二进制 + 引擎骨架，M1 兑现。
- ~~`third_party/libpinyin` 追踪与补丁应用方式待定~~ **已在 R1 收口**：按 project 决策保持
  普通 clone，`third_party/libpinyin/` 加入 `.gitignore`；`scripts/setup-deps.ps1` 增加幂等的
  patch 应用步骤（checkout 前 `git apply --reverse`，checkout 后 `git apply`）。
- `vcpkg_installed/` 已在 .gitignore；`build/` 同。

## 4. 复现命令

```powershell
# 依赖（幂等）
powershell -ExecutionPolicy Bypass -File scripts\setup-deps.ps1 -SkipMsys2Update

# B3/B4：UCRT64 构建 libpinyin
& C:\msys64\usr\bin\bash.exe -lc "MSYSTEM=UCRT64 CHERE_INVOKING=1 bash -lc 'cd /e/work/myabc/third_party/libpinyin && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DBUILD_UTILS=OFF && cmake --build build -j'"

# B5：MSVC 侧配置（在 vcvars x64 环境）
. scripts\env.ps1
cmd /c "`"$MyabcVcvarsall`" x64 && `"$MyabcCMake`" --preset x64-release"
```
