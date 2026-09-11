# myabc —— 智能ABC输入法（Windows 10/11）

一个基于 **TSF（Text Services Framework）** 的现代中文智能拼音输入法，目标是在 Windows 10/11
（桌面应用、浏览器、UWP/商店应用均可用）下提供接近老"智能ABC"体验的整句智能拼音输入。

- 引擎核心：集成 [libpinyin](https://github.com/libpinyin/libpinyin)（统计语言模型，智能整句转换）
- 架构：薄 TSF DLL（宿主进程内） + 独立引擎进程（词库/语言模型） + 候选窗 UI，进程间用 IPC
- 语言/构建：C++ / CMake / MSVC v143（Visual Studio 2022）

## 安装

从 [Releases](https://github.com/realgz/myabc/releases) 下载最新的 `myabc-setup-*.exe`
（仅 x64，需要管理员权限——注册 TSF 语言配置要写 `HKEY_CLASSES_ROOT`），运行即可。
安装完成后在语言栏切换到"智能ABC (myabc)"。卸载：控制面板/开始菜单里的卸载程序，
会自动反注册。

从源码构建：见 `docs/plan/00-toolchain-and-build-plan.md` + `scripts/*.ps1`/`*.sh`；
打包安装包：`installer/myabc.iss`（Inno Setup 6）。

## 文档

全部文档以 [`docs/INDEX.md`](./docs/INDEX.md) 为总入口。

## License

[GPL-3.0-or-later](./LICENSE)（因集成 GPLv3 的 [libpinyin](https://github.com/libpinyin/libpinyin)，
详见 `docs/decisions/pinyin-engine/`）。
