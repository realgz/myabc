# tests/review —— 复核门禁（可执行）

把 docs/review-standards/code-review-checklist.md §D 的可机器判定项固化为脚本。
合并前必须 run_all 全绿（对接 AGENTS.md §3）。

## 脚本清单（待实现，随 M0 起逐步补齐）

| 脚本 | 依据 | M 几起启用 |
|---|---|---|
| check_structure.py | checklist A4 | M0 |
| check_tip_isolation.py | checklist B1 | M0 |
| check_hardcode.py | checklist A2 | M0 |
| check_decision_comments.py | checklist A1 | M0 |
| check_spdx.py | checklist A6 | M0 |
| check_protocol_version.py | checklist B5 | M1 |
| check_dll_deps.py | checklist B1 | M2 |
| run_all.ps1 / run_all.sh | 汇总 + ctest | M0 |

检查器复用的库代码放 src/review/，头部注释指回 code-review-checklist.md 的对应条目。
