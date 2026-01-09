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
struct PostgresTag {};

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

    static constexpr bool support_auto_increment() { return true; }
    static constexpr bool support_timestamp_default() { return true; }
    static constexpr bool support_timestamp_update() { return false; }

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

        // 时间戳默认值处理
        if (tags.created_at) {
            parts.push_back("DEFAULT CURRENT_TIMESTAMP");
        }

        // 注意：SQLite不支持ON UPDATE CURRENT_TIMESTAMP，updated_at需要在应用层处理

        return detail::join_strs(parts, " ");
    }

    // 生成索引语句
    static std::vector<std::string>
    generate_index_statements(std::string_view                                    table_name,
                              const std::vector<std::pair<std::string, SqlTags>> &columns) {
        std::vector<std::string> index_statements;

        for (const auto &[column_name, tags] : columns) {
            if (tags.index && !tags.primary_key && !tags.unique) {
                // 只为普通索引生成CREATE INDEX语句，主键和唯一键会自动创建索引
                std::string index_name = "idx_" + std::string(table_name) + "_" + column_name;
                std::string statement =
                    "CREATE INDEX " + index_name + " ON " + std::string(table_name) + " (" + column_name + ")";
                index_statements.push_back(statement);
            }
        }

        return index_statements;
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
    static constexpr bool support_auto_increment() { return true; }
    static constexpr bool support_timestamp_default() { return true; }
    static constexpr bool support_timestamp_update() { return true; }
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

        // 时间戳默认值和更新行为
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
            // 主键和唯一键会自动创建索引，所以只为普通列添加索引
            // 注意：更规范的做法是在表末尾用 KEY `idx_name` (`col_name`) 创建
            parts.push_back("KEY");
        }

        return detail::join_strs(parts, " ");
    }

    // 生成索引语句
    static std::vector<std::string>
    generate_index_statements(std::string_view                                    table_name,
                              const std::vector<std::pair<std::string, SqlTags>> &columns) {
        std::vector<std::string> index_statements;

        for (const auto &[column_name, tags] : columns) {
            if (tags.index && !tags.primary_key && !tags.unique) {
                // 只为普通索引生成CREATE INDEX语句，主键和唯一键会自动创建索引
                std::string index_name = "idx_" + std::string(table_name) + "_" + column_name;
                std::string statement =
                    "CREATE INDEX `" + index_name + "` ON `" + std::string(table_name) + "` (`" + column_name + "`)";
                index_statements.push_back(statement);
            }
        }

        return index_statements;
    }
};

// ================= PostgreSQL 特化 =================
template <>
struct Dialect<PostgresTag> {
    static bool check(std::string_view name) {
        std::string nameLower {name};
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        return nameLower == "postgresql" || nameLower == "postgres" || nameLower == "pgsql";
    }

    // 1. 类型映射
    template <typename T>
    static constexpr std::string type_name([[maybe_unused]] const SqlTags &tags) {
        using DT = detail::strip_wrapper_t<T>;
        if constexpr (std::is_same_v<DT, SqlNull>) {
            static_assert(!std::is_same_v<DT, SqlNull>, "SqlNull is not a valid type for PostgreSQL");
        }
        else if constexpr (std::is_same_v<DT, bool>) {
            return "BOOLEAN";
        }
        else if constexpr (std::is_integral_v<DT>) {
            if (sizeof(DT) == sizeof(int16_t))
                return "SMALLINT";
            else if (sizeof(DT) == sizeof(int32_t))
                return "INTEGER";
            else if (sizeof(DT) == sizeof(int64_t))
                return "BIGINT";
            else
                return "INTEGER";
        }
        else if constexpr (std::is_same_v<DT, float>)
            return "REAL";
        else if constexpr (std::is_same_v<DT, double>)
            return "DOUBLE PRECISION";
        else if constexpr (std::is_same_v<DT, SqlDate>)
            return "TIMESTAMP";
        else if constexpr (std::is_same_v<DT, SqlBlob>)
            return "BYTEA";
        else if constexpr (std::is_same_v<DT, std::string> || std::is_same_v<DT, const char *>) {
            bool needs_index = tags.primary_key || tags.unique || tags.index;
            if (tags.length > 0) {
                return "VARCHAR(" + std::to_string(tags.length) + ")";
            }
            if (needs_index) {
                return "VARCHAR(255)";
            }
            return "TEXT";
        }
        else {
            return "TEXT";
        }
    }

    template <typename T>
    static std::string generate_column_definition(std::string_view name, const SqlTags &tags) {
        std::vector<std::string> parts;

        parts.push_back("\"" + std::string(name) + "\"");
        parts.push_back(type_name<T>(tags));

        if (tags.not_null) {
            parts.push_back("NOT NULL");
        }

        // PostgreSQL uses SERIAL/BIGSERIAL for auto increment
        if (tags.auto_increment) {
            // Replace the type with SERIAL for auto increment
            if constexpr (std::is_integral_v<detail::strip_wrapper_t<T>>) {
                if (sizeof(detail::strip_wrapper_t<T>) <= sizeof(int32_t)) {
                    parts[1] = "SERIAL";
                }
                else {
                    parts[1] = "BIGSERIAL";
                }
            }
        }

        // 时间戳默认值处理
        if (tags.created_at) {
            parts.push_back("DEFAULT CURRENT_TIMESTAMP");
        }

        // PostgreSQL doesn't support ON UPDATE CURRENT_TIMESTAMP like MySQL
        // updated_at needs to be handled at application level or with triggers

        if (tags.unique && !tags.primary_key) {
            parts.push_back("UNIQUE");
        }

        if (tags.primary_key) {
            parts.push_back("PRIMARY KEY");
        }

        return detail::join_strs(parts, " ");
    }

    static constexpr bool support_auto_increment() { return true; }
    static constexpr bool support_timestamp_default() { return true; }
    static constexpr bool support_timestamp_update() { return false; }

    // 生成索引语句
    static std::vector<std::string>
    generate_index_statements(std::string_view                                    table_name,
                              const std::vector<std::pair<std::string, SqlTags>> &columns) {
        std::vector<std::string> index_statements;

        for (const auto &[column_name, tags] : columns) {
            if (tags.index && !tags.primary_key && !tags.unique) {
                // 只为普通索引生成CREATE INDEX语句，主键和唯一键会自动创建索引
                std::string index_name = "idx_" + std::string(table_name) + "_" + column_name;
                std::string statement  = "CREATE INDEX \"" + index_name + "\" ON \"" + std::string(table_name) +
                                        "\" (\"" + column_name + "\")";
                index_statements.push_back(statement);
            }
        }

        return index_statements;
    }
};

ILIAS_SQL_NS_END
