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
struct SqlValueConverter;

template <typename T>
concept DereferenceableAndNullable = requires(T &t) {
    { t.has_value() } -> std::convertible_to<bool>; // optional-like
    { t.reset() };                                  // optional-like
    { *t };                                         // dereferenceable
};
template <typename T>
struct OptionalLikeType;

template <typename T>
    requires(!DereferenceableAndNullable<T>)
struct OptionalLikeType<T> {
    using type = T;
    static auto to_sql_pointer(T &&val) { return to_sql_pointer(val); }
};

template <typename T>
    requires DereferenceableAndNullable<T>
struct OptionalLikeType<T> {
    using type = std::decay_t<decltype(*std::declval<T>())>;
    static auto to_sql_pointer(T &&val) { return to_sql_pointer(*val); }
};

// ==========================================
// 1. 默认转换器 (处理基础类型)
// ==========================================
// 这里搬运了你原本 unpack 函数中的 switch 逻辑
template <typename T>
    requires(!DereferenceableAndNullable<T> && !std::is_enum_v<T>)
struct SqlValueConverter<T, void> {
    template <typename U>
    static auto convert(SqlValueView &ret, U &value) -> IoResult<void> {
        // 如果目标类型不是 optional，但数据库返回了 Null，通常视为错误或忽略
        // 这里为了严谨，如果是非 optional 的类型遇到 null，报参数错误
        if (ret.index() == (size_t)SqlValueType::kNull) {
            if (std::is_same_v<SqlValueTraits<SqlValueType::kNull>::type, T> ||
                std::is_constructible_v<T, SqlValueTraits<SqlValueType::kText>::type> ||
                std::is_constructible_v<T, SqlValueTraits<SqlValueType::kBlob>::type> ||
                std::is_constructible_v<T, SqlValueTraits<SqlValueType::kText>::viewType> ||
                std::is_constructible_v<T, SqlValueTraits<SqlValueType::kBlob>::viewType>) {
                value = T {};
                return {};
            }
            return Unexpected(SqlError::NullValue);
        }

        switch ((SqlValueType)ret.index()) {
            case SqlValueType::kChar: {
                if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kChar>::type, T>) {
                    value = get<SqlValueType::kChar>(ret);
                    return {};
                }
                break;
            }
            case SqlValueType::kBool: {
                if constexpr (std::is_same_v<T, bool>) {
                    value = get<SqlValueType::kBool>(ret);
                    return {};
                }
            }
            case SqlValueType::kInt: {
                if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kInt>::type, T>) {
                    value = get<SqlValueType::kInt>(ret);
                    return {};
                }
                break;
            }
            case SqlValueType::kBigInt: {
                if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kBigInt>::type, T>) {
                    value = get<SqlValueType::kBigInt>(ret);
                    return {};
                }
                break;
            }
            case SqlValueType::kFloat: {
                if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kFloat>::type, T>) {
                    value = get<SqlValueType::kFloat>(ret);
                    return {};
                }
                break;
            }
            case SqlValueType::kDouble: {
                if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kDouble>::type, T>) {
                    value = get<SqlValueType::kDouble>(ret);
                    return {};
                }
                break;
            }
            case SqlValueType::kText: {
                if constexpr (std::is_constructible_v<T, SqlValueTraits<SqlValueType::kText>::viewType>) {
                    value = T(get<SqlValueType::kText>(ret));
                    return {};
                }
                else if constexpr (std::is_constructible_v<T, SqlValueTraits<SqlValueType::kText>::type>) {
                    value = T(get<SqlValueType::kText>(ret));
                    return {};
                }
                break;
            }
            case SqlValueType::kBlob: {
                if constexpr (std::is_constructible_v<T, SqlValueTraits<SqlValueType::kBlob>::viewType>) {
                    value = T(get<SqlValueType::kBlob>(ret));
                    return {};
                }
                else if constexpr (std::is_constructible_v<T, SqlValueTraits<SqlValueType::kBlob>::type>) {
                    auto blob_data = get<SqlValueType::kBlob>(ret);
                    value          = T(std::begin(blob_data), std::end(blob_data));
                    return {};
                }
                break;
            }
            case SqlValueType::kDate: {
                if constexpr (std::is_constructible_v<T, SqlValueTraits<SqlValueType::kDate>::type>) {
                    value = T(std::move(get<SqlValueType::kDate>(ret)));
                    return {};
                }
                break;
            }
            default:
                break;
        }
        // 如果类型不匹配或未处理
        return Unexpected(make_error_code(std::errc::invalid_argument));
    }
};

// ==========================================
// 2. std::optional 特化支持
// ==========================================
template <typename T>
    requires DereferenceableAndNullable<T>
struct SqlValueConverter<T, void> {
    using InnerType = std::decay_t<decltype(*std::declval<T>())>;
    static auto convert(SqlValueView &ret, T &value) -> IoResult<void> {
        // 1. 处理 Null 情况
        if (ret.index() == (size_t)SqlValueType::kNull) {
            value.reset();
            return {};
        }

        // 2. 如果不是 Null，创建一个临时的 T，调用 T 的转换器
        InnerType temp_val;
        // 递归调用基础类型的转换逻辑
        auto res = SqlValueConverter<InnerType>::convert(ret, temp_val);

        if (res) {
            value = std::move(temp_val);
            return {};
        }
        else {
            // 如果转换失败（比如类型不匹配），返回错误
            return res;
        }
    }
};

// ==========================================
// 3. (可选) Enum 枚举类型的通用支持
// ==========================================
// 如果 T 是枚举，且不是 SqlValue 原生支持的类型，尝试将其作为底层类型(int/int64)处理
template <typename T>
struct SqlValueConverter<T, std::enable_if_t<std::is_enum_v<T>>> {
    static auto convert(SqlValueView &ret, T &value) -> IoResult<void> {
        using UnderlyingType = std::underlying_type_t<T>;
        UnderlyingType temp_val;
        auto           res = SqlValueConverter<UnderlyingType>::convert(ret, temp_val);
        if (res) {
            value = static_cast<T>(temp_val);
        }
        return res;
    }
};

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
        return stmt.bind(index, to_sql_pointer(std::forward<U>(value)));
    }
    template <typename U>
    static auto bind(IStatement &stmt, std::string_view name, U &&value) -> IoResult<void> {
        return stmt.bind(name, to_sql_pointer(std::forward<U>(value)));
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
            return stmt.bind(index, &g_sql_null);
        }
    }

    static auto bind(IStatement &stmt, std::string_view name, const T &value) -> IoResult<void> {
        if (value.has_value()) {
            return SqlBinder<InnerType>::bind(stmt, name, *value);
        }
        else {
            return stmt.bind(name, &g_sql_null);
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