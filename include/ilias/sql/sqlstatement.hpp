/**
 * @file sqlstatement.hpp
 * @brief Prepared statement wrapper with type-safe parameter binding
 *
 * This file provides the SqlStatement template class for handling
 * prepared SQL statements with type-safe parameter binding using reflection.
 */

#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/detail/reflection_metadata.hpp"
#include "ilias/sql/detail/type_traits.hpp"
#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/sqlresult.hpp"

#include <nekoproto/serialization/reflection.hpp>

ILIAS_SQL_NS_BEGIN

/**
 * @brief Forward declaration of SqlStatement template
 */
template <typename T>
class SqlStatement;

/**
 * @brief Specialization for void statements (no type mapping)
 *
 * This specialization is used when no type mapping is required,
 * allowing direct access to the underlying IStatement.
 */
template <>
class SqlStatement<void> {
public:
    using value_type      = void;
    using reference       = void;
    using const_reference = void;
    using pointer         = void;
    using const_pointer   = void;

public:
    SqlStatement() = default;
    explicit SqlStatement(std::unique_ptr<IStatement> stmt) : mStmt(std::move(stmt)) {}
    SqlStatement(const SqlStatement &)                = delete;
    SqlStatement &operator=(const SqlStatement &)     = delete;
    SqlStatement(SqlStatement &&) noexcept            = default;
    SqlStatement &operator=(SqlStatement &&) noexcept = default;
    ~SqlStatement()                                   = default;

    auto operator->() -> IStatement * { return mStmt.get(); }
    auto operator->() const -> const IStatement * { return mStmt.get(); }
    auto operator*() -> IStatement & { return *mStmt; }
    auto operator*() const -> const IStatement & { return *mStmt; }
    /**
     * @brief 绑定反射对象，wrapper 会保存一份参数存储以覆盖后续 query()/execute()。
     *
     * 这比直接调用 IStatement::bind() 更适合临时值和表达式结果；如绑定了大对象，
     * 可在执行完成且不再复用这些参数后调用 clearKeepAlives() 释放保存的副本。
     */
    template <typename U>
        requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
    auto bind(U &&arg) -> IoResult<void>;
    /**
     * @brief 绑定位置参数，wrapper 会按值保存参数副本以保证绑定生命周期。
     */
    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<Args> && ... &&
                                          !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>)
    auto bind(Args &&...args) -> IoResult<void>;
    template <typename U = void>
    auto query() -> IoTask<SqlResult<U>>;
    auto execute() -> IoTask<size_t>;
    auto reset() -> void;
    auto clearKeepAlives() -> void;
    auto lastNativeError() const -> std::optional<NativeSqlError>;

private:
    using DeleterFunc = void (*)(void *);

    std::unique_ptr<IStatement>                     mStmt;
    std::vector<std::unique_ptr<void, DeleterFunc>> mKeepAlive; // to keep the rvalue objects alive

    template <typename T>
    friend class SqlStatement;
};

/**
 * @brief Typed statement wrapper with automatic type mapping
 *
 * This specialization provides type-safe statement execution,
 * automatically binding parameters and mapping results to C++ types.
 *
 * @tparam T The type to bind parameters from and map results to
 */
template <typename T>
class SqlStatement : public SqlStatement<void> {
public:
    using value_type      = T;
    using reference       = T &;
    using const_reference = const T &;
    using pointer         = T *;
    using const_pointer   = const T *;

public:
    SqlStatement() = default;
    explicit SqlStatement(std::unique_ptr<IStatement> stmt) : SqlStatement<void>(std::move(stmt)) {}
    template <typename U>
    explicit SqlStatement(SqlStatement<U> &&other) noexcept {
        mStmt      = std::move(other.mStmt);
        mKeepAlive = std::move(other.mKeepAlive);
    }
    SqlStatement(const SqlStatement &)                = delete;
    SqlStatement &operator=(const SqlStatement &)     = delete;
    SqlStatement(SqlStatement &&) noexcept            = default;
    SqlStatement &operator=(SqlStatement &&) noexcept = default;

    ~SqlStatement() = default;

    using SqlStatement<void>::operator->;
    using SqlStatement<void>::operator*;
    using SqlStatement<void>::bind;
    using SqlStatement<void>::query;
    using SqlStatement<void>::execute;
    using SqlStatement<void>::reset;
    auto query() -> IoTask<SqlResult<T>> { return SqlStatement<void>::template query<T>(); }
    auto bind(T &&arg) -> IoResult<void>;

    template <typename U>
    friend class SqlStatement;
};

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlStatement<void>::bind(U &&arg) -> IoResult<void> {
    IoResult<void> ret = {};
    using KeepAliveT   = std::tuple<StorageType_t<U>>;
    auto keepAlive     = new KeepAliveT(std::forward<U>(arg));
    // clang-format off
    NEKO_NAMESPACE::Reflect<std::decay_t<U>>::forEach(std::get<0>(*keepAlive), 
        [&ret, this](auto &field, std::string_view name, const auto &tags) {
            // ILIAS_INFO("ilias-sql", "Binding field {} with {}", name, field);
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                if (ret) {
                    using FieldType = std::decay_t<decltype(field)>;
                    ret = SqlBinder<FieldType>::bind(*mStmt, detail::reflectedFieldName(name, tags), field);
                }
            }
        });
    // clang-format on
    mKeepAlive.emplace_back(
        std::unique_ptr<void, DeleterFunc>(keepAlive, [](void *ptr) { delete static_cast<KeepAliveT *>(ptr); }));
    return ret;
}

template <typename... Args>
    requires(sizeof...(Args) > 1) ||
            (!NEKO_NAMESPACE::detail::has_names_meta<Args> && ... && !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>)
auto SqlStatement<void>::bind(Args &&...args) -> IoResult<void> {
    using KeepAliveTuple     = std::tuple<StorageType_t<Args>...>;
    auto           keepAlive = new KeepAliveTuple(std::forward<Args>(args)...);
    IoResult<void> ret       = {};
    [this, &ret, keepAlive]<size_t... I>(std::index_sequence<I...>) {
        int idx = 0;
        ((ret = ret ? SqlBinder<std::decay_t<decltype(std::get<I>(*keepAlive))>>::bind(
                          *mStmt, ++idx, std::get<I>(*keepAlive))
                    : ret), ...);
    }(std::make_index_sequence<sizeof...(Args)>());
    mKeepAlive.emplace_back(
        std::unique_ptr<void, DeleterFunc>(keepAlive, [](void *ptr) { delete static_cast<KeepAliveTuple *>(ptr); }));
    return ret;
}

template <typename U>
auto SqlStatement<void>::query() -> IoTask<SqlResult<U>> {
    ILIAS_TRACE("ilias-sql", "Executing query");
    ILIAS_CO_TRY(auto ret, co_await mStmt->query());
    co_return SqlResult<U>(std::move(ret));
}

inline auto SqlStatement<void>::execute() -> IoTask<size_t> {
    ILIAS_CO_TRY(auto ret, co_await mStmt->execute());
    co_return ret;
}

inline auto SqlStatement<void>::reset() -> void {
    mStmt->reset();
}

inline auto SqlStatement<void>::clearKeepAlives() -> void {
    mKeepAlive.clear();
}

inline auto SqlStatement<void>::lastNativeError() const -> std::optional<NativeSqlError> {
    if (!mStmt) {
        return std::nullopt;
    }
    return mStmt->lastNativeError();
}

template <typename T>
auto SqlStatement<T>::bind(T &&arg) -> IoResult<void> {
    if constexpr (NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>>) {
        return std::apply(
            [this](auto &&...args) { return SqlStatement<void>::bind(std::forward<decltype(args)>(args)...); }, arg);
    }
    else {
        return SqlStatement<void>::bind(std::forward<T>(arg));
    }
}
ILIAS_SQL_NS_END
