#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# check_hardcode.sh --- 反硬编码扫描（M0 种子版）
#
# 依据：docs/review-standards/code-review-checklist.md A2
#       （由 tests/review/check_hardcode.sh 自动校验 —— 文档对应条目需回标本脚本）
#
# 扫 src/ 下业务代码里不该出现的裸值：管道名、IP、端口、langid、模型/词库绝对路径。
# 例外目录：src/config（配置本就该有字面量）、src/review（检查器自身带正则样例）。
#
# M0 现状：src/ 尚无业务源码，脚本应干净通过（exit 0）。
# 后续里程碑加入源码后，命中即非零退出，作为合并门禁。

set -u
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SRC_DIR="$REPO_ROOT/src"

# 被扫描的源码扩展名
INCLUDE_GLOBS=('*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp' '*.hxx')
# 排除目录（相对 src/）
EXCLUDE_DIRS=('config' 'review')

# 违规正则（扩展正则）。每条附带说明。
declare -a PATTERNS=(
  '\\\\\\\\\.\\\\pipe\\\\|\\\\\\\\\.\\\\pipe\\\\'   # 裸命名管道路径 \\.\pipe\
  '0x0804'                                          # 裸 langid（应来自 config）
  '([0-9]{1,3}\.){3}[0-9]{1,3}'                     # 裸 IPv4
  ':[0-9]{2,5}\b.*//|//[^ ]*:[0-9]{2,5}'            # URL 里的裸端口（粗筛）
  '[A-Za-z]:\\\\(Users|Program|msys64|work)\\\\'    # 裸 Windows 绝对路径
  'C:/(Users|msys64|Program)'                       # 裸 Windows 绝对路径（正斜杠）
)
declare -a PATTERN_DESC=(
  'bare named-pipe path (\\\\.\\pipe\\...) -- use src/config ipc.pipe_name_template'
  'bare langid 0x0804 -- use src/config langid'
  'bare IPv4 literal'
  'bare port number in URL'
  'bare Windows absolute path (backslash)'
  'bare Windows absolute path (forward slash)'
)

if [ ! -d "$SRC_DIR" ]; then
  echo "check_hardcode: src/ 不存在，跳过（视为通过）"
  exit 0
fi

# 组装 find 的排除表达式
FIND_ARGS=("$SRC_DIR")
for d in "${EXCLUDE_DIRS[@]}"; do
  FIND_ARGS+=(-not -path "$SRC_DIR/$d/*")
done
FIND_ARGS+=('(')
first=1
for g in "${INCLUDE_GLOBS[@]}"; do
  if [ $first -eq 1 ]; then FIND_ARGS+=(-name "$g"); first=0; else FIND_ARGS+=(-o -name "$g"); fi
done
FIND_ARGS+=(')')

mapfile -t FILES < <(find "${FIND_ARGS[@]}" -type f 2>/dev/null)

if [ ${#FILES[@]} -eq 0 ]; then
  echo "check_hardcode: 未发现待扫描源码（src/ 目前为空）—— PASS"
  exit 0
fi

violations=0
for i in "${!PATTERNS[@]}"; do
  pat="${PATTERNS[$i]}"
  desc="${PATTERN_DESC[$i]}"
  hits="$(grep -nEH "$pat" "${FILES[@]}" 2>/dev/null || true)"
  if [ -n "$hits" ]; then
    echo "FAIL [$desc]:"
    echo "$hits" | sed 's/^/  /'
    violations=$((violations + 1))
  fi
done

if [ $violations -gt 0 ]; then
  echo "check_hardcode: $violations 类硬编码违规 —— FAIL"
  exit 1
fi

echo "check_hardcode: 扫描 ${#FILES[@]} 个源文件，无硬编码违规 —— PASS"
exit 0
