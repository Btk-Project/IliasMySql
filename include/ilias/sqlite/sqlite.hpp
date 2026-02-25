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

#include "ilias/sql/interfaces.hpp"

ILIAS_SQLITE_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

class ILIAS_SQL_API SqliteStatement final : public IStatement {
public:
    SqliteStatement(std::shared_ptr<sqlite3> sqlite, std::shared_ptr<SqlValueConverterContext> context);
    SqliteStatement(const SqliteStatement &) = delete;
    SqliteStatement &operator=(const SqliteStatement &) = delete;
    ~SqliteStatement();
    auto native() const -> sqlite3_stmt *;
    auto bind(std::type_index type_index, size_t index, const SqlCellView &value) -> IoResult<void> override;
    auto bind(std::type_index type_index, std::string_view name, const SqlCellView &value) -> IoResult<void> override;

    // 执行查询 (SELECT)，返回结果集
    auto query() -> IoTask<std::unique_ptr<IResultSet>> override;

    // 执行命令 (INSERT, UPDATE, DELETE)，返回影响行数
    auto execute() -> IoTask<size_t> override;

    // 重置状态以便复用
    auto reset() -> void override;
    auto prepare(std::string_view sql) -> IoTask<void>;
    auto clearBinds() -> void;
    auto close() -> IoTask<void>;
    auto nativeHandle() const -> void * override;

private:
    std::shared_ptr<sqlite3>                             mSqlite     = nullptr;
    std::shared_ptr<sqlite3_stmt>                        mSqliteStmt = nullptr;
    std::vector<std::unique_ptr<void, void (*)(void *)>> mDataGuards;
    std::shared_ptr<SqlValueConverterContext>            mContext;
};

class ILIAS_SQL_API SqliteStmtResultSet final : public IResultSet {
public:
    SqliteStmtResultSet(std::shared_ptr<sqlite3> sqlite, std::shared_ptr<sqlite3_stmt> stmt,
                        std::shared_ptr<SqlValueConverterContext> context);
    ~SqliteStmtResultSet();
    auto next() -> IoTask<bool> override;

    auto rowCount() const -> size_t override;
    // 获取列数
    auto columnCount() const -> size_t override;
    auto columnName(size_t index) const -> std::string_view override;

    // 获取当前行的某一列数据 (零拷贝尽可能)
    // 如果是 Text/Blob，返回的 View 有效期通常仅在下一次 next() 之前
    auto getValue(size_t index) -> IoResult<SqlCellView> override;

    // 按列名获取
    auto getValue(std::string_view name) -> IoResult<SqlCellView> override;
    auto setPrivate(std::unique_ptr<SqliteStatement> mp);
    auto nativeHandle() const -> void * override;

private:
    std::shared_ptr<sqlite3>                  mSqlite     = nullptr;
    std::shared_ptr<sqlite3_stmt>             mSqliteStmt = nullptr;
    bool                                      mIsFirst    = true;
    std::unordered_map<std::string, int>      mIndexs;
    std::unique_ptr<SqliteStatement>          mPrivate;
    std::shared_ptr<SqlValueConverterContext> mContext;
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

    auto valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> override;
    auto nativeHandle() const -> void * override;

private:
    std::shared_ptr<sqlite3>                  mSqlite = nullptr;
    ConnectOptions                            mOptions;
    std::shared_ptr<SqlValueConverterContext> mContext;
};

ILIAS_SQLITE_NS_END