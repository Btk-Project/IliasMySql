#pragma once

#include "postgres.hpp"
#include "ilias/sql/sqlerror.hpp"
#include "ilias/sql/types.hpp"
#include <memory>
#include <vector>
#include <charconv>

ILIAS_POSTGRES_NS_BEGIN

// #############################################################################
// #  PostgresResultSet
// #############################################################################
class PostgresResultSet final : public IResultSet {
public:
    PostgresResultSet(std::shared_ptr<Postgres> pg, PGresult *result);
    ~PostgresResultSet();

    PostgresResultSet(const PostgresResultSet &)            = delete;
    PostgresResultSet(PostgresResultSet &&)                 = default;
    PostgresResultSet &operator=(const PostgresResultSet &) = delete;
    PostgresResultSet &operator=(PostgresResultSet &&)      = default;

    auto next() -> IoTask<bool> override;
    auto rowCount() const -> size_t override;
    auto columnCount() const -> size_t override;
    auto columnName(size_t index) const -> std::string_view override;
    auto getValue(size_t index) -> IoResult<SqlValue> override;
    auto getValue(std::string_view name) -> IoResult<SqlValue> override;
    auto native() -> PGresult *;

private:
    auto toValue(const PGresult *res, int rowIndex, int colIndex) -> IoResult<SqlValue>;

private:
    std::shared_ptr<Postgres>                     mPg;
    std::unique_ptr<PGresult, decltype(&PQclear)> mResult;
    int                                           mCurrentRow = -1;
    int                                           mTotalRows  = 0;
};

// #############################################################################
// #  PostgresStatement
// #############################################################################
class PostgresStatement final : public IStatement {
public:
    PostgresStatement(std::shared_ptr<Postgres> pg);
    ~PostgresStatement();

    auto bind(size_t index, SqlValuePointer value) -> Result<void, std::error_code> override;
    auto bind(std::string_view name, SqlValuePointer value) -> Result<void, std::error_code> override;
    auto query() -> IoTask<std::unique_ptr<IResultSet>> override;
    auto execute() -> IoTask<size_t> override;
    auto reset() -> void override;
    auto prepare(std::string_view sql) -> IoTask<void>;

private:
    auto parser(std::string_view sql) -> std::string;
    auto convertBinds() -> bool;
    void clearBinds();

private:
    std::shared_ptr<Postgres> mPg;
    std::string               mStatementName;
    std::string               mPreparedSql;

    // Storage for bound values
    std::vector<SqlValuePointer>         mBindValues;
    std::unordered_map<std::string, int> mNamedParamIndex;

    // Buffers for libpq C-style arrays
    std::vector<std::string>  mParamData;
    std::vector<const char *> mParamValuesPtrs;
    std::vector<int>          mParamLengths;
};

// #############################################################################
// #  PostgresConnection
// #############################################################################
class PostgresConnection final : public IConnection {
public:
    PostgresConnection(std::shared_ptr<Postgres> pg, ConnectOptions options);
    ~PostgresConnection() = default;

    auto sqlname() -> std::string override;
    auto sqlinfo() -> std::string override;
    auto connect() -> IoTask<void> override;
    auto disconnect() -> IoTask<void> override;
    auto selectDatabase(std::string_view name) -> IoTask<void> override;
    auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> override;
    auto execute(std::string_view sql) -> IoTask<size_t> override;
    auto query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> override;
    auto beginTransaction() -> IoTask<bool> override;
    auto commit() -> IoTask<bool> override;
    auto rollback() -> IoTask<bool> override;
    auto syncRollback() -> bool override;
    auto lastInsertId() const -> int64_t override;
    auto ping() -> IoTask<bool> override;

private:
    std::shared_ptr<Postgres>            mPg;
    ConnectOptions                       mOptions;
    bool                                 mIsConnected  = false;
    int64_t                              mLastInsertId = 0;
};

ILIAS_POSTGRES_NS_END