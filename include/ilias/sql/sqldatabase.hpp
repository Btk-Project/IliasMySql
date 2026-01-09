#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>
#include <string_view>

#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "ilias/sql/detail/type_traits.hpp"

ILIAS_SQL_NS_BEGIN

class SqlTransaction;
class ILIAS_SQL_API SqlDatabase {
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
    template <typename... Args>
        requires(sizeof...(Args) > 0) &&
                ((sizeof...(Args) > 1) ||
                 ((!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && !std::is_void_v<Args>) && ... &&
                  !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>))
    auto query(std::string_view query) -> IoTask<SqlResult<std::tuple<Args...>>>;
    template <typename T = void>
        requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>> ||
                 NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>> || std::is_void_v<T>)
    auto query(std::string_view query) -> IoTask<SqlResult<T>>;
    template <typename... Args>
        requires(sizeof...(Args) > 0) &&
                ((sizeof...(Args) > 1) ||
                 ((!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && !std::is_void_v<Args>) && ... &&
                  !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>))
    auto prepare(std::string_view query) -> IoTask<SqlStatement<std::tuple<Args...>>>;
    template <typename T = void>
        requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>> ||
                 NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>> || std::is_void_v<T>)
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

class ILIAS_SQL_API SqlTransaction {
    enum class State {
        kUnused = 0,
        kBeginned,
        kCommitted,
        kRolledBack,
    };

public:
    SqlTransaction(SqlDatabase &db) : mDatabase(db) {}
    SqlTransaction(const SqlTransaction &)            = delete;
    SqlTransaction &operator=(const SqlTransaction &) = delete;
    SqlTransaction(SqlTransaction &&othre) : mDatabase(othre.mDatabase), mState(othre.mState) {
        othre.mState = State::kUnused;
    }
    SqlTransaction &operator=(SqlTransaction &&) = delete;
    ~SqlTransaction();

    auto commit() -> IoTask<void>;
    auto rollback() -> IoTask<void>;

    auto execute(std::string_view query) -> IoTask<size_t>;
    template <typename... Args>
    auto query(std::string_view query)
        -> IoTask<SqlResult<std::conditional_t<sizeof...(Args) == 0, void, std::tuple<Args...>>>>;
    template <typename... Args>
    auto prepare(std::string_view query)
        -> IoTask<SqlStatement<std::conditional_t<sizeof...(Args) == 0, void, std::tuple<Args...>>>>;

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

protected:
    auto begin() -> IoTask<void>;

    friend class SqlDatabase;

private:
    SqlDatabase &mDatabase;
    State        mState = State::kUnused;
};

template <typename... Args>
    requires(sizeof...(Args) > 0) &&
            ((sizeof...(Args) > 1) ||
             ((!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && !std::is_void_v<Args>) && ... &&
              !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>))
auto SqlDatabase::query(std::string_view query) -> IoTask<SqlResult<std::tuple<Args...>>> {
    auto ret = co_await mConnection->query(query);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return SqlResult<std::tuple<Args...>>(std::move(ret.value()));
}

template <typename T>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>> ||
             NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>> || std::is_void_v<T>)
auto SqlDatabase::query(std::string_view query) -> IoTask<SqlResult<T>> {
    auto ret = co_await mConnection->query(query);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return SqlResult<T>(std::move(ret.value()));
}

template <typename... Args>
    requires(sizeof...(Args) > 0) &&
            ((sizeof...(Args) > 1) ||
             ((!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && !std::is_void_v<Args>) && ... &&
              !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>))
auto SqlDatabase::prepare(std::string_view query) -> IoTask<SqlStatement<std::tuple<Args...>>> {
    auto ret = co_await mConnection->prepare(query);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return SqlStatement<std::tuple<std::decay_t<Args>...>>(std::move(ret.value()));
}

template <typename T>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>> ||
             NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>> || std::is_void_v<T>)
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
    if constexpr (sizeof...(Args) > 0) {
        ret.value().bind(std::forward<Args>(args)...);
    }
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
    ILIAS_TRACE("ilias-sql", "Executing query {} with args", query.sql);
    auto ret = co_await prepare_with<Args...>(query, std::forward<Args>(args)...);
    if (!ret) {
        ILIAS_TRACE("ilias-sql", "Failed to prepare query {} with args", query.sql);
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().query();
    if (!ret1) {
        ILIAS_TRACE("ilias-sql", "Failed to execute query {} with args", query.sql);
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
        ILIAS_TRACE("ilias-sql", "Failed to prepare query {}", query.sql);
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().execute();
    if (!ret1) {
        ILIAS_TRACE("ilias-sql", "Failed to execute query {}", query.sql);
        co_return Unexpected(ret1.error());
    }
    co_return ret1.value();
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlDatabase::execute_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<size_t> {
    auto ret = co_await prepare_with(query, std::forward<U>(arg));
    if (!ret) {
        ILIAS_TRACE("ilias-sql", "Failed to prepare query {}", query.sql);
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await ret.value().execute();
    if (!ret1) {
        ILIAS_TRACE("ilias-sql", "Failed to execute query {}", query.sql);
    }
    co_return ret1.value();
}

template <typename... Args>
auto SqlTransaction::query(std::string_view query)
    -> IoTask<SqlResult<std::conditional_t<sizeof...(Args) == 0, void, std::tuple<Args...>>>> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    if constexpr (sizeof...(Args) == 0) {
        co_return co_await mDatabase.query<void>(query);
    }
    else {
        co_return co_await mDatabase.query<Args...>(query);
    }
}

template <typename... Args>
auto SqlTransaction::prepare(std::string_view query)
    -> IoTask<SqlStatement<std::conditional_t<sizeof...(Args) == 0, void, std::tuple<Args...>>>> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    if constexpr (sizeof...(Args) == 0) {
        co_return co_await mDatabase.prepare<void>(query);
    }
    else {
        co_return co_await mDatabase.prepare<Args...>(query);
    }
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlTransaction::prepare_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
    -> IoTask<SqlStatement<std::tuple<Args...>>> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.prepare_with(query, std::forward<Args>(args)...);
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlTransaction::prepare_with(SqlStructCheck<std::decay_t<U>> query, U &&arg)
    -> IoTask<SqlStatement<std::decay_t<U>>> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.prepare_with(query, std::forward<U>(arg));
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlTransaction::query_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
    -> IoTask<SqlResult<void>> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.query_with(query, std::forward<Args>(args)...);
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlTransaction::query_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlResult<void>> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.query_with(query, std::forward<U>(arg));
}

template <typename... Args>
    requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlTransaction::execute_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
    -> IoTask<size_t> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.execute_with(query, std::forward<Args>(args)...);
}

template <typename U>
    requires NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<U>>
auto SqlTransaction::execute_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<size_t> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.execute_with(query, std::forward<U>(arg));
}

ILIAS_SQL_NS_END