# SPDX-License-Identifier: GPL-3.0-or-later
#
# scripts/setup-deps.ps1 --- 从干净机器拉起 myabc 全部构建依赖（幂等，可重复执行）
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §3（依赖获取与验证）+ §5
#
# 执行内容：
#   1. dot-source env.ps1，校验 VS / CMake / Ninja（B1）
#   2. git submodule 初始化 + vcpkg bootstrap（B5）
#   3. winget 安装 MSYS2（若未装）
#   4. pacman -Syu（两遍）+ 安装 UCRT64 toolchain/cmake/ninja/pkgconf/glib2/db（B2）
#   5. clone + 锁定 libpinyin 版本（B3 的准备）
#   6. 校验 pkg-config 能找到 glib-2.0
#
# 实际构建 libpinyin 交给 scripts/build-engine.sh（在 UCRT64 环境跑）。
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File scripts\setup-deps.ps1
#   powershell ... -File scripts\setup-deps.ps1 -SkipMsys2Update   # 跳过耗时的 -Syu

[CmdletBinding()]
param(
    [switch]$SkipMsys2Update,
    [string]$LibpinyinRef = '074a2219c90feaf962d0d24f034514033ece5f99'  # DECISION: docs/decisions/_debt-log.md（M0 R1 libpinyin 版本锁定）
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1"

function Step($msg) { Write-Host "`n=== $msg ===" -ForegroundColor Cyan }

# --- 1. 工具链校验（env.ps1 载入即校验 CMake/Ninja/VS，会 throw）------------
Step "B1: VS / CMake / Ninja"
Write-Host "CMake : $MyabcCMake"
Write-Host "Ninja : $MyabcNinja"
& $MyabcCMake --version | Select-Object -First 1

# --- 2. vcpkg submodule + bootstrap（B5）----------------------------------
Step "B5: vcpkg submodule + bootstrap"
Push-Location $MyabcRepoRoot
try {
    git submodule update --init --recursive third_party/vcpkg
    if (-not (Test-Path $MyabcVcpkgExe)) {
        & (Join-Path $MyabcVcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    } else {
        Write-Host "vcpkg.exe 已存在，跳过 bootstrap。"
    }
    if (-not (Test-Path (Join-Path $MyabcRepoRoot 'vcpkg.json'))) {
        throw "仓库根缺 vcpkg.json"
    }
    & $MyabcVcpkgExe install --dry-run --x-manifest-root=$MyabcRepoRoot --triplet=x64-windows | Select-Object -Last 6
} finally { Pop-Location }

# --- 3. MSYS2（winget）--------------------------------------------------
Step "B2a: MSYS2"
if (Test-Path $MyabcMsysBash) {
    Write-Host "MSYS2 已安装于 $MyabcMsys2Root，跳过 winget。"
} else {
    winget install --id MSYS2.MSYS2 --accept-package-agreements --accept-source-agreements --disable-interactivity --silent
    if (-not (Test-Path $MyabcMsysBash)) { throw "MSYS2 安装后仍未找到 $MyabcMsysBash" }
}

# --- 4. pacman 更新 + UCRT64 依赖（B2）--------------------------------
Step "B2b: pacman -Syu + UCRT64 依赖"
if ($SkipMsys2Update) {
    Write-Host "跳过 -Syu（-SkipMsys2Update）。"
} else {
    # 跑两遍：第一遍可能升级 msys2-runtime 并要求关闭 shell；每次是全新进程，安全。
    & $MyabcMsysBash -lc "pacman -Syu --noconfirm"
    & $MyabcMsysBash -lc "pacman -Syu --noconfirm"
}
$pkgs = @(
    'mingw-w64-ucrt-x86_64-toolchain',
    'mingw-w64-ucrt-x86_64-cmake',
    'mingw-w64-ucrt-x86_64-ninja',
    'mingw-w64-ucrt-x86_64-pkgconf',
    'mingw-w64-ucrt-x86_64-glib2',
    'mingw-w64-ucrt-x86_64-db'
) -join ' '
& $MyabcMsysBash -lc "pacman -S --noconfirm --needed $pkgs"

# --- 5. libpinyin clone + 版本锁定（B3 准备）--------------------------
Step "B3a: libpinyin clone + checkout $LibpinyinRef"
if (-not (Test-Path (Join-Path $MyabcLibpinyinDir '.git'))) {
    git clone https://github.com/libpinyin/libpinyin.git $MyabcLibpinyinDir
}
Push-Location $MyabcLibpinyinDir
try {
    git fetch --all --tags
    # checkout 前先撤销已应用的补丁，保证 checkout 干净、可重复
    $patch = Join-Path $MyabcRepoRoot 'patches\libpinyin-win32-shared-build.patch'
    if (Test-Path $patch) {
        git apply --reverse --check $patch 2>$null
        if ($LASTEXITCODE -eq 0) { git apply --reverse $patch }
    }
    git checkout $LibpinyinRef
    git submodule update --init
    Write-Host "libpinyin HEAD = $(git rev-parse HEAD)"

    # --- 5b. 应用 UCRT64 共享库构建补丁（DECISION: docs/decisions/_debt-log.md 2026-09-10）---
    Step "B3 准备: 应用 libpinyin-win32-shared-build.patch"
    if (-not (Test-Path $patch)) {
        throw "缺补丁文件 $patch（B3 会失败）。"
    }
    git apply --reverse --check $patch 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "补丁已在工作树中，跳过。"
    } else {
        git apply --check $patch
        if ($LASTEXITCODE -ne 0) { throw "补丁无法干净应用到 $LibpinyinRef，需更新 patches/。" }
        git apply $patch
        Write-Host "已应用 $([System.IO.Path]::GetFileName($patch))。"
    }
} finally { Pop-Location }

# --- 6. 校验 glib pkg-config（B2 验收）------------------------------
Step "B2 验收: pkg-config --modversion glib-2.0"
$pcExe = Join-Path $MyabcUcrt64Bin 'pkg-config.exe'
if (-not (Test-Path $pcExe)) { $pcExe = Join-Path $MyabcUcrt64Bin 'pkgconf.exe' }
& $pcExe --modversion glib-2.0
& $pcExe --cflags --libs glib-2.0

Write-Host "`n依赖就绪。下一步：在 UCRT64 环境执行  scripts\build-engine.sh  构建 libpinyin + 引擎。" -ForegroundColor Green
