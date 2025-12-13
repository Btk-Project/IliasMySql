#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/global/type_traits.hpp"
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
    template <typename U>
        requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
    auto bind(U &&arg) -> IoResult<void>;
    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<Args> && ... &&
                                          !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>)
    auto bind(Args &&...args) -> IoResult<void>;
    template <typename U = void>
    auto query() -> IoTask<SqlResult<U>>;
    auto execute() -> IoTask<size_t>;
    auto reset() -> void;
    auto clearKeepAlives() -> void;

private:
    using DeleterFunc = void (*)(void *);

    std::unique_ptr<IStatement>                     mStmt;
    std::vector<std::unique_ptr<void, DeleterFunc>> mKeepAlive; // to keep the rvalue objects alive

    template <typename T>
    friend class SqlStatement;
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
        [&ret, this](auto &field, std::string_view name) {
            // ILIAS_INFO("ilias-sql", "Binding field {} with {}", name, field);
            if (!ret) {
                return;
            }
            using FieldType = std::decay_t<decltype(field)>;
            ret = SqlBinder<FieldType>::bind(*mStmt, name, field);
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

inline auto SqlStatement<void>::clearKeepAlives() -> void {
    mKeepAlive.clear();
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
