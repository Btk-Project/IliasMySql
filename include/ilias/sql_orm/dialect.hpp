#pragma once

#include <type_traits>
#include <algorithm>
#include <cctype>
#include <stdexcept>

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_traits.hpp"
#include "ilias/sql/sqlresult.hpp"

#include <ilias/task.hpp>

ILIAS_SQL_NS_BEGIN

struct SqliteTag {};
struct MysqlTag {};
struct PostgresTag {};

template <typename BackendTag>
struct Dialect;

namespace detail {
inline auto is_valid_sql_identifier(std::string_view identifier) -> bool {
    auto is_start = [](unsigned char c) {
        return std::isalpha(c) != 0 || c == '_';
    };
    auto is_body = [&](unsigned char c) {
        return is_start(c) || std::isdigit(c) != 0;
    };

    if (identifier.empty() || !is_start(static_cast<unsigned char>(identifier.front()))) {
        return false;
    }
    return std::ranges::all_of(identifier, [&](char c) { return is_body(static_cast<unsigned char>(c)); });
}

inline auto quote_sql_identifier(std::string_view identifier, char quote) -> std::string {
    if (!is_valid_sql_identifier(identifier)) {
        throw std::invalid_argument("Invalid SQL identifier: " + std::string(identifier));
    }
    return std::string(1, quote) + std::string(identifier) + std::string(1, quote);
}

inline auto trim_sql_identifier(std::string_view identifier) -> std::string_view {
    while (!identifier.empty() && std::isspace(static_cast<unsigned char>(identifier.front())) != 0) {
        identifier.remove_prefix(1);
    }
    while (!identifier.empty() && std::isspace(static_cast<unsigned char>(identifier.back())) != 0) {
        identifier.remove_suffix(1);
    }
    return identifier;
}

inline auto quote_sql_identifier_path(std::string_view identifier, char quote) -> std::string {
    identifier = trim_sql_identifier(identifier);
    if (identifier.empty()) {
        throw std::invalid_argument("Invalid SQL identifier: " + std::string(identifier));
    }

    std::string quoted;
    std::size_t start = 0;
    while (start <= identifier.size()) {
        const auto dot  = identifier.find('.', start);
        const auto part = trim_sql_identifier(identifier.substr(start, dot - start));
        if (part.empty()) {
            throw std::invalid_argument("Invalid SQL identifier path: " + std::string(identifier));
        }
        if (!quoted.empty()) {
            quoted.push_back('.');
        }
        quoted += quote_sql_identifier(part, quote);
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return quoted;
}
} // namespace detail

struct ColumnSchema {
    std::string name;
    std::string db_type; // 数据库报告的原始类型字符串, e.g., "VARCHAR(255)", "INTEGER"
    SqlTags     tags;
};

// 用于封装校验结果
struct ValidationResult {
    std::vector<std::string> errors;

