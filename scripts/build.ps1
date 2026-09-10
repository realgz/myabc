#Requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-or-later
#
# scripts/build.ps1 --- 构建 MSVC 侧产物（TIP DLL / 候选窗 / 部署工具）
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §4.1 / §5
#       docs/plan/01-m0-tsf-skeleton-plan.md §5（M0-1）
#
# 每个架构在对应的 vcvarsall.bat 环境里独立 configure + build，产物落 build/msvc/<preset>/。
# 用法：
#   powershell -ExecutionPolicy Bypass -File scripts\build.ps1                 # x64 RelWithDebInfo
#   powershell ... -File scripts\build.ps1 -Arch x64,x86 -Config Release
#   powershell ... -File scripts\build.ps1 -Arch x64 -Config Debug -Target myabc-ipc

[CmdletBinding()]
param(
    # 逗号分隔，如 -Arch x64,x86；-File 模式下也接受单串 "x64,x86"。
    [string]$Arch = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Release',

    [string]$Target,

    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1"

$env:VCPKG_ROOT = $MyabcVcpkgRoot

$archList = @($Arch -split '[,\s]+' | Where-Object { $_ })
foreach ($a in $archList) {
    if ($a -notin @('x64', 'x86')) { throw "未知架构 '$a'（仅 x64 | x86）。" }
}

# Config -> preset 名（x86 只有 release 预设；Debug 仅 x64）。
function Resolve-Preset([string]$arch, [string]$config) {
    if ($arch -eq 'x86') {
        if ($config -eq 'Debug') { throw "x86 无 Debug 预设（见 CMakePresets.json）。用 -Config Release。" }
        return 'x86-release'
    }
    if ($config -eq 'Debug') { return 'x64-debug' }
    return 'x64-release'
}

# vcvarsall 架构参数
function Resolve-VcArch([string]$arch) {
    if ($arch -eq 'x86') { return 'x86' } else { return 'x64' }
}

foreach ($a in $archList) {
    $preset = Resolve-Preset $a $Config
    $vcArch = Resolve-VcArch $a
    $binDir = Join-Path $MyabcRepoRoot "build\msvc\$preset"

    if ($Clean -and (Test-Path $binDir)) {
        Write-Host "清理 $binDir" -ForegroundColor DarkYellow
        Remove-Item -Recurse -Force $binDir
    }

    Write-Host "`n=== [$a / $Config] preset=$preset ===" -ForegroundColor Cyan

    $cmakeArgs = "--preset $preset"
    $buildArgs = "--build --preset $preset"
    if ($Target) { $buildArgs += " --target $Target" }

    # 在一个 cmd 会话里：初始化 vcvars -> configure -> build。
    # 必须在仓库根执行（CMake --preset 从 cwd 找 CMakePresets.json）。
    # 注：vcvarsall 内部对 vswhere 的告警走 stderr、不影响初始化，忽略即可。
    $line = "call `"$MyabcVcvarsall`" $vcArch && `"$MyabcCMake`" $cmakeArgs && `"$MyabcCMake`" $buildArgs"
    Push-Location $MyabcRepoRoot
    try {
        & cmd /c $line
        $rc = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($rc -ne 0) { throw "构建失败（$a / $Config，退出码 $rc）。" }
}

Write-Host "`nMSVC 侧构建完成。产物在 build\msvc\<preset>\。" -ForegroundColor Green
