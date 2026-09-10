# src/review —— 复核逻辑（约定即代码）

存放 tests/review/ 各检查器复用的库代码与常量：
- include/import 解析器（判定领域分层、TIP 隔离）
- 禁用 include 清单、决策承载文件 glob 清单、硬编码正则清单
- dll 依赖白名单

每个模块头部用注释指回所依据的 docs/review-standards/code-review-checklist.md 条目；
该文档对应条目也注明"由 src/review/xxx + tests/review/xxx 自动校验"。双向引用，同步变更。
