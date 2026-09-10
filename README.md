# myabc —— 智能ABC输入法（Windows 10/11）

一个基于 **TSF（Text Services Framework）** 的现代中文智能拼音输入法，目标是在 Windows 10/11
（桌面应用、浏览器、UWP/商店应用均可用）下提供接近老"智能ABC"体验的整句智能拼音输入。

- 引擎核心：集成 [libpinyin](https://github.com/libpinyin/libpinyin)（统计语言模型，智能整句转换）
- 架构：薄 TSF DLL（宿主进程内） + 独立引擎进程（词库/语言模型） + 候选窗 UI，进程间用 IPC
- 语言/构建：C++ / CMake / MSVC v143（Visual Studio 2022）

## 启动方式

（待补充：见 `docs/plan/` 实施计划与 `docs/architecture/` 架构设计）

## 文档

全部文档以 [`docs/INDEX.md`](./docs/INDEX.md) 为总入口。
