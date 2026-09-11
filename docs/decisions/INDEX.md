# decisions 索引

| 领域目录 | 主题 |
|---|---|
| [ime-framework/](./ime-framework/) | 框架层选型（TSF） |
| [pinyin-engine/](./pinyin-engine/) | 拼音引擎选型（libpinyin） |
| [project-setup/](./project-setup/) | 语言/构建/工具链 |
| [input-engine/](./input-engine/) | 引擎侧交互行为（候选选择/确认逻辑等） |

## ADR 清单

| 文件 | 主题 | 状态 | 日期 |
|---|---|---|---|
| [ime-framework/20260909-adopt-tsf-over-imm32.md](./ime-framework/20260909-adopt-tsf-over-imm32.md) | 采用 TSF，不用 IMM32 | 生效中 | 2026-09-09 |
| [pinyin-engine/20260909-adopt-libpinyin-mingw-engine-process.md](./pinyin-engine/20260909-adopt-libpinyin-mingw-engine-process.md) | 集成 libpinyin，构建为独立 MinGW 引擎进程 | 生效中 | 2026-09-09 |
| [project-setup/20260909-cpp-cmake-split-toolchain.md](./project-setup/20260909-cpp-cmake-split-toolchain.md) | C++/CMake，MSVC 与 MinGW 拆分工具链 | 生效中 | 2026-09-09 |
| [input-engine/20260911-space-key-two-step-confirm.md](./input-engine/20260911-space-key-two-step-confirm.md) | 空格键智能ABC 风格两段式确认（用户明确的核心目标功能） | 生效中 | 2026-09-11 |

未及时留痕登记表：[_debt-log.md](./_debt-log.md)
