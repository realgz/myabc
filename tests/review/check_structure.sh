#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# check_structure.sh --- 项目骨架结构校验（M0 种子版）
#
# 依据：docs/review-standards/code-review-checklist.md A4
#       ~/.gemini/rules/project-structure.md §0 §1 §3
#       （文档对应条目需回标：由 tests/review/check_structure.sh 自动校验）
#
# M0 现状：只校验必备骨架目录/索引文件存在。
# 后续里程碑扩展：解析 src/domains/*/{ui,logic,backend} 的 include 依赖方向
#   （ui 不引 backend、跨域不引对方 backend），命中即 FAIL。

set -u
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT" || exit 2

fail=0
require_dir() {
  if [ ! -d "$1" ]; then echo "FAIL: 缺目录 $1"; fail=$((fail+1)); fi
}
require_file() {
  if [ ! -f "$1" ]; then echo "FAIL: 缺文件 $1"; fail=$((fail+1)); fi
}

# --- 顶层 ---
require_file README.md
require_file .gitignore
require_dir src
require_dir tests
require_dir assets
require_dir docs

# --- docs 子目录 + 各自 INDEX ---
for d in requirements architecture discussion decisions plan test reference review-standards check; do
  require_dir "docs/$d"
  require_dir "docs/$d/_archive"
done
for d in requirements architecture discussion decisions plan test reference review-standards check; do
  require_file "docs/$d/INDEX.md"
done

# --- src 骨架 ---
require_dir src/domains
require_dir src/shared
require_dir src/config
require_dir src/review

# --- tests 骨架 ---
require_dir tests/unit
require_dir tests/integration
require_dir tests/e2e
require_dir tests/review

# --- 复核双轨强制文件 ---
require_file docs/review-standards/acceptance-criteria.md
require_file docs/review-standards/code-review-checklist.md
require_file src/review/README.md
require_file tests/review/README.md

# --- .gitignore 必含项 ---
for pat in 'tmp/' '.ag_state/' 'build/' 'vcpkg_installed/'; do
  if ! grep -qF "$pat" .gitignore 2>/dev/null; then
    echo "FAIL: .gitignore 未包含 '$pat'"; fail=$((fail+1))
  fi
done

if [ $fail -gt 0 ]; then
  echo "check_structure: $fail 项不合规 —— FAIL"
  exit 1
fi
echo "check_structure: 骨架完整 —— PASS"
exit 0
