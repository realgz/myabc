// SPDX-License-Identifier: GPL-3.0-or-later
//
// src/domains/input-engine/backend/bihuoma_table.hpp --- 字 -> 笔形码 查表
//
// 依据：docs/plan/05-m4-bihuo-numbers-gbk-plan.md §3.1
//
// 笔形码：横1 竖2 撇3 捺4 折5，通常取该字前若干笔的笔形序列（本实现按"前缀匹配"用，
// 即候选字的笔形码序列以用户输入的数字段为前缀才保留）。
//
// DECISION: 表未命中的字永远不参与过滤（返回 std::nullopt，调用方原样放行），
// 保证空表/小表在功能上是安全的空操作，绝不会因为数据缺失而错误地滤掉正确候选。
// 数据来源：assets/data/bihuoma.txt 现已从最初 8 字的种子表换成基于 cnchar-order
// （MIT License）笔画顺序数据推算的约 6900 字全量表，见 assets/README.md 与
// docs/decisions/_debt-log.md 2026-09-12。

#ifndef MYABC_ENGINE_BIHUOMA_TABLE_HPP
#define MYABC_ENGINE_BIHUOMA_TABLE_HPP

#include <optional>
#include <string>
#include <unordered_map>

namespace myabc::engine {

class BihuoTable {
public:
    // 从文件加载，格式：每行 "<单字 UTF-8>\t<笔形码数字串，如 31>"，'#' 开头整行是注释，
    // 空行跳过。文件不存在或打不开时静默返回 false（表保持空，全部查询按"未收录"处理）。
    bool LoadFromFile(const std::string& path);

    // 从内存字符串按同样的格式加载（供单测用，不碰文件系统）。
    void LoadFromText(const std::string& tsv_text);

    // utf8_char：一个字的 UTF-8 字节序列。未收录返回 std::nullopt。
    std::optional<std::string> Lookup(const std::string& utf8_char) const;

    std::size_t size() const noexcept { return table_.size(); }

private:
    std::unordered_map<std::string, std::string> table_;
};

// 取 UTF-8 字符串的第一个字符（按 UTF-8 变长编码规则），字符串为空返回空串。
std::string FirstUtf8Char(const std::string& utf8_text);

}  // namespace myabc::engine

#endif  // MYABC_ENGINE_BIHUOMA_TABLE_HPP
