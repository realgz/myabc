<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# third_party/json

- **内容**：nlohmann/json 单头文件 `nlohmann/json.hpp`（以 `<nlohmann/json.hpp>` 引入）
- **版本**：v3.11.3（tag `v3.11.3`，取自 `single_include/nlohmann/json.hpp`）
- **来源**：https://github.com/nlohmann/json/raw/v3.11.3/single_include/nlohmann/json.hpp
- **License**：MIT（见文件头 SPDX 标记，兼容本项目 GPL-3.0-or-later）
- **用途**：`src/shared/ipc-protocol` 的 JSON 编解码（plan 01 §3.1）。两套工具链（MSVC / MinGW）
  都直接 `#include`，仅依赖标准库，不引入构建依赖。
- **更新方式**：替换 `json.hpp` 并更新上面的版本/来源；不要本地修改该文件。
