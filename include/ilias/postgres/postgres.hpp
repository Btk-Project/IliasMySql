/**
 * @file postgres.hpp
 * @author llhsdmd (llhsdmd@gmail.com) & Gemini
 * @brief postgresql I/O
 * @version 0.1
 * @date 2025-12-18
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include "ilias/postgres/global.hpp"

#include <ilias/io/context.hpp>
#include <ilias/io/fd_utils.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/net/poller.hpp>
#include <ilias/net/sockfd.hpp>
#include <ilias/task/when_any.hpp>
#include <libpq-fe.h>

#include "ilias/sql/sql_plugin.hpp"
#include "ilias/sql/types.hpp"

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

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

    // Asynchronous command sending
    auto sendQuery(std::string_view sql) -> IoTask<void>;
    auto sendQueryParams(std::string_view command, int nParams, const Oid *paramTypes, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void>;
    auto sendPrepare(std::string_view stmtName, std::string_view query, int nParams, const Oid *paramTypes) -> IoTask<void>;
    auto sendQueryPrepared(std::string_view stmtName, int nParams, const char *const *paramValues, const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void>;
    
    // Asynchronous result fetching
    auto getResult() -> IoTask<PGresult *>;

    // Utility coroutines and functions
    auto waitForReadable() -> IoTask<void>;
    auto consumeInput() -> IoResult<void>;

    // Status and error reporting
    auto lastError() -> int;
    auto lastErrorMessage() -> const char *;
    auto info() -> std::string;

    bool operator==(Postgres &other);
    auto initializeTypeMap() -> IoTask<void>;
    auto getTypeMap() -> std::unordered_map<Oid, std::string> &;

private:
    IoContext *mCtxt = nullptr;
    PGconn    *mConn = nullptr;
    Poller     mPoller;
    std::unordered_map<Oid, std::string> mTypeMap;
};

ILIAS_POSTGRES_NS_END