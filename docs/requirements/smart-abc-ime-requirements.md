# 智能ABC输入法 需求说明

- 状态：生效
- 更新日期：2026-09-09
- 来源：用户对话（2026-09-09）

## 1. 定位

在 **Windows 10 / Windows 11** 下可用的中文智能拼音输入法，体验对标经典"智能ABC"，
但用现代框架（TSF）与现代引擎（libpinyin 统计语言模型）实现。

## 2. 已确认的技术决策

| 项 | 决策 | 备选与否决理由见 |
|---|---|---|
| 框架层 | TSF（Text Services Framework） | `docs/decisions/ime-framework/` |
| 拼音引擎 | 集成 libpinyin（统计语言模型，智能整句） | `docs/decisions/pinyin-engine/` |
| 语言/构建 | C++ / CMake / MSVC v143（VS 2022） | `docs/decisions/project-setup/` |
| 架构 | 薄 TSF DLL + 独立引擎进程 + 候选窗 UI，IPC 解耦 | `docs/architecture/` |
| License | 整体 GPLv3（libpinyin 传染） | reference 调研文档 §5 |

## 3. 功能范围 —— 第一个可用版本（MVP + 智能ABC经典特性）

### 3.1 MVP 智能拼音（必须）
- 全拼输入、智能整句转换（句子级最优路径，非逐字选词）
- 候选列表 + 翻页（`-`/`=` 或 `,`/`.`，数字键选字）
- 用户词库自学习（记忆新词、调整词频）
- 中英文切换（Shift 切换，英文模式直接上屏）
- 标点符号中英文映射
- 词间上屏、部分上屏、回退重选

### 3.2 智能ABC经典特性（本版本纳入）
- **简拼 / 混拼**：`bj` → 北京；`beij` / `bjing` 等混合
- **音节自动切分**：`xian` → xi'an 与 xian 的歧义处理与提示
- **笔形辅助码**：`横竖撇捺折` (1–5) 作为二级筛选码（智能ABC 的标志特性）
- **中文数字 / 金额大写**：`i` 引导数字 → 一二三…；金额大写"壹拾贰"
- **GBK 生僻字**：候选覆盖 GBK 字集，不止 GB2312

### 3.3 平台兼容（验收硬指标）
- 记事本 / 写字板（传统 Win32 edit）
- 浏览器（Chrome / Edge，含地址栏与网页输入框）
- UWP / 商店应用（如"邮件""设置"搜索框）
- Office（Word / Excel）
- 32 位与 64 位宿主进程都能加载（DLL 双架构）

## 4. 非目标（本版本不做）

- 云输入 / 大模型联想
- 五笔、双拼方案（架构预留，不实现）
- 手写 / 语音
- ARM64（架构预留）
- 皮肤系统（候选窗先做够用的默认样式）
- 安装包签名与商店上架

## 5. 待明确（plan 阶段或后续澄清）

- libpinyin 的 Windows/MSVC 构建路径（见 reference §3 风险）
- 词库来源（libpinyin 自带 model.text / 是否需自训练增补）
- 候选窗 UI 技术（Win32 分层窗 / Direct2D / 其他）
- 安装与注册方式（regsvr32 / 自写 installer / MSIX）
