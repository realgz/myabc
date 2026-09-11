#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# scripts/build-engine.sh --- 在 MSYS2 UCRT64 环境构建引擎侧（libpinyin + myabc-engine.exe）
#
# 依据：docs/plan/00-toolchain-and-build-plan.md §3.4 / §4.1
#       docs/plan/01-m0-tsf-skeleton-plan.md §5（M0-2）
#       docs/plan/02-m1-libpinyin-quanpin-plan.md（词库数据生成，M1 R1）
#
# 运行方式（必须在 UCRT64 shell，即 C:\msys64\ucrt64.exe，或设 MSYSTEM=UCRT64）：
#   /e/work/myabc/scripts/build-engine.sh [--clean] [--libpinyin-only]
#
# 产出两套独立 build（原因见 docs/decisions/_debt-log.md 2026-09-11 "词库二进制生成
# 流水线"）：
#   third_party/libpinyin/build       —— 共享库，myabc-engine.exe 运行时链接
#   third_party/libpinyin/build-data  —— 静态库 + utils，只用来跑 data 目标生成词库

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
libpinyin_dir="${repo_root}/third_party/libpinyin"
lp_build="${libpinyin_dir}/build"

clean=0
libpinyin_only=0
for arg in "$@"; do
    case "$arg" in
        --clean) clean=1 ;;
        --libpinyin-only) libpinyin_only=1 ;;
        *) echo "未知参数: $arg" >&2; exit 2 ;;
    esac
done

if [[ "${MSYSTEM:-}" != "UCRT64" ]]; then
    echo "错误：必须在 MSYS2 UCRT64 环境运行（当前 MSYSTEM='${MSYSTEM:-未设置}'）。" >&2
    echo "打开 C:\\msys64\\ucrt64.exe 后再执行本脚本。" >&2
    exit 1
fi

command -v cmake >/dev/null || { echo "缺 cmake（pacman -S mingw-w64-ucrt-x86_64-cmake）" >&2; exit 1; }
command -v ninja >/dev/null || { echo "缺 ninja" >&2; exit 1; }
pkg-config --exists glib-2.0 || { echo "pkg-config 找不到 glib-2.0" >&2; exit 1; }

# --- 1. libpinyin（普通 clone + 补丁，见 setup-deps.ps1）--------------------
if [[ ! -d "$libpinyin_dir" ]]; then
    echo "错误：$libpinyin_dir 不存在。先在 PowerShell 跑 scripts\\setup-deps.ps1。" >&2
    exit 1
fi

if [[ "$clean" == "1" ]]; then rm -rf "$lp_build"; fi

if [[ ! -f "${lp_build}/src/libpinyin.dll" ]]; then
    echo "=== 构建 libpinyin（Release, UTILS/TESTING off）==="
    cmake -S "$libpinyin_dir" -B "$lp_build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=OFF -DBUILD_UTILS=OFF
    cmake --build "$lp_build" -j
else
    echo "libpinyin.dll 已存在，跳过（--clean 强制重建）。"
fi

ldd "${lp_build}/src/libpinyin.dll" | awk '{print $1}' | sort -u | sed 's/^/  dep: /'

# --- 1b. 词库/模型二进制数据（独立静态 build，避免与共享库的 utils 链接冲突）------
lp_build_data="${libpinyin_dir}/build-data"
if [[ "$clean" == "1" ]]; then rm -rf "$lp_build_data"; fi

if [[ ! -f "${lp_build_data}/data/table.conf" ]]; then
    echo "=== 生成 libpinyin 词库二进制（静态 build，下载 model20.text.tar.gz ~20MB）==="
    cmake -S "$libpinyin_dir" -B "$lp_build_data" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_UTILS=ON
    cmake --build "$lp_build_data" -j
else
    echo "词库数据已存在（${lp_build_data}/data/table.conf），跳过（--clean 强制重建）。"
fi

if [[ "$libpinyin_only" == "1" ]]; then
    echo "仅 libpinyin（含词库数据）完成。"
    exit 0
fi

# --- 2. myabc-engine（mingw-engine 预设）----------------------------------
echo "=== 配置 + 构建 mingw-engine 预设 ==="
cmake --preset mingw-engine -S "$repo_root"
cmake --build --preset mingw-engine -j

echo ""
echo "引擎侧构建完成。产物在 build/mingw/engine/。"
