// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/wubi_table.hpp --- 五笔编码 -> 候选字/词 查表
//
// 依据：docs/plan/08-wubi-input-scheme-plan.md §5.4
//       docs/decisions/input-engine/20260913-wubi-input-scheme.md
//
// 跟 BihuoTable（字 -> 笔形码，单一映射）方向相反：编码 -> 候选列表，一对多，且
// 需要按权重排序，不能复用 BihuoTable 本身，但复用它"安全空表 + LoadFromText 供
// 单测、不碰文件系统"这套既有设计模式。
//
// DECISION: 表未命中的编码返回空 vector（不是错误），调用方（WubiCandidateSource）
// 按"组字中无候选"既有分支处理，不崩溃、不报错——文件缺失/损坏时五笔模式退化为
// "任何编码都查不到候选"而不是让引擎异常终止。
// 数据来源：assets/data/wubi86.txt，转换自 rime/rime-wubi 的 wubi86.dict.yaml
// （LGPL-3.0），见 assets/README.md。

#ifndef MYABC_ENGINE_WUBI_TABLE_HPP
#define MYABC_ENGINE_WUBI_TABLE_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace myabc::engine {

class WubiTable {
public:
    // 从文件加载，格式：每行 "<编码>\t<字/词 UTF-8>\t<权重整数>"，'#' 开头整行是注释，
    // 空行跳过。文件不存在或打不开时静默返回 false（表保持空，全部查询按"未收录"
    // 处理）。
    bool LoadFromFile(const std::string& path);

    // 从内存字符串按同样的格式加载（供单测用，不碰文件系统）。
    void LoadFromText(const std::string& tsv_text);

    // code：小写字母编码（1-4 位）。未收录返回空 vector（不是错误）。
    // 返回值已按权重降序排列（同一编码下多个候选，如重码字，权重高的排前面）。
    std::vector<std::string> Lookup(const std::string& code) const;

    std::size_t size() const noexcept { return table_.size(); }

private:
    // code -> [(text, weight)]，加载完一次性按 weight 降序排序，Lookup 直接返回
    // 已排序结果的 text 部分（避免每次查询都排序）。
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::int64_t>>> table_;
};

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_WUBI_TABLE_HPP
