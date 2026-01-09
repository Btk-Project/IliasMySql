#pragma once

#include <type_traits>
#include <optional>
#include <memory>
#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/detail/coverter.hpp"

ILIAS_SQL_NS_BEGIN
namespace detail {

// =========================================================
// 1. 剥皮器 (Type Stripper)
// =========================================================
template <typename T>
struct strip_wrapper {
    using type = T;
};
template <typename T>
struct strip_wrapper<std::optional<T>> {
    using type = typename strip_wrapper<T>::type;
};
template <typename T>
struct strip_wrapper<std::shared_ptr<T>> {
    using type = typename strip_wrapper<T>::type;
};
template <typename T>
struct strip_wrapper<std::unique_ptr<T>> {
    using type = typename strip_wrapper<T>::type;
};
template <typename T>
using strip_wrapper_t = typename strip_wrapper<std::decay_t<T>>::type;

// =========================================================
// 2. 概念约束 (Concepts)
// =========================================================

// 检测是否存在有效的 SqlBinder 特化
// 注意：这里我们依靠 SFINAE 检查 SqlBinder::bind 是否存在
template <typename T>
concept SqlBindable =
    requires(IStatement &stmt, T &&val) { SqlBinder<std::decay_t<T>>::bind(stmt, 1, std::forward<T>(val)); };

// 检测是否拥有 .sql() 方法 (即是否为另一个 SqlVariable/Column)
template <typename T>
concept HasSqlMethod = requires(T t) {
    { t.sql() } -> std::convertible_to<std::string>;
};

// 兼容性检测：用于 Values 绑定时的类型安全检查
// 如果是 Column vs Column，则不走此检查
template <typename ColumnType, typename ValueType>
concept IsCompatible = std::is_constructible_v<strip_wrapper_t<ColumnType>, strip_wrapper_t<ValueType>> ||
                       std::is_convertible_v<strip_wrapper_t<ValueType>, strip_wrapper_t<ColumnType>> ||
                       std::is_same_v<strip_wrapper_t<ColumnType>, strip_wrapper_t<ValueType>>;

template <size_t N, typename... Ts>
struct select_type_helper;

template <size_t N, typename T, typename... Ts>
struct select_type_helper<N, T, Ts...> {
    using type = typename select_type_helper<N - 1, Ts...>::type;
};

template <typename T, typename... Ts>
struct select_type_helper<0, T, Ts...> {
    using type = T;
};

template <size_t N>
struct select_type_helper<N> {
    using type = void;
};

template <size_t N, typename... Ts>
using select_type_t = typename select_type_helper<N, Ts...>::type;

} // namespace detail
ILIAS_SQL_NS_END