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

// 简单的UTF-8字符计数函数
inline size_t utf8_length(const std::string &str) {
    size_t len = 0;
    for (size_t i = 0; i < str.size();) {
        unsigned char c = str[i];
        if (c < 0x80) {
            i += 1;
        }
        else if ((c >> 5) == 0x06) {
            i += 2;
        }
        else if ((c >> 4) == 0x0e) {
            i += 3;
        }
        else if ((c >> 3) == 0x1e) {
            i += 4;
        }
        else {
            i += 1; // 无效字符，跳过
        }
        len++;
    }
    return len;
}

// UTF-8安全的字符串截断
inline std::string utf8_truncate(const std::string &str, size_t max_chars) {
    if (utf8_length(str) <= max_chars) {
        return str;
    }

    size_t chars = 0;
    size_t bytes = 0;

    for (size_t i = 0; i < str.size() && chars < max_chars - 3;) {
        unsigned char c          = str[i];
        size_t        char_bytes = 1;

        if (c < 0x80) {
            char_bytes = 1;
        }
        else if ((c >> 5) == 0x06) {
            char_bytes = 2;
        }
        else if ((c >> 4) == 0x0e) {
            char_bytes = 3;
        }
        else if ((c >> 3) == 0x1e) {
            char_bytes = 4;
        }

        if (i + char_bytes > str.size())
            break;

        i += char_bytes;
        bytes = i;
        chars++;
    }

    return str.substr(0, bytes) + "...";
}

class ConsoleTable {
public:
    ConsoleTable(const std::string &tableName, std::vector<std::string> headers, size_t maxColumnWidth = 50)
        : mTableName(tableName), mHeaders(std::move(headers)), mMaxColumnWidth(maxColumnWidth) {
        mColumnWidths.resize(mHeaders.size());
        for (size_t i = 0; i < mHeaders.size(); ++i) {
            size_t headerLen = utf8_length(mHeaders[i]);
            mColumnWidths[i] = std::min(headerLen, mMaxColumnWidth);
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

            // 截断过长的内容 (UTF-8安全)
            if (utf8_length(tmp) > mMaxColumnWidth) {
                tmp = utf8_truncate(tmp, mMaxColumnWidth);
            }

            // 更新每一列的最大宽度
            size_t displayLen = utf8_length(tmp);
            mColumnWidths[i]  = std::max(mColumnWidths[i], std::min(displayLen, mMaxColumnWidth));
        }
    }

    void print(std::ostream &stream = std::cout) const {
        // 打印表名
        stream << fmtlib::format("Table: {}\n", mTableName);

        printSeparator(stream);
        printRow(stream, mHeaders);
        printSeparator(stream);
        for (const auto &row : mRows) {
            printRow(stream, row);
        }
        printSeparator(stream);
        // 打印行数统计
        stream << fmtlib::format("{} rows in set.\n", mRows.size());
    }

private:
    void printSeparator(std::ostream &stream) const {
        stream << "+";
        for (const auto &width : mColumnWidths) {
            stream << fmtlib::format("{:-^{}}+", "", width + 2);
        }
        stream << std::endl;
    }

    void printRow(std::ostream &stream, const std::vector<std::string> &row) const {
        stream << "|";
        for (size_t i = 0; i < row.size(); ++i) {
            stream << fmtlib::format(" {:<{}} |", row[i], mColumnWidths[i]);
        }
        stream << std::endl;
    }
    std::string                           mTableName;
    std::vector<std::string>              mHeaders;
    std::vector<std::vector<std::string>> mRows;
    std::vector<size_t>                   mColumnWidths;
    size_t                                mMaxColumnWidth;
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
    else if constexpr (requires(const T &t) { fmtlib::format("{}", t); }) {
        return fmtlib::format("{}", t);
    }
    else {
        // 对于其他类型，尝试使用 neko 的序列化或返回占位符
        return NEKO_NAMESPACE::serializable_to_string(t);
    }
}

} // namespace detail

ILIAS_SQL_NS_END
