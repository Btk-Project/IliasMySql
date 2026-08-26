/**
 * @file postgres.hpp
 * @author llhsdmd (llhsdmd@gmail.com) & Gemini
 * @brief PostgreSQL async client implementation
 * @version 1.0.0
 * @date 2025-12-18
 *
 * @copyright Copyright (c) 2024
 *
 * This file provides async PostgreSQL client implementation
 * using the native libpq library with coroutine support.
 */

#pragma once

#include "ilias/postgres/global.hpp"

#include <ilias/io/context.hpp>
#include <ilias/io/fd_utils.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/net/poller.hpp>
#include <ilias/net/sockfd.hpp>
#include <ilias/task/when_any.hpp>
#include <libpq-fe.h>

#include <optional>

#include "ilias/sql/sql_plugin.hpp"
#include "ilias/sql/types.hpp"

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

/**
 * @brief Async PostgreSQL client
 *
 * Provides async database operations using c++23 coroutines.
 * This class wraps the native libpq library and provides
 * coroutine-friendly methods for all database operations,
 * including streaming result support.
 */
class ILIAS_SQL_API Postgres final {
public:
    using SqlError = sql::SqlError;

public:
    Postgres();
    Postgres(const Postgres &) = delete;
    ~Postgres();

    auto native() -> PGconn *;

    auto connect(std::string_view conninfo) -> IoTask<void>;
    auto disconnect() -> void;
    auto disconnectAsync() -> IoTask<void>;

    // Asynchronous command sending
    auto sendQuery(std::string_view sql) -> IoTask<void>;
    auto sendQueryParams(std::string_view command, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void>;
    auto sendPrepare(std::string_view stmtName, std::string_view query, int nParams, const Oid *paramTypes) -> IoTask<void>;
    auto sendQueryPrepared(std::string_view stmtName, int nParams, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void>;
    
    // Streaming support
    auto setSingleRowMode() -> bool;
    
    // Asynchronous result fetching
    auto getResult() -> IoTask<PGresult *>;

    // Utility coroutines and functions
    auto waitForReadable() -> IoTask<void>;
    auto waitForWritable() -> IoTask<void>;
    auto flushOutput() -> IoTask<void>;
    auto consumeInput() -> IoResult<void>;

    // Status and error reporting
    auto lastError() -> int;
    auto lastErrorMessage() -> const char *;
    auto setLastNativeError(NativeSqlError error) -> void;
    auto lastNativeError() const -> std::optional<NativeSqlError>;
    auto info() -> std::string;

    bool operator==(Postgres &other);
    auto initializeTypeMap() -> IoTask<void>;
    auto getTypeMap() -> std::map<Oid, std::string> &;
    auto valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext>;

private:
    IoContext *mCtxt = nullptr;
    PGconn    *mConn = nullptr;
    Poller     mPoller;
    std::map<Oid, std::string> mTypeMap;
    std::shared_ptr<SqlValueConverterContext> mContext;
    std::optional<NativeSqlError> mLastNativeError;
};

ILIAS_POSTGRES_NS_END
