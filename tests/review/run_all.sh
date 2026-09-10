#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# run_all.sh --- 复核门禁汇总入口（M0 种子版）
#
# 依据：docs/review-standards/code-review-checklist.md §D（run_all）
#       ~/.gemini/rules/project-structure.md §3.2（合并前强制门禁）
#
# 依次跑 tests/review/ 下所有 check_*.sh；任一非零则本脚本非零退出。
# M0：仅 check_structure.sh + check_hardcode.sh。
# 后续补齐 check_spdx / check_tip_isolation / check_decision_comments /
#          check_protocol_version / check_dll_deps，以及 ctest(tests/unit)。

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

CHECKS=(
  "check_structure.sh"
  "check_hardcode.sh"
)

rc=0
for c in "${CHECKS[@]}"; do
  script="$HERE/$c"
  if [ ! -f "$script" ]; then
    echo "SKIP: $c 不存在"
    continue
  fi
  echo "=== 运行 $c ==="
  if bash "$script"; then
    echo "--- $c OK"
  else
    echo "--- $c FAILED"
    rc=1
  fi
  echo
done

# TODO(M0+): ctest --test-dir build/mingw/engine 等单测接入

if [ $rc -eq 0 ]; then
  echo "run_all: 全部复核门禁通过 —— GREEN"
else
  echo "run_all: 存在失败项 —— RED"
fi
exit $rc
