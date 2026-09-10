# SPDX-License-Identifier: GPL-3.0-or-later
#
# scripts/env.ps1 --- myabc 构建环境定位与变量导出（单点配置，反硬编码落点）
#
# 用法：其它脚本用 dot-source 载入本文件，不要复制其中的路径：
#     . "$PSScriptRoot\env.ps1"
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §3.1 / §5
#       docs/review-standards/code-review-checklist.md A2（反硬编码）
#
# 所有"换机器 / 换环境就要改"的路径集中在此，别散落到 build.ps1 / stage.ps1 等。
# 载入后可用变量：
#   $MyabcRepoRoot   仓库根目录
#   $MyabcVsPath     Visual Studio 安装根
#   $MyabcCMake      cmake.exe 绝对路径（VS 自带，或回退安装的）
#   $MyabcNinja      ninja.exe 绝对路径
#   $MyabcVcvarsall  vcvarsall.bat 绝对路径（MSVC 环境初始化）
#   $MyabcMsys2Root  MSYS2 安装根（默认 C:\msys64）
#   $MyabcMsysBash   MSYS2 usr\bin\bash.exe（登录 shell 用 -lc）
#   $MyabcUcrt64Bin  MSYS2 ucrt64\bin
#   $MyabcVcpkgRoot  third_party/vcpkg
#   $MyabcVcpkgExe   vcpkg.exe（bootstrap 后存在）
#   $MyabcLibpinyinDir third_party/libpinyin
#
# 约定：找不到必需工具时抛异常（fail fast），不静默继续。

$ErrorActionPreference = 'Stop'

# --- 仓库根 -------------------------------------------------------------------
$MyabcRepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

# --- 可覆盖的默认值（环境变量优先，便于 CI / 非标准安装）---------------------
$MyabcMsys2Root = if ($env:MYABC_MSYS2_ROOT) { $env:MYABC_MSYS2_ROOT } else { 'C:\msys64' }

# --- Visual Studio + CMake + Ninja ------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe 未找到（$vswhere）。请安装 Visual Studio 2022（含 C++ 工作负载）。"
}

$MyabcVsPath = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath) | Select-Object -First 1
if (-not $MyabcVsPath) {
    # 放宽：任意最新 VS 实例
    $MyabcVsPath = (& $vswhere -latest -property installationPath) | Select-Object -First 1
}
if (-not $MyabcVsPath) { throw "未定位到 Visual Studio 安装（vswhere -latest 无结果）。" }

$MyabcVcvarsall = Join-Path $MyabcVsPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path $MyabcVcvarsall)) {
    throw "vcvarsall.bat 未找到（$MyabcVcvarsall）。VS 缺少 C++ 生成工具工作负载。"
}

# CMake：优先 VS 自带；其次 PATH 上的（如 winget 装的 Kitware.CMake）。要求 >= 3.25
$vsCMake = Join-Path $MyabcVsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$vsNinja = Join-Path $MyabcVsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'

function Get-CMakeVersion([string]$exe) {
    try {
        $line = (& $exe --version 2>$null | Select-Object -First 1)
        if ($line -match '(\d+)\.(\d+)\.(\d+)') {
            return [version]("{0}.{1}.{2}" -f $Matches[1], $Matches[2], $Matches[3])
        }
    } catch { }
    return $null
}

$minCMake = [version]'3.25.0'
$MyabcCMake = $null
$MyabcNinja = $null

if ((Test-Path $vsCMake) -and ((Get-CMakeVersion $vsCMake) -ge $minCMake)) {
    $MyabcCMake = $vsCMake
    if (Test-Path $vsNinja) { $MyabcNinja = $vsNinja }
}

if (-not $MyabcCMake) {
    # 回退：PATH 上的 cmake（例如 winget install Kitware.CMake 之后）
    $pathCMake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
    if ($pathCMake -and ((Get-CMakeVersion $pathCMake) -ge $minCMake)) {
        $MyabcCMake = $pathCMake
        Write-Warning "使用 PATH 上的 cmake（$pathCMake），VS 自带 cmake 不可用或版本过低。"
    }
}

if (-not $MyabcCMake) {
    throw "未找到 >= $minCMake 的 cmake。请执行：winget install Kitware.CMake --accept-package-agreements --accept-source-agreements，然后重开终端。"
}

if (-not $MyabcNinja) {
    $pathNinja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
    if ($pathNinja) { $MyabcNinja = $pathNinja }
    elseif (Test-Path $vsNinja) { $MyabcNinja = $vsNinja }
    else { throw "未找到 ninja。VS 的 C++ CMake 组件应自带；或 winget install Ninja-build.Ninja。" }
}

# --- MSYS2 / UCRT64 --------------------------------------------------------
$MyabcMsysBash  = Join-Path $MyabcMsys2Root 'usr\bin\bash.exe'
$MyabcUcrt64Bin = Join-Path $MyabcMsys2Root 'ucrt64\bin'
# 注意：首次运行 setup-deps.ps1 之前 MSYS2 可能尚未安装，这里不 throw，只警告。
if (-not (Test-Path $MyabcMsysBash)) {
    Write-Warning "MSYS2 未安装（缺 $MyabcMsysBash）。运行 scripts\setup-deps.ps1 安装。"
}

# --- vcpkg ---------------------------------------------------------------
$MyabcVcpkgRoot = Join-Path $MyabcRepoRoot 'third_party\vcpkg'
$MyabcVcpkgExe  = Join-Path $MyabcVcpkgRoot 'vcpkg.exe'

# --- libpinyin ---------------------------------------------------------
$MyabcLibpinyinDir = Join-Path $MyabcRepoRoot 'third_party\libpinyin'

# --- 便捷函数：在 MSYS2 UCRT64 环境跑一条 bash 命令 ------------------------
# 用法： Invoke-Ucrt64 'cd /e/work/myabc/third_party/libpinyin && cmake --build build -j'
function Invoke-Ucrt64([string]$Command) {
    if (-not (Test-Path $MyabcMsysBash)) { throw "MSYS2 未安装，无法执行 UCRT64 命令。" }
    $env:MSYSTEM = 'UCRT64'
    $env:CHERE_INVOKING = '1'
    & $MyabcMsysBash -lc $Command
    return $LASTEXITCODE
}

if ($MyInvocation.InvocationName -ne '.') {
    # 被直接执行（非 dot-source）时打印一份摘要，方便人工核对
    Write-Output "MyabcRepoRoot   = $MyabcRepoRoot"
    Write-Output "MyabcVsPath     = $MyabcVsPath"
    Write-Output "MyabcCMake      = $MyabcCMake  ($(Get-CMakeVersion $MyabcCMake))"
    Write-Output "MyabcNinja      = $MyabcNinja"
    Write-Output "MyabcVcvarsall  = $MyabcVcvarsall"
    Write-Output "MyabcMsys2Root  = $MyabcMsys2Root  (installed=$(Test-Path $MyabcMsysBash))"
    Write-Output "MyabcUcrt64Bin  = $MyabcUcrt64Bin"
    Write-Output "MyabcVcpkgRoot  = $MyabcVcpkgRoot  (bootstrapped=$(Test-Path $MyabcVcpkgExe))"
    Write-Output "MyabcLibpinyinDir = $MyabcLibpinyinDir"
}