    bool is_ok() const { return errors.empty(); }
    void add_error(const std::string &error) { errors.push_back(error); }
};

// ================= SQLite 特化 =================
template <>
struct Dialect<SqliteTag> {
    static bool check(std::string_view name) { return name == "sqlite"; }
    static bool validate_identifier(std::string_view name) { return detail::is_valid_sql_identifier(name); }
    static std::string quote_identifier(std::string_view name) { return detail::quote_sql_identifier(name, '"'); }
    static std::string quote_identifier_path(std::string_view name) {
        return detail::quote_sql_identifier_path(name, '"');
    }
    static std::string qualified_identifier(std::string_view table, std::string_view column) {
        return quote_identifier(table) + "." + quote_identifier(column);
    }
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
        parts.push_back(quote_identifier(name));
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
                std::string statement = "CREATE INDEX " + quote_identifier(index_name) + " ON " +
                                        quote_identifier(table_name) + " (" + quote_identifier(column_name) + ")";
                index_statements.push_back(statement);
            }
        }

        return index_statements;
    }

    /**
     * @brief 返回用于查询表结构的SQL语句。
     */
    static std::string get_schema_query(std::string_view table_name) {
        // SQLite使用 PRAGMA table_info
        return "PRAGMA table_info(" + quote_identifier(table_name) + ");";
    }

    using SchemaQueryResultType = std::tuple<int, std::string, std::string, int, std::optional<std::string>, int>;
    /**
     * @brief 解析PRAGMA table_info的结果。
     * @note PRAGMA table_info 返回: cid, name, type, notnull, dflt_value, pk
     */
    static auto parse_schema_result(SqlResult<SchemaQueryResultType> rs)
        -> IoTask<std::map<std::string, ColumnSchema>> {
        std::map<std::string, ColumnSchema> schema_map;
        ilias_for_await(auto row, rs.rangeResult()) {
            if (!row) {
                co_return Unexpected(row.error());
            }
            auto [cid, name, type, notnull, dflt_value, pk] = row.value();
            ColumnSchema col;
            col.name             = name;
            col.db_type          = type;
            col.tags.not_null    = (notnull == 1);
            col.tags.primary_key = (pk == 1);
            schema_map[col.name] = std::move(col);
        }
        co_return schema_map;
    }

    /**
     * @brief 比较C++类型和数据库类型是否兼容。
     */
    template <typename FieldType>
    static bool are_types_compatible(const std::string &db_type, const SqlTags &tags) {
        // SQLite的类型亲和性比较宽松
        std::string expected_type = type_name<FieldType>(tags); // e.g., "INTEGER", "REAL", "TEXT"
        std::string upper_db_type = db_type;
        std::transform(upper_db_type.begin(), upper_db_type.end(), upper_db_type.begin(), ::toupper);

        if (expected_type == "INTEGER") {
            return upper_db_type.find("INT") != std::string::npos;
        }
        if (expected_type == "REAL") {
            return upper_db_type.find("REAL") != std::string::npos ||
                   upper_db_type.find("FLOAT") != std::string::npos ||
                   upper_db_type.find("DOUBLE") != std::string::npos;
        }
        // 对于TEXT, BLOB等，通常是直接比较
        return upper_db_type.find(expected_type) != std::string::npos;
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
    static bool validate_identifier(std::string_view name) { return detail::is_valid_sql_identifier(name); }
    static std::string quote_identifier(std::string_view name) { return detail::quote_sql_identifier(name, '`'); }
    static std::string quote_identifier_path(std::string_view name) {
        return detail::quote_sql_identifier_path(name, '`');
    }
    static std::string qualified_identifier(std::string_view table, std::string_view column) {
        return quote_identifier(table) + "." + quote_identifier(column);
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

        parts.push_back(quote_identifier(name));
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
                std::string statement = "CREATE INDEX " + quote_identifier(index_name) + " ON " +
                                        quote_identifier(table_name) + " (" + quote_identifier(column_name) + ")";
                index_statements.push_back(statement);
            }
        }

        return index_statements;
    }

    /**
     * @brief 返回用于查询表结构的SQL语句。
     * @note 使用参数化查询以防止SQL注入，这里返回带'?'的模板。
     */
    static std::string get_schema_query() {
        return "SELECT COLUMN_NAME, COLUMN_TYPE, IS_NULLABLE, COLUMN_KEY "
               "FROM INFORMATION_SCHEMA.COLUMNS "
               "WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ?";
    }

    using SchemaQueryResultType = std::tuple<std::string, std::string, std::string, std::string>;
    /**
     * @brief 解析INFORMATION_SCHEMA查询的结果。
     */
    static auto parse_schema_result(SqlResult<SchemaQueryResultType> rs)
        -> IoTask<std::map<std::string, ColumnSchema>> {
        std::map<std::string, ColumnSchema> schema_map;
        ilias_for_await(auto row, rs.rangeResult()) {
            if (!row) {
                co_return Unexpected(row.error());
            }
            auto [name, type, is_nullable, key_type] = row.value();
            ColumnSchema col;
            col.name             = name;
            col.db_type          = type;
            col.tags.not_null    = (is_nullable == "NO");
            col.tags.primary_key = (key_type == "PRI");
            schema_map[col.name] = std::move(col);
        }
        co_return schema_map;
    }

    template <typename FieldType>
    static bool are_types_compatible(const std::string &db_type, const SqlTags &tags) {
        using DT                       = detail::strip_wrapper_t<FieldType>;
        std::string expected_base_type = type_name<DT>(tags);

        // 简化比较逻辑：检查数据库类型字符串是否以期望的类型开头
        // 例如，C++ int -> "INT"，数据库 "int(11)"。 "int(11)".startswith("int")
        std::string base_db_type       = db_type.substr(0, db_type.find('('));
        std::string base_expected_type = expected_base_type.substr(0, expected_base_type.find('('));

        return base_db_type == base_expected_type;
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
    static bool validate_identifier(std::string_view name) { return detail::is_valid_sql_identifier(name); }
    static std::string quote_identifier(std::string_view name) { return detail::quote_sql_identifier(name, '"'); }
    static std::string quote_identifier_path(std::string_view name) {
        return detail::quote_sql_identifier_path(name, '"');
    }
    static std::string qualified_identifier(std::string_view table, std::string_view column) {
        return quote_identifier(table) + "." + quote_identifier(column);
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

        parts.push_back(quote_identifier(name));
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
                std::string statement = "CREATE INDEX " + quote_identifier(index_name) + " ON " +
                                        quote_identifier(table_name) + " (" + quote_identifier(column_name) + ")";
                index_statements.push_back(statement);
            }
        }

        return index_statements;
    }
    /**
     * @brief 返回用于查询表结构元数据的SQL语句 (PostgreSQL原生版本)。
     * @note 查询 pg_catalog 比 information_schema 更快且信息更全。
     * @return 返回一个需要绑定表名作为第一个参数的SQL模板。
     */
    static std::string get_schema_query() {
        return R"SQL(
            SELECT
                a.attname AS column_name,
                format_type(a.atttypid, a.atttypmod) AS column_type,
                a.attnotnull AS is_not_null,
                COALESCE(con.contype = 'p', false) AS is_primary_key
            FROM
                pg_class AS c
            JOIN
                pg_attribute AS a ON a.attrelid = c.oid
            LEFT JOIN
                pg_constraint AS con ON con.conrelid = c.oid AND a.attnum = ANY(con.conkey) AND con.contype = 'p'
            WHERE
                c.relname = $1
                AND c.relnamespace = (SELECT oid FROM pg_namespace WHERE nspname = current_schema())
                AND a.attnum > 0 AND NOT a.attisdropped
            ORDER BY
                a.attnum;
        )SQL";
    }
    using SchemaQueryResultType = std::tuple<std::string, std::string, bool, bool>;

    /**
     * @brief 解析 get_schema_query 返回的结果集。
     * @param rs 数据库驱动返回的结果集对象。
     * @return 一个从列名到其Schema描述的map。
     */
    static auto parse_schema_result(SqlResult<SchemaQueryResultType> rs)
        -> IoTask<std::map<std::string, ColumnSchema>> {
        std::map<std::string, ColumnSchema> schema_map;
        ilias_for_await(auto row, rs.rangeResult()) {
            if (!row) {
                co_return Unexpected(row.error());
            }
            auto [name, type, is_not_null, is_pk] = row.value();
            ColumnSchema col;
            col.name             = name;
            col.db_type          = type;
            col.tags.not_null    = is_not_null;
            col.tags.primary_key = is_pk;
            schema_map[col.name] = std::move(col);
        }
        co_return schema_map;
    }

    /**
     * @brief 比较C++类型和数据库类型字符串是否兼容 (PostgreSQL版本)。
     */
    template <typename FieldType>
    static bool are_types_compatible(const std::string &db_type, const SqlTags &tags) {
        std::string orm_type      = type_name<FieldType>(tags);
        std::string lower_db_type = db_type;
        std::transform(lower_db_type.begin(), lower_db_type.end(), lower_db_type.begin(), ::tolower);

        // SERIAL 和 BIGSERIAL 在数据库中显示为 integer 和 bigint
        if (tags.auto_increment) {
            if (orm_type == "INTEGER" && lower_db_type == "integer")
                return true;
            if (orm_type == "BIGINT" && lower_db_type == "bigint")
                return true;
        }

        // 处理 VARCHAR -> character varying 的同义词情况
        if (orm_type.rfind("VARCHAR", 0) == 0) {  // orm_type starts with VARCHAR
            auto orm_suffix = orm_type.substr(7); // (255)
            if (lower_db_type.rfind("character varying", 0) == 0) {
                return lower_db_type.substr(17) == orm_suffix;
            }
        }

        // 处理 DOUBLE PRECISION 的大小写和空格
        if (orm_type == "DOUBLE PRECISION" && lower_db_type == "double precision")
            return true;

        // 其他大部分类型可以直接比较(转为小写)
        std::string lower_orm_type = orm_type;
        std::transform(lower_orm_type.begin(), lower_orm_type.end(), lower_orm_type.begin(), ::tolower);

        return lower_db_type == lower_orm_type;
    }
};

ILIAS_SQL_NS_END
