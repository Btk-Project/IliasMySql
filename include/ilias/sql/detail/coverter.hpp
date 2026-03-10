#pragma once

#include "../global/global.hpp"

#include <optional>
#include <type_traits>
#include <ilias/io/error.hpp>

#include "../types.hpp"
#include "../sqlerror.hpp"
#include "../interfaces.hpp"

ILIAS_SQL_NS_BEGIN
// 前置声明
template <typename T, typename Enable = void>
struct SqlBinder;

// ==========================================
// 1. 默认绑定器 (处理基础类型)
// ==========================================
template <typename T>
    requires(!DereferenceableAndNullable<T> && !std::is_enum_v<T>)
struct SqlBinder<T, void> {
    // 按索引绑定
    template <typename U>
    static auto bind(IStatement &stmt, int index, U &&value) -> IoResult<void> {
        return stmt.bind(index, std::forward<U>(value));
    }
    template <typename U>
    static auto bind(IStatement &stmt, std::string_view name, U &&value) -> IoResult<void> {
        return stmt.bind(name, std::forward<U>(value));
    }
};

// ==========================================
// 2. std::optional 特化支持
// ==========================================
template <typename T>
    requires DereferenceableAndNullable<T>
struct SqlBinder<T, void> {
    using InnerType = std::decay_t<decltype(*std::declval<T>())>;
    static auto bind(IStatement &stmt, int index, const T &value) -> IoResult<void> {
        if (value.has_value()) {
            return SqlBinder<InnerType>::bind(stmt, index, *value);
        }
        else {
            return stmt.bind(index, g_sql_null);
        }
    }

    static auto bind(IStatement &stmt, std::string_view name, const T &value) -> IoResult<void> {
        if (value.has_value()) {
            return SqlBinder<InnerType>::bind(stmt, name, *value);
        }
        else {
            return stmt.bind(name, g_sql_null);
        }
    }
};

// ==========================================
// 3. (可选) Enum 枚举支持
// ==========================================
template <typename T>
struct SqlBinder<T, std::enable_if_t<std::is_enum_v<T>>> {
    static auto bind(IStatement &stmt, int index, const T &value) -> IoResult<void> {
        using UnderlyingType = std::underlying_type_t<T>;
        return SqlBinder<UnderlyingType>::bind(stmt, index, static_cast<UnderlyingType>(value));
    }

    static auto bind(IStatement &stmt, std::string_view name, const T &value) -> IoResult<void> {
        using UnderlyingType = std::underlying_type_t<T>;
        return SqlBinder<UnderlyingType>::bind(stmt, name, static_cast<UnderlyingType>(value));
    }
};

ILIAS_SQL_NS_END