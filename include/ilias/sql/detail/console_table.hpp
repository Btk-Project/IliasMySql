#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include <stdint.h>

#include <nekoproto/serialization/to_string.hpp>
#include <nekoproto/serialization/types/types.hpp>

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql/detail/coverter.hpp"

ILIAS_SQL_NS_BEGIN

namespace detail {

class ConsoleTable {
public:
    ConsoleTable(const std::string &tableName, std::vector<std::string> headers)
        : mTableName(tableName), mHeaders(std::move(headers)) {
        mColumnWidths.resize(mHeaders.size());
        for (size_t i = 0; i < mHeaders.size(); ++i) {
            mColumnWidths[i] = mHeaders[i].length();
        }
    }

    void addRow(const std::vector<std::string> &row) {
        if (row.size() != mHeaders.size())
            return;
        mRows.push_back(row);
        for (size_t i = 0; i < row.size(); ++i) {
            // 处理换行符
            std::string &tmp = mRows.back()[i];
            size_t       pos = 0;
            while ((pos = tmp.find('\n', pos)) != std::string::npos)
                tmp.replace(pos, 1, "\\n"), pos += 2; // 换行符替换为 \n
            // 更新每一列的最大宽度
            // 注意：这里假设是 ASCII，如果是中文，对齐可能会有偏差，需要专门的 utf8 长度计算库
            mColumnWidths[i] = std::max(mColumnWidths[i], tmp.length());
        }
    }

    void print() const {
        // 打印表名
        printf("%s\n", fmtlib::format("Table: {}", mTableName).c_str());
        printSeparator();
        printRow(mHeaders);
        printSeparator();
        for (const auto &row : mRows) {
            printRow(row);
        }
        printSeparator();
        // 打印行数统计
        printf("%s", fmtlib::format("{} rows in set.\n", mRows.size()).c_str());
    }

private:
    void printSeparator() const {
        printf("+");
        for (const auto &width : mColumnWidths) {
            printf("%s", fmtlib::format("{:-^{}}+", "", width + 2).c_str());
        }
        printf("\n");
    }

    void printRow(const std::vector<std::string> &row) const {
        printf("|");
        for (size_t i = 0; i < row.size(); ++i) {
            printf(" %s |", fmtlib::format("{:<{}}", row[i], mColumnWidths[i]).c_str());
        }
        printf("\n");
    }
    std::string                           mTableName;
    std::vector<std::string>              mHeaders;
    std::vector<std::vector<std::string>> mRows;
    std::vector<size_t>                   mColumnWidths;
};

// 辅助函数：将任意类型转为 string
template <typename T>
std::string to_string_view(const T &t) {
    if constexpr (DereferenceableAndNullable<std::decay_t<T>>) {
        if (!t.has_value()) {
            return "NULL";
        }
        return to_string_view(*t);
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
        return t;
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, std::string_view>) {
        return std::string(t);
    }
    else if constexpr (std::is_enum_v<std::decay_t<T>>) {
        return std::to_string(static_cast<std::underlying_type_t<std::decay_t<T>>>(t));
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, SqlDate>) {
        return t.toString();
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, SqlBlob>) {
        // std::vector<std::byte>
        std::stringstream result;
        for (const auto &byte : t) {
            result << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
        }
        return result.str();
    }
    else if constexpr (std::is_arithmetic_v<std::decay_t<T>>) {
        return std::to_string(t);
    }
    else {
        // 对于其他类型，尝试使用 neko 的序列化或返回占位符
        return NEKO_NAMESPACE::serializable_to_string(t);
    }
}

} // namespace detail

ILIAS_SQL_NS_END
