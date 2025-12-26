#pragma once

#include <type_traits>
#include <algorithm>

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_traits.hpp"

ILIAS_SQL_NS_BEGIN

struct SqliteTag {};
struct MysqlTag {};

template <typename BackendTag>
struct Dialect;

// ================= SQLite 特化 =================
template <>
struct Dialect<SqliteTag> {
    static bool check(std::string_view name) { return name == "sqlite"; }
    // 1. 类型映射
    template <typename T>
    static constexpr std::string type_name([[maybe_unused]] const SqlTags &tags) {
        using DT = detail::strip_wrapper_t<T>;
        if constexpr (std::is_same_v<DT, SqlNull>) {
            static_assert(!std::is_same_v<DT, SqlNull>, "SqlNull is not a valid type for SQLite");
        }
        else if constexpr (std::is_integral_v<DT> || std::is_same_v<DT, bool> || std::is_enum_v<DT>) {
            return "INTEGER"; // SQLite 只有 INTEGER
        }
        else if constexpr (std::is_same_v<DT, float> || std::is_same_v<DT, double>) {
            return "REAL";
        }
        else if constexpr (std::is_same_v<DT, SqlBlob>) {
            return "BLOB";
        }
        else {
            return "TEXT";
        }
    }

    template <typename T>
    static std::string generate_column_definition(std::string_view name, const SqlTags &tags) {
        std::vector<std::string> parts;
        parts.push_back(std::string(name));
        parts.push_back(type_name<T>(tags));

        // 约束
        // 注意: SQLite中，PRIMARY KEY AUTOINCREMENT 必须一起使用且作用于INTEGER类型
        if (tags.primary_key) {
            parts.push_back("PRIMARY KEY");
            if (tags.auto_increment) {
                parts.push_back("AUTOINCREMENT");
            }
        }
        if (tags.not_null) {
            parts.push_back("NOT NULL");
        }
        if (tags.unique && !tags.primary_key) {
            parts.push_back("UNIQUE");
        }
        if (tags.created_at) {
            parts.push_back("DEFAULT CURRENT_TIMESTAMP");
        }

        return detail::join_strs(parts, " ");
    }
};

// ================= MySQL 特化 =================
template <>
struct Dialect<MysqlTag> {
    static bool check(std::string_view name) {
        std::string nameLower {name};
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        return nameLower == "mysql" || nameLower == "mariadb";
    }
    // 1. 类型映射
    template <typename T>
    static constexpr std::string type_name([[maybe_unused]] const SqlTags &tags) {
        using DT = detail::strip_wrapper_t<T>;
        if constexpr (std::is_same_v<DT, bool>) {
            return "TINYINT(1)";
        }
        else if constexpr (std::is_integral_v<DT>) {
            std::string base_type;
            if (sizeof(DT) == sizeof(int8_t))
                base_type = "TINYINT";
            else if (sizeof(DT) == sizeof(int16_t))
                base_type = "SMALLINT";
            else if (sizeof(DT) == sizeof(int32_t))
                base_type = "INT";
            else if (sizeof(DT) == sizeof(int64_t))
                base_type = "BIGINT";
            else
                base_type = "INTEGER";

            if (tags.unsigned_type)
                base_type += " UNSIGNED";
            return base_type;
        }
        else if constexpr (std::is_same_v<DT, float>)
            return "FLOAT";
        else if constexpr (std::is_same_v<DT, double>)
            return "DOUBLE";
        else if constexpr (std::is_same_v<DT, SqlDate>)
            return "DATETIME";
        else if constexpr (std::is_same_v<DT, SqlBlob>)
            return "BLOB";
        else if constexpr (std::is_same_v<DT, std::string> || std::is_same_v<DT, const char *>) {
            bool needs_index = tags.primary_key || tags.unique || tags.index;
            if (tags.length > 0) {
                return "VARCHAR(" + std::to_string(tags.length) + ")";
            }
            if (needs_index) {
                return "VARCHAR(255)";
            }
            return "TEXT"; // 提供一个通用的默认长度
        }
        else {
            return "TEXT";
        }
    }

    // 2. 关键字差异

    template <typename T>
    static std::string generate_column_definition(std::string_view name, const SqlTags &tags) {
        std::vector<std::string> parts;

        parts.push_back("`" + std::string(name) + "`");
        parts.push_back(type_name<T>(tags));

        if (tags.not_null) {
            parts.push_back("NOT NULL");
        }
        if (tags.auto_increment) {
            parts.push_back("AUTO_INCREMENT");
        }

        if (tags.created_at) {
            parts.push_back("DEFAULT CURRENT_TIMESTAMP");
        }

        if (tags.updated_at) {
            // DATETIME 和 TIMESTAMP 类型支持 ON UPDATE
            parts.push_back("ON UPDATE CURRENT_TIMESTAMP");
        }

        if (tags.unique && !tags.primary_key) {
            parts.push_back("UNIQUE KEY");
        }

        if (tags.primary_key) {
            parts.push_back("PRIMARY KEY");
        }

        if (tags.index && !tags.primary_key && !tags.unique) {
            // 主键和唯一键会自动创建索引，所以只为普通列添普通加索引
            // 注意：更规范的做法是在表末尾用 KEY `idx_name` (`col_name`) 创建
            parts.push_back("KEY");
        }

        return detail::join_strs(parts, " ");
    }
};

ILIAS_SQL_NS_END
