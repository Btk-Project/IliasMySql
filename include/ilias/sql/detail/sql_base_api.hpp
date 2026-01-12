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
namespace detail {
/**
 * @brief 使用CRTP模式，提供通用的SQL API实现
 *
 * 任何继承自该类的派生类，只需要实现基础的 prepare, query, execute 方法，
 * 就能自动获得 prepare_with, query_with, execute_with 等更高级的API。
 * @tparam Derived 派生类类型 (例如 SqlDatabase 或 SqlTransaction)
 */
template <typename Derived>
class SqlApiMixin {
public:
    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
    auto prepare_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
        -> IoTask<SqlStatement<std::tuple<Args...>>> {
        Derived *self = static_cast<Derived *>(this);
        auto     ret  = co_await self->template prepare<Args...>(query.sql); // 调用派生类的prepare
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
    auto prepare_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlStatement<std::decay_t<U>>> {
        Derived *self = static_cast<Derived *>(this);
        auto     ret  = co_await self->template prepare<U>(query.sql); // 调用派生类的prepare
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        ret.value().bind(std::forward<U>(arg));
        co_return std::move(ret.value());
    }

    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
    auto query_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
        -> IoTask<SqlResult<void>> {
        ILIAS_TRACE("ilias-sql", "Executing query {} with args", query.sql);
        auto ret = co_await this->prepare_with(query, std::forward<Args>(args)...); // 调用本Mixin的prepare_with
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
    auto query_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlResult<void>> {
        auto ret = co_await this->prepare_with(query, std::forward<U>(arg)); // 调用本Mixin的prepare_with
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
    auto execute_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args) -> IoTask<size_t> {
        auto ret = co_await this->prepare_with(query, std::forward<Args>(args)...); // 调用本Mixin的prepare_with
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
    auto execute_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<size_t> {
        auto ret = co_await this->prepare_with(query, std::forward<U>(arg)); // 调用本Mixin的prepare_with
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
        requires(sizeof...(Args) > 0) &&
                ((sizeof...(Args) > 1) ||
                 ((!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && !std::is_void_v<Args>) && ... &&
                  !NEKO_NAMESPACE::detail::is_std_tuple_v<Args...>))
    auto query(std::string_view query) -> IoTask<SqlResult<std::tuple<Args...>>> {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            co_return Unexpected(conn_ret.error());
        }
        auto ret = co_await (*conn_ret)->query(query);
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        co_return SqlResult<std::tuple<Args...>>(std::move(ret.value()));
    }

    template <typename T = void>
        requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>> ||
                 NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>> || std::is_void_v<T>)
    auto query(std::string_view query) -> IoTask<SqlResult<T>> {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            co_return Unexpected(conn_ret.error());
        }
        auto ret = co_await (*conn_ret)->query(query);
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
    auto prepare(std::string_view query) -> IoTask<SqlStatement<std::tuple<Args...>>> {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            co_return Unexpected(conn_ret.error());
        }
        auto ret = co_await (*conn_ret)->prepare(query);
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        co_return SqlStatement<std::tuple<std::decay_t<Args>...>>(std::move(ret.value()));
    }

    template <typename T = void>
        requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>> ||
                 NEKO_NAMESPACE::detail::is_std_tuple_v<std::decay_t<T>> || std::is_void_v<T>)
    auto prepare(std::string_view query) -> IoTask<SqlStatement<T>> {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            co_return Unexpected(conn_ret.error());
        }
        auto ret = co_await (*conn_ret)->prepare(query);
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        co_return SqlStatement<T>(std::move(ret.value()));
    }

    auto execute(std::string_view query) -> IoTask<size_t> {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            co_return Unexpected(conn_ret.error());
        }
        co_return co_await (*conn_ret)->execute(query);
    }

    auto sqlname() -> std::string {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            return "unknown";
        }
        return (*conn_ret)->sqlname();
    }
    
    auto sqlinfo() -> std::string {
        Derived *self     = static_cast<Derived *>(this);
        auto     conn_ret = self->connection();
        if (!conn_ret) {
            return "unknown";
        }
        return (*conn_ret)->sqlinfo();
    }
};
} // namespace detail
ILIAS_SQL_NS_END