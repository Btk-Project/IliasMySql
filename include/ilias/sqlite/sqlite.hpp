#pragma once

/**
 * @file sqlite.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief sqlite I/O
 * @version 0.1
 * @date 2025-11-26
 *
 * @copyright Copyright (c) 2024
 *
 */

#include "global.hpp"

#include <ilias/io/context.hpp>
#include <ilias/io/fd_utils.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/net/poller.hpp>
#include <ilias/net/sockfd.hpp>
#include <ilias/task/when_any.hpp>
#include <sqlite3.h>

#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/interfaces.hpp"

ILIAS_SQL_NS_BEGIN
class DriverManager;
ILIAS_SQL_NS_END

ILIAS_SQLITE_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

class ILIAS_SQL_API SqliteStatement final : public IStatement {
public:
    SqliteStatement(std::shared_ptr<sqlite3> sqlite);
    ~SqliteStatement();
    auto bind(size_t index, SqlValuePointer value) -> Result<void, std::error_code> override;
    auto bind(std::string_view name, SqlValuePointer value) -> Result<void, std::error_code> override;

    // 执行查询 (SELECT)，返回结果集
    auto query() -> IoTask<std::unique_ptr<IResultSet>> override;

    // 执行命令 (INSERT, UPDATE, DELETE)，返回影响行数
    auto execute() -> IoTask<size_t> override;

    // 重置状态以便复用
    auto reset() -> void override;
    auto prepare(std::string_view sql) -> IoTask<void>;
    auto clearBinds() -> void;
    auto close() -> IoTask<void>;

private:
    std::shared_ptr<sqlite3>      mSqlite     = nullptr;
    std::shared_ptr<sqlite3_stmt> mSqliteStmt = nullptr;
};

class ILIAS_SQL_API SqliteStmtResultSet final : public IResultSet {
public:
    SqliteStmtResultSet(std::shared_ptr<sqlite3> sqlite, std::shared_ptr<sqlite3_stmt> stmt);
    ~SqliteStmtResultSet();
    auto next() -> IoTask<bool> override;

    auto rowCount() const -> size_t override;
    // 获取列数
    auto columnCount() const -> size_t override;
    auto columnName(size_t index) const -> std::string_view override;

    // 获取当前行的某一列数据 (零拷贝尽可能)
    // 如果是 Text/Blob，返回的 View 有效期通常仅在下一次 next() 之前
    auto getValue(size_t index) -> IoResult<SqlValue> override;

    // 按列名获取
    auto getValue(std::string_view name) -> IoResult<SqlValue> override;
    auto setPrivate(std::unique_ptr<SqliteStatement> mp);

private:
    std::shared_ptr<sqlite3>             mSqlite     = nullptr;
    std::shared_ptr<sqlite3_stmt>        mSqliteStmt = nullptr;
    bool                                 mIsFirst    = true;
    std::unordered_map<std::string, int> mIndexs;
    std::unique_ptr<SqliteStatement>     mPrivate;
};

class ILIAS_SQL_API Sqlite final : public IConnection {
public:
    Sqlite(const ConnectOptions &options);
    ~Sqlite();
    auto native() -> sqlite3 *;
    auto sqlname() -> std::string override;
    auto sqlinfo() -> std::string override;
    auto connect() -> IoTask<void> override;
    auto disconnect() -> IoTask<void> override;
    auto selectDatabase(std::string_view name) -> IoTask<void> override;

    // 预编译 SQL
    auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> override;

    // 直接执行 SQL
    auto execute(std::string_view sql) -> IoTask<size_t> override;
    auto query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> override;

    // 事务控制
    auto beginTransaction() -> IoTask<bool> override;
    auto commit() -> IoTask<bool> override;
    auto rollback() -> IoTask<bool> override;
    auto syncRollback() -> bool override;

    // 获取最后一次插入的 ID
    auto lastInsertId() const -> int64_t override;

    // 连通性检测
    auto ping() -> IoTask<bool> override;

private:
    std::shared_ptr<sqlite3> mSqlite = nullptr;
    ConnectOptions           mOptions;
};

ILIAS_SQL_USE_NAMESPACE
ILIAS_SQL_API void ilias_register_sql_plugin(DriverManager *manager);

ILIAS_SQLITE_NS_END