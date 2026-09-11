# check 索引

qa-reviewer 的质检报告落此目录（对照 docs/plan/ 逐项 diff 审查 + 回归抽查）。

| 文件 | 里程碑 | 结论 | 日期 |
|---|---|---|---|
| [20260910-m0-r1-toolchain-build.md](./20260910-m0-r1-toolchain-build.md) | M0 地基 (B1–B5) | 通过（libpinyin 打 2 处补丁） | 2026-09-10 |
| [20260910-m0-r2-shared-libs-build-scripts.md](./20260910-m0-r2-shared-libs-build-scripts.md) | M0 (plan 01 §3.1/§3.2 + 构建脚本) | 通过 | 2026-09-10 |
| [20260910-m0-r3-tsf-deployer-engine.md](./20260910-m0-r3-tsf-deployer-engine.md) | M0 全部判据 (M0-1~M0-10) | **通过，M0 收官** | 2026-09-10~11 |
| [20260911-m1-r1-engine-libpinyin.md](./20260911-m1-r1-engine-libpinyin.md) | M1 引擎侧 (plan 02 §3.1-3.3, M1-1/M1-2) | 通过 | 2026-09-11 |
| [20260911-m1-r2-tsf-composition-ui.md](./20260911-m1-r2-tsf-composition-ui.md) | M1 TSF侧 (plan 02 §3.4/3.5) | 构建通过；记事本交互待用户 | 2026-09-11 |
| [20260911-m2-engine-lifecycle-thin-dll.md](./20260911-m2-engine-lifecycle-thin-dll.md) | M2 (plan 03，DLL变薄+引擎生命周期) | 核心架构端到端验证通过；记事本回归待用户 | 2026-09-11 |

## 已归档
见 [_archive/](./_archive/)
