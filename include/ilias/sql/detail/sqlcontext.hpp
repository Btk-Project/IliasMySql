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

#include "detail/global.hpp"

#include <ilias/io/context.hpp>
#include <ilias/io/fd_utils.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/net/poller.hpp>
#include <ilias/net/sockfd.hpp>
#include <ilias/task/when_any.hpp>
#include <map>

#include "sqlerror.hpp"
#include "sqlopt.hpp"

ILIAS_SQL_NS_BEGIN
class ISqlStmt;

class ISqlContext {
public:
    ISqlContext();
    ISqlContext(const ISqlContext &) = delete;
    ~ISqlContext();

    virtual auto connect(const std::map<std::string, std::string> &params = {}) -> IoTask<void> = 0;
    virtual auto resetConnection() -> IoTask<int>                                               = 0;
    virtual auto disconnect() -> IoTask<void>                                                   = 0;
    virtual auto changeUser(const std::map<std::string, std::string> &params) -> IoTask<bool>   = 0;
    virtual auto dumpDebugInfo() -> IoTask<void>                                                = 0;

    virtual auto selectDb(std::string_view db) -> IoTask<void> = 0;
    virtual auto query(std::string_view sql) -> IoTask<void>   = 0;
    virtual auto commit() -> IoTask<void>                      = 0;
    virtual auto autoCommit(bool autoMode) -> IoTask<void>     = 0;

    virtual auto rollback() -> IoTask<void>;
    virtual auto sendQuery(std::string_view sql) -> IoTask<void>;
    virtual auto ping() -> IoTask<int>;
    virtual auto stat() -> IoTask<const char *>;

    // shutdown
    virtual auto shutdown() -> IoTask<void>;

    // stmt
    virtual auto stmtInit() -> IoTask<ISqlStmt*>;

    virtual auto setOpt(sqlopt::OptionBase *opt) -> int;
    virtual auto getOpt(sqlopt::OptionBase *opt) -> int;
    virtual auto close() -> void;
    virtual auto lastError() -> SqlError;
    virtual auto lastErrorMessage() -> std::string;
};
ILIAS_SQL_NS_END
