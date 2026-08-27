#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>
#include <string_view>
#include <ilias/result.hpp>

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
        requires(sizeof...(Args) > 1) || (!nekoproto::NamedReflectable<std::decay_t<Args>> && ...)
    auto prepare_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
        -> IoTask<SqlStatement<std::tuple<Args...>>> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto ret, co_await self->template prepare<Args...>(query.sql)); // 调用派生类的prepare
        if constexpr (sizeof...(Args) > 0) {
            ILIAS_CO_TRYV(ret.bind(std::forward<Args>(args)...));
        }
        co_return SqlStatement<std::tuple<Args...>>(std::move(ret));
    }

    template <typename U>
        requires nekoproto::NamedReflectable<std::decay_t<U>>
    auto prepare_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlStatement<std::decay_t<U>>> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto ret, co_await self->template prepare<U>(query.sql)); // 调用派生类的prepare
        ILIAS_CO_TRYV(ret.bind(std::forward<U>(arg)));
        co_return ret;
    }

    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!nekoproto::NamedReflectable<std::decay_t<Args>> && ...)
    auto query_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args)
        -> IoTask<SqlResult<void>> {
        ILIAS_TRACE("ilias-sql", "Executing query {} with args", query.sql);
        ILIAS_CO_TRY(auto prepare_ret,
                     co_await this->prepare_with(query, std::forward<Args>(args)...)); // 调用本Mixin的prepare_with
        ILIAS_CO_TRY(auto query_ret, co_await prepare_ret.query());
        co_return query_ret;
    }

    template <typename U>
        requires nekoproto::NamedReflectable<std::decay_t<U>>
    auto query_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlResult<void>> {
        ILIAS_CO_TRY(auto ret, co_await this->prepare_with(query, std::forward<U>(arg))); // 调用本Mixin的prepare_with
        ILIAS_CO_TRY(auto ret1, co_await ret.query());
        co_return SqlResult<void>(std::move(ret1));
    }

    template <typename... Args>
        requires(sizeof...(Args) > 1) || (!nekoproto::NamedReflectable<std::decay_t<Args>> && ...)
    auto execute_with(SqlCheck<std::tuple<std::type_identity_t<Args>...>> query, Args &&...args) -> IoTask<size_t> {
        ILIAS_CO_TRY(auto ret,
                     co_await this->prepare_with(query, std::forward<Args>(args)...)); // 调用本Mixin的prepare_with
        ILIAS_CO_TRY(auto ret1, co_await ret.execute());
        co_return ret1;
    }

    template <typename U>
        requires nekoproto::NamedReflectable<std::decay_t<U>>
    auto execute_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<size_t> {
        ILIAS_CO_TRY(auto ret, co_await this->prepare_with(query, std::forward<U>(arg))); // 调用本Mixin的prepare_with
        ILIAS_CO_TRY(auto ret1, co_await ret.execute());
        co_return ret1;
    }

    template <typename... Args>
        requires(sizeof...(Args) > 0) &&
                ((sizeof...(Args) > 1) ||
                 (((!nekoproto::NamedReflectable<std::decay_t<Args>> && !std::is_void_v<Args>) && ...) &&
                  (!nekoproto::TupleLike<Args> && ...)))
    auto query(std::string_view query) -> IoTask<SqlResult<std::tuple<Args...>>> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto conn_ret, self->connection());
        ILIAS_CO_TRY(auto ret, co_await conn_ret->query(query));
        co_return SqlResult<std::tuple<Args...>>(std::move(ret));
    }

    template <typename T = void>
        requires(nekoproto::NamedReflectable<std::decay_t<T>> || nekoproto::TupleLike<std::decay_t<T>> ||
                 std::is_void_v<T>)
    auto query(std::string_view query) -> IoTask<SqlResult<T>> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto conn_ret, self->connection());
        ILIAS_CO_TRY(auto ret, co_await conn_ret->query(query));
        co_return SqlResult<T>(std::move(ret));
    }

    template <typename... Args>
        requires(sizeof...(Args) > 0) &&
                ((sizeof...(Args) > 1) ||
                 (((!nekoproto::NamedReflectable<std::decay_t<Args>> && !std::is_void_v<Args>) && ...) &&
                  (!nekoproto::TupleLike<Args> && ...)))
    auto prepare(std::string_view query) -> IoTask<SqlStatement<std::tuple<Args...>>> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto conn_ret, self->connection());
        ILIAS_CO_TRY(auto ret, co_await conn_ret->prepare(query));
        co_return SqlStatement<std::tuple<Args...>>(std::move(ret));
    }

    template <typename T = void>
        requires(nekoproto::NamedReflectable<std::decay_t<T>> || nekoproto::TupleLike<std::decay_t<T>> ||
                 std::is_void_v<T>)
    auto prepare(std::string_view query) -> IoTask<SqlStatement<T>> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto conn_ret, self->connection());
        ILIAS_CO_TRY(auto ret, co_await conn_ret->prepare(query));
        co_return SqlStatement<T>(std::move(ret));
    }

    auto execute(std::string_view query) -> IoTask<size_t> {
        Derived *self = static_cast<Derived *>(this);
        ILIAS_CO_TRY(auto conn_ret, self->connection());
        co_return co_await conn_ret->execute(query);
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
