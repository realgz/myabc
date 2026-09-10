#Requires -Version 5.1
# SPDX-License-Identifier: GPL-3.0-or-later
#
# scripts/stage.ps1 --- 汇总两侧产物 + MinGW 运行时 DLL 到 out/<Config>/
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §2 / §5
#       docs/plan/01-m0-tsf-skeleton-plan.md §5（M0-3）
#
# 前置：scripts\build.ps1（MSVC 侧）与 scripts\build-engine.sh（引擎侧）已产出。
# 用法：powershell -ExecutionPolicy Bypass -File scripts\stage.ps1 -Config RelWithDebInfo

[CmdletBinding()]
param(
    [string]$Config = 'RelWithDebInfo',
    [string[]]$Arch = @('x64', 'x86')   # -Arch x64,x86
)

$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\env.ps1"

# 归一：把可能的 "x64,x86" 单串拆开，去空。
$Arch = @($Arch | ForEach-Object { $_ -split '[,\s]+' } | Where-Object { $_ })
foreach ($a in $Arch) {
    if (@('x64', 'x86') -notcontains $a) { throw "未知架构 '$a'（仅 x64 | x86）。" }
}

$outDir = Join-Path $MyabcRepoRoot "out\$Config"
$engineOut = Join-Path $outDir 'engine'
New-Item -ItemType Directory -Force -Path $outDir, $engineOut | Out-Null

function Copy-IfExists([string]$src, [string]$dst) {
    if (Test-Path $src) {
        Copy-Item $src $dst -Force
        Write-Host "  + $([System.IO.Path]::GetFileName($src))"
    } else {
        Write-Warning "  缺产物：$src"
    }
}

# --- MSVC 侧：TIP DLL（按架构分子目录）、deployer ------------------------
foreach ($a in $Arch) {
    $preset = if ($a -eq 'x86') { 'x86-release' } elseif ($Config -eq 'Debug') { 'x64-debug' } else { 'x64-release' }
    $msvcBin = Join-Path $MyabcRepoRoot "build\msvc\$preset\bin"
    $archDir = Join-Path $outDir $a
    New-Item -ItemType Directory -Force -Path $archDir | Out-Null

    Write-Host "[$a] <- build\msvc\$preset" -ForegroundColor Cyan
    Copy-IfExists (Join-Path $msvcBin 'myabc-tip.dll') $archDir
    Copy-IfExists (Join-Path $msvcBin 'myabc-tip.pdb') $archDir
    Copy-IfExists (Join-Path $msvcBin 'myabc-deployer.exe') $archDir
}

# --- 引擎侧：myabc-engine.exe + libpinyin.dll + MinGW 运行时 -------------
$mingwBin = Join-Path $MyabcRepoRoot 'build\mingw\engine\bin'
$lpBin = Join-Path $MyabcLibpinyinDir 'build\src'
Write-Host "[engine] <- build\mingw\engine + libpinyin" -ForegroundColor Cyan
Copy-IfExists (Join-Path $mingwBin 'myabc-engine.exe') $engineOut
Copy-IfExists (Join-Path $lpBin 'libpinyin.dll') $engineOut

# 用 ldd 收集 libpinyin.dll 依赖的 /ucrt64/bin DLL（排除系统 DLL）。
if (Test-Path (Join-Path $lpBin 'libpinyin.dll')) {
    $env:MSYSTEM = 'UCRT64'
    $lddOut = & $MyabcMsysBash -lc "ldd '$((Join-Path $lpBin 'libpinyin.dll') -replace '\\','/')'"
    foreach ($ln in $lddOut) {
        if ($ln -match '=>\s+(/ucrt64/\S+\.dll)') {
            $p = $Matches[1] -replace '^/ucrt64', ($MyabcUcrt64Bin -replace '\\bin$', '') -replace '/', '\'
            Copy-IfExists $p $engineOut
        }
    }
}

# --- 资源 + 配置样例 --------------------------------------------------
Copy-IfExists (Join-Path $MyabcRepoRoot 'assets\config\config.sample.toml') $outDir

Write-Host "`n已汇总到 $outDir" -ForegroundColor Green
Get-ChildItem -Recurse $outDir | Select-Object -ExpandProperty FullName |
    ForEach-Object { $_.Substring($outDir.Length + 1) } | Sort-Object
