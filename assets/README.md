# assets

静态资源与测试夹具，不放可执行逻辑。子目录用途/来源/版权在此登记。

## icons/

- `myabc.ico`：语言栏/输入法切换菜单图标，16/20/24/32/48/256 多尺寸。蓝底 + 白色"拼"字，
  纯代码生成（Pillow + 系统自带 Microsoft YaHei Bold 字体渲染，见
  `docs/decisions/_debt-log.md` 2026-09-11 的生成方式记录），无第三方素材/版权问题。
  由 `src/domains/tsf-service/tsf-service.rc` 编译进 `myabc-tip.dll` 作为资源，
  `ITfInputProcessorProfiles::AddLanguageProfile` 的 icon 参数指向 DLL 自身 + IconIndex=0。

## config/

- `config.sample.toml`：配置样例（`config_loader` 尚未解析 TOML，见 debt-log）。
- `punctuation.toml`：标点全角映射数据草稿（同上）。

## data/

- `bihuoma.txt`：笔形辅助码表（字 -> 笔形码，横1竖2撇3捺4折5）。笔画顺序原始
  数据取自 [cnchar-order](https://github.com/theajack/cnchar)（npm 包
  `cnchar-order@3.2.6`，MIT License，Copyright (c) theajack），按 myabc 自己的
  规则（取前两笔、归并成五笔形分类）转换生成，覆盖约 6939 个字。转换脚本与推导
  过程记录见 `docs/decisions/_debt-log.md` 2026-09-12。最初（M4）只有 8 个字的
  种子表，2026-09-12 换成这份全量表。
- `wubi86.txt`：五笔字型（86 版）编码表（code -> text -> weight）。原始数据取自
  [rime-wubi](https://github.com/rime/rime-wubi) 的 `wubi86.dict.yaml`
  （LGPL-3.0 License，与 myabc 的 GPL-3.0-or-later 兼容），按基本 CJK
  统一表意文字区（U+4E00-U+9FFF）过滤、丢弃拆分提示字段后转换生成，共 86788
  条。转换脚本与统计记录见文件头部注释及 `docs/decisions/_debt-log.md`
  2026-09-13。原始 LICENSE 全文见同目录 `wubi86-LICENSE`。
