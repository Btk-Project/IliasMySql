/**
 * @file mysql.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief mysql I/O
 * @version 0.1
 * @date 2024-12-31
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <ilias/io/context.hpp>
#include <ilias/io/fd_utils.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/net/poller.hpp>
#include <ilias/net/sockfd.hpp>
#include <ilias/task/when_any.hpp>
#include <mariadb/mysql.h>

#include "global.hpp"
#include "sqlopt.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql/driver_registry.hpp"

ILIAS_SQL_NS_BEGIN
class DriverManager;
ILIAS_SQL_NS_END

ILIAS_MYSQL_NS_BEGIN
namespace detail {

class ILIAS_SQL_API MySql final {
public:
    using SqlError = sql::SqlError;
    enum ShutdownType {
        SHUTDOWN_DEFAULT = ::mysql_enum_shutdown_level::SHUTDOWN_DEFAULT,
        KILL_QUERY       = ::mysql_enum_shutdown_level::KILL_QUERY,
        KILL_CONNECTION  = ::mysql_enum_shutdown_level::KILL_CONNECTION
    };

    enum ServerOption {
        MULTI_STATEMENTS_ON  = ::enum_mysql_set_option::MYSQL_OPTION_MULTI_STATEMENTS_ON,
        MULTI_STATEMENTS_OFF = ::enum_mysql_set_option::MYSQL_OPTION_MULTI_STATEMENTS_OFF,
    };

public:
    MySql();
    MySql(const MySql &) = delete;
    ~MySql();

    auto native() -> MYSQL *;
    // connect
    auto connect(std::string_view host, std::string_view user, std::string_view passwd, std::string_view db,
                 int port = 3306, std::string_view unix_socket = "", unsigned long client_flag = 0) -> IoTask<void>;
    auto resetConnection() -> IoTask<int>;
    auto disconnect() -> IoTask<void>;
    auto changeUser(std::string_view user, std::string_view passwd, std::string_view db) -> IoTask<bool>;
    auto dumpDebugInfo() -> IoTask<int>;
    auto setServerOption(ServerOption option) -> IoTask<int>;
    auto setCharacterSet(std::string_view csname) -> IoTask<int>;

    // mysql database
    auto selectDb(std::string_view db) -> IoTask<int>;

    auto query(std::string_view sql) -> IoTask<int>;
    auto commit() -> IoTask<bool>;
    auto autoCommit(bool autoMode) -> IoTask<bool>;

    auto rollback() -> IoTask<bool>;
    auto syncRollback() -> bool;
    auto listFields(std::string_view table, std::string_view wildcard = "*") -> IoTask<MYSQL_RES *>;
    auto sendQuery(std::string_view sql) -> IoTask<int>;
    auto refresh(uint32_t refreshOptions) -> IoTask<int>; // FIXME: where has defines abort options?
    auto kill(uint64_t pid) -> IoTask<int>;               // FIXME: is kill self?
    auto ping() -> IoTask<int>;
    auto stat() -> IoTask<const char *>;
    auto readQueryResult() -> IoTask<bool>;

    // useResult -> fetchRow -> freeResult
    auto useResult() -> IoTask<MYSQL_RES *>;
    auto nextResult() -> IoTask<int>;
    // store result might use too much memory in retrieving a large result set all at once.
    auto storeResult() -> IoTask<MYSQL_RES *>;
    auto fieldCount() -> std::size_t;

    // shutdown
    auto shutdown(ShutdownType shutdownType) -> IoTask<void>;
    auto pollStatus(int &status, uint32_t pollEvents = 0) -> IoTask<void>;

    // stmt
    auto stmtInit() -> MYSQL_STMT *;
    auto setOpt(sqlopt::OptionBase *opt) -> int;
    auto getOpt(sqlopt::OptionBase *opt) -> int;
    auto close() -> void;
    auto lastError() -> int;
    auto lastErrorMessage() -> const char *;

    bool operator==(MySql &other);

private:
    IoContext *mCtxt = nullptr;
    MYSQL      mMysql;
    Poller     mPoller;
};
} // namespace detail

ILIAS_SQL_USE_NAMESPACE
ILIAS_SQL_API void ilias_register_sql_plugin(DriverManager *manager);

ILIAS_MYSQL_NS_END
