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
