#pragma once

#include <type_traits>
#include "ilias/sql/global/global.hpp"
#include "ilias/sql/types.hpp"

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
    static constexpr std::string_view type_name() {
        using DT = std::decay_t<T>;
        if constexpr (std::is_same_v<DT, SqlNull>) {
            static_assert(!std::is_same_v<DT, SqlNull>, "SqlNull is not a valid type for SQLite");
        }
        else if constexpr (std::is_integral_v<DT>) {
            return "INTEGER"; // SQLite 只有 INTEGER
        }
        else if constexpr (std::is_floating_point_v<DT>) {
            return "REAL";
        }
        else {
            return "TEXT";
        }
    }

    // 2. 关键字差异
    static constexpr std::string_view auto_increment() { return "AUTOINCREMENT"; }
    static constexpr std::string_view primary_key() { return "PRIMARY KEY"; }

    // 3. 绑定占位符 (SQLite 支持 ? 和 :name，假设用 ?)
    static std::string placeholder([[maybe_unused]] int index) { return "?"; }
    static std::string placeholder([[maybe_unused]] std::string_view name) { return ":" + std::string(name); }
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
    static constexpr std::string_view type_name() {
        using DT = std::decay_t<T>;
        if constexpr (std::is_same_v<DT, SqlNull>) {
            static_assert(!std::is_same_v<DT, SqlNull>, "SqlNull is not a valid type for SQLite");
        }
        else if constexpr (std::is_same_v<DT, int64_t>) {
            return "BIGINT";
        }
        else if constexpr (std::is_integral_v<DT>) {
            return "INTEGER";
        }
        else if constexpr (std::is_floating_point_v<DT>) {
            return "DOUBLE";
        }
        else if constexpr (std::is_same_v<DT, SqlDate>) {
            return "DATE";
        }
        else if constexpr (std::is_same_v<DT, SqlBlob>) {
            return "BLOB";
        }
        else {
            return "VARCHAR(255)"; // MySQL 通常需要指定长度，这里可能需要更复杂的逻辑
        }
    }

    // 2. 关键字差异
    static constexpr std::string_view auto_increment() { return "AUTO_INCREMENT"; } // 注意下划线
    static constexpr std::string_view primary_key() { return "PRIMARY KEY"; }

    // 3. 绑定占位符 (MySQL Connector/C++ 通常也支持 ?)
    static std::string placeholder([[maybe_unused]] int index) { return "?"; }
    static std::string placeholder([[maybe_unused]] std::string_view name) { return ":" + std::string(name); }
};

ILIAS_SQL_NS_END
