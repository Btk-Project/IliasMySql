/**
 * @file interfaces.hpp
 * @brief Abstract interfaces for SQL drivers
 */
#pragma once
#include <ilias/io/context.hpp>
#include <memory>
#include <string_view>
#include <map>

#include "ilias/sql/global/global.hpp"
#include "types.hpp"

ILIAS_SQL_NS_BEGIN

struct ConnectOptions {
    std::string                        host;
    uint16_t                           port = 0;
    std::string                        user;
    std::string                        password;
    std::string                        database;
    std::string                        filename;
    std::map<std::string, std::string> extra;
};

// 前置声明
class IStatement;

/**
 * @brief 结果集迭代器接口
 * 对应 rusqlite 的 Rows
 */
class IResultSet {
public:
    virtual ~IResultSet() = default;

    // 移动到下一行。返回 false 表示结束。
    virtual auto next() -> IoTask<bool> = 0;

    virtual auto rowCount() const -> size_t = 0;
    // 获取列数
    virtual auto columnCount() const -> size_t                      = 0;
    virtual auto columnName(size_t index) const -> std::string_view = 0;

    // 获取当前行的某一列数据 (零拷贝尽可能)
    // 如果是 Text/Blob，返回的 View 有效期通常仅在下一次 next() 之前
    virtual auto getValue(size_t index) -> IoResult<SqlValue> = 0;

    // 按列名获取
    virtual auto getValue(std::string_view name) -> IoResult<SqlValue> = 0;
};

/**
 * @brief 预编译语句接口
 * 对应 rusqlite 的 Statement
 */
class IStatement {
public:
    virtual ~IStatement() = default;

    virtual auto bind(size_t index, SqlValuePointer value) -> Result<void, std::error_code>          = 0;
    virtual auto bind(std::string_view name, SqlValuePointer value) -> Result<void, std::error_code> = 0;
    // 执行查询 (SELECT)，返回结果集
    virtual auto query() -> IoTask<std::unique_ptr<IResultSet>> = 0;

    // 执行命令 (INSERT, UPDATE, DELETE)，返回影响行数
    virtual auto execute() -> IoTask<size_t> = 0;

    // 重置状态以便复用
    virtual auto reset() -> void = 0;
};

/**
 * @brief 数据库连接接口
 * 对应 rusqlite 的 Connection
 */
class IConnection {
public:
    virtual ~IConnection()                                             = default;
    virtual auto sqlname() -> std::string                              = 0;
    virtual auto sqlinfo() -> std::string                              = 0;
    virtual auto connect() -> IoTask<void>                             = 0;
    virtual auto disconnect() -> IoTask<void>                          = 0;
    virtual auto selectDatabase(std::string_view name) -> IoTask<void> = 0;

    // 预编译 SQL
    virtual auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> = 0;

    // 直接执行 SQL
    virtual auto execute(std::string_view sql) -> IoTask<size_t>                    = 0;
    virtual auto query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> = 0;

    // 事务控制
    virtual auto beginTransaction() -> IoTask<bool> = 0;
    virtual auto commit() -> IoTask<bool>           = 0;
    virtual auto rollback() -> IoTask<bool>         = 0;
    virtual auto syncRollback() -> bool             = 0;

    // 获取最后一次插入的 ID
    virtual auto lastInsertId() const -> int64_t = 0;

    // 连通性检测
    virtual auto ping() -> IoTask<bool> = 0;
};

ILIAS_SQL_NS_END