#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/sqlresult.hpp"

#include <nekoproto/serialization/reflection.hpp>

ILIAS_SQL_NS_BEGIN

template <typename T>
class SqlStatement;

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
    SqlStatement(std::unique_ptr<IStatement> stmt) : mStmt(std::move(stmt)) {}
    SqlStatement(const SqlStatement &)            = delete;
    SqlStatement(SqlStatement &&)                 = default;
    SqlStatement &operator=(const SqlStatement &) = delete;
    SqlStatement &operator=(SqlStatement &&)      = default;
    ~SqlStatement()                               = default;

    auto operator->() -> IStatement * { return mStmt.get(); }
    auto operator->() const -> const IStatement * { return mStmt.get(); }
    auto operator*() -> IStatement & { return *mStmt; }
    auto operator*() const -> const IStatement & { return *mStmt; }

    template <typename U>
        requires(!std::is_class_v<U>) ||
                (std::is_class_v<U> && NEKO_NAMESPACE::detail::has_values_meta<std::decay_t<U>>)
    auto bind(U &arg) -> IoResult<void>;
    template <typename... Args>
        requires(sizeof...(Args) > 1) || ((sizeof...(Args) == 1) && !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>)
    auto bind(Args &&...args) -> IoResult<void>;
    template <typename U = void>
    auto query() -> IoTask<SqlResult<U>>;
    auto execute() -> IoTask<size_t>;
    auto reset() -> void;

private:
    std::unique_ptr<IStatement> mStmt;
};

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
    SqlStatement(std::unique_ptr<IStatement> stmt) : SqlStatement<void>(std::move(stmt)) {}
    SqlStatement(const SqlStatement &)            = delete;
    SqlStatement(SqlStatement &&)                 = default;
    SqlStatement &operator=(const SqlStatement &) = delete;
    SqlStatement &operator=(SqlStatement &&)      = default;
    ~SqlStatement()                               = default;

    using SqlStatement<void>::operator->;
    using SqlStatement<void>::operator*;
    using SqlStatement<void>::bind;
    using SqlStatement<void>::query;
    using SqlStatement<void>::execute;
    using SqlStatement<void>::reset;
    auto query() -> IoTask<SqlResult<T>> { return SqlStatement<void>::template query<T>(); }
    auto bind(T &&arg) -> IoResult<void>;
};

template <typename U>
    requires(!std::is_class_v<U>) || (std::is_class_v<U> && NEKO_NAMESPACE::detail::has_values_meta<std::decay_t<U>>)
auto SqlStatement<void>::bind(U &arg) -> IoResult<void> {
    IoResult<void> ret = {};

    NEKO_NAMESPACE::Reflect<U>::forEach(std::forward<U>(arg), [&ret, this](auto &field, std::string_view name) {
        // ILIAS_INFO("ilias-sql", "Binding field {} with {}", name, field);
        ret = ret ? mStmt->bind(name, to_sql_value_view(field)) : ret;
    });
    return ret;
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || ((sizeof...(Args) == 1) && !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>)
auto SqlStatement<void>::bind(Args &&...args) -> IoResult<void> {
    IoResult<void> ret = {};
    int            idx = 0;
    ((ret = ret ? mStmt->bind(++idx, to_sql_value_view(std::forward<Args>(args))) : ret), ...);
    return ret;
}

template <typename U>
auto SqlStatement<void>::query() -> IoTask<SqlResult<U>> {
    ILIAS_INFO("ilias-sql", "Executing query");
    auto ret = co_await mStmt->query();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return SqlResult<U>(std::move(ret.value()));
}

inline auto SqlStatement<void>::execute() -> IoTask<size_t> {
    auto ret = co_await mStmt->execute();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return ret.value();
}

inline auto SqlStatement<void>::reset() -> void {
    mStmt->reset();
}

template <typename T>
auto SqlStatement<T>::bind(T &&arg) -> IoResult<void> {
    if constexpr (NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>>) {
        return std::apply(
            [this](auto... args) { return SqlStatement<void>::bind(std::forward<decltype(args)>(args)...); }, arg);
    }
    else {
        return SqlStatement<void>::bind(arg);
    }
}
ILIAS_SQL_NS_END
