#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>
#include <string_view>

#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "ilias/sql/global/type_traits.hpp"

ILIAS_SQL_NS_BEGIN

class SqlTransaction;
class SqlDatabase {
public:
    static auto open(std::string_view name, ConnectOptions options) -> IoTask<SqlDatabase>;
#ifdef ENABLE_SQLITE_PLUGINS
    static auto open_in_memory() -> IoTask<SqlDatabase>;
#endif
    SqlDatabase(std::unique_ptr<IConnection> connection) : mConnection(std::move(connection)) {}
    SqlDatabase(SqlDatabase &&)                 = default;
    SqlDatabase &operator=(SqlDatabase &&)      = default;
    SqlDatabase(const SqlDatabase &)            = delete;
    SqlDatabase &operator=(const SqlDatabase &) = delete;
    ~SqlDatabase();

    auto operator->() -> IConnection * { return mConnection.get(); }
    auto operator->() const -> const IConnection * { return mConnection.get(); }
    auto operator*() -> IConnection & { return *mConnection; }
    auto operator*() const -> const IConnection & { return *mConnection; }

    auto close() -> IoTask<void>;
    auto execute(std::string_view query) -> IoTask<size_t>;
    template <typename T = void>
    auto query(std::string_view query) -> IoTask<SqlResult<T>>;
    template <typename T = void>
    auto prepare(std::string_view query) -> IoTask<SqlStatement<T>>;

    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
    auto prepare_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
        -> IoTask<SqlStatement<std::tuple<Args...>>>;
    template <typename U>
        requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
    auto prepare_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlStatement<std::decay_t<U>>>;
    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
    auto query_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
        -> IoTask<SqlResult<void>>;
    template <typename U>
        requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
    auto query_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlResult<void>>;
    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
    auto execute_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args) -> IoTask<size_t>;
    template <typename U>
        requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
    auto execute_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<size_t>;

    auto transaction() -> IoTask<SqlTransaction>;

private:
    std::unique_ptr<IConnection> mConnection;
};

class SqlTransaction {
    enum class State {
        kUnused = 0,
        kBeginned,
        kCommitted,
        kRolledBack,
    };

public:
    SqlTransaction(SqlDatabase &db) : mDatabase(db) {}
    ~SqlTransaction();

    auto commit() -> IoTask<void>;
    auto rollback() -> IoTask<void>;

    auto execute(std::string_view query) -> IoTask<size_t>;
    template <typename T = void>
    auto query(std::string_view query) -> IoTask<SqlResult<T>>;
    template <typename T = void>
    auto prepare(std::string_view query) -> IoTask<SqlStatement<T>>;

protected:
    auto begin() -> IoTask<void>;

    friend class SqlDatabase;

private:
    SqlDatabase &mDatabase;
    State        mState = State::kUnused;
};

template <typename T>
auto SqlDatabase::query(std::string_view query) -> IoTask<SqlResult<T>> {
    auto ret = co_await mConnection->query(query);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return SqlResult<T>(std::move(ret.value()));
}

template <typename T>
auto SqlDatabase::prepare(std::string_view query) -> IoTask<SqlStatement<T>> {
    auto ret = co_await mConnection->prepare(query);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return SqlStatement<T>(std::move(ret.value()));
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlDatabase::prepare_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
    -> IoTask<SqlStatement<std::tuple<Args...>>> {
    auto ret = co_await prepare(query.sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    ret.value().bind(std::forward<Args>(args)...);
    co_return std::move(ret.value());
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlDatabase::prepare_with(SqlStructCheck<std::decay_t<U>> query, U &&arg)
    -> IoTask<SqlStatement<std::decay_t<U>>> {
    auto ret = co_await prepare<std::decay_t<U>>(query.sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    ret.value().bind(std::forward<U>(arg));
    co_return std::move(ret.value());
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlDatabase::query_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
    -> IoTask<SqlResult<void>> {
    ILIAS_INFO("ilias-sql", "Executing query {} with args", query.sql);
    auto ret = co_await prepare_with<Args...>(query, std::forward<Args>(args)...);
    if (!ret) {
        ILIAS_INFO("ilias-sql", "Failed to prepare query {} with args", query.sql);
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().query();
    if (!ret1) {
        ILIAS_INFO("ilias-sql", "Failed to execute query {} with args", query.sql);
        co_return Unexpected(ret1.error());
    }
    co_return std::move(ret1.value());
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlDatabase::query_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlResult<void>> {
    auto ret = co_await prepare_with<U>(query, std::forward<U>(arg));
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().query();
    if (!ret1) {
        co_return Unexpected(ret1.error());
    }
    co_return SqlResult<void>(std::move(ret1.value()));
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlDatabase::execute_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
    -> IoTask<size_t> {
    auto ret = co_await prepare_with(query, std::forward<Args>(args)...);
    if (!ret) {
        ILIAS_INFO("ilias-sql", "Failed to prepare query {}", query.sql);
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().execute();
    if (!ret1) {
        ILIAS_INFO("ilias-sql", "Failed to execute query {}", query.sql);
    }
    co_return ret1.value();
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlDatabase::execute_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<size_t> {
    auto ret = co_await prepare_with(query, std::forward<U>(arg));
    if (!ret) {
        ILIAS_INFO("ilias-sql", "Failed to prepare query {}", query.sql);
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().execute();
    if (!ret1) {
        ILIAS_INFO("ilias-sql", "Failed to execute query {}", query.sql);
    }
    co_return ret1.value();
}

ILIAS_SQL_NS_END