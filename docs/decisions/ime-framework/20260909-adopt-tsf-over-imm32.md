# 采用 TSF（Text Services Framework）作为框架层，不用 IMM32

- 日期：2026-09-09
- 领域：ime-framework
- 状态：生效中
- 触发来源：用户对话（2026-09-09）+ deep-planner 调研

## 背景
要在 Windows 10/11 上做中文输入法，框架层有两条路：旧的 IMM32（imm32.dll）和现代的
TSF（msctf.dll）。需求 3.3 明确要求支持记事本、浏览器、UWP/商店应用、Office，且 32/64 位宿主都要能加载。

## 决策内容
框架层使用 TSF，输入法实现为注册到系统的 TIP（Text Input Processor）——一个进程内 COM DLL
（myabc-tip.dll），注册 GUID_TFCAT_TIP_KEYBOARD 及 Win8+ 的 TIPCAP 类别集合，语言 profile 为 zh-CN(0x0804)。
不提供 IMM32 实现，也不做 IMM32 回退。
由于 TIP DLL 会被加载进每个文本输入进程（含 AppContainer 沙箱），重逻辑不放 DLL——见架构文档
与 pinyin-engine ADR。

## 被否决的备选方案
- **IMM32**：Win8 起在部分场景有 bug，商店/UWP 应用完全不支持，微软已将其定位为兼容层。
  对需求 3.3 的 UWP 硬指标是致命的。否决。
- **IMM32 + TSF 双实现**：维护两套输入路径成本高，且 IMM32 侧仍无法覆盖 UWP，收益不匹配。否决。
- **仅桌面 hook / 全局键盘钩子自绘**：非标准、杀软敏感、无法进入受保护/沙箱应用、输入合成不可靠。否决。

## 影响范围
- src/domains/tsf-service/（全部）
- src/domains/deployment/backend/（TSF profile 与 category 注册）
- src/domains/candidate-ui/（候选窗与 TSF caret 位置耦合）
- 兼容性测试矩阵（docs/review-standards/acceptance-criteria §1.6）

## 关联
- 调研：docs/reference/windows-ime-framework-reference.md §1-§2
- 架构：docs/architecture/system-overview.md
- 参考实现：微软 SampleIME(MIT)、Weasel(GPLv3)、PIME(LGPL)
