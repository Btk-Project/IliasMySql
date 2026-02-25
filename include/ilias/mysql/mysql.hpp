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
#include "mysqlopt.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql/interfaces.hpp"

ILIAS_MYSQL_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

class ILIAS_SQL_API MySqlResultBase {
public:
    MySqlResultBase(std::shared_ptr<SqlValueConverterContext> context) : mContext(context) {}
    MySqlResultBase(MySqlResultBase &&)            = default;
    MySqlResultBase &operator=(MySqlResultBase &&) = default;
    virtual ~MySqlResultBase()                     = default;

    MySqlResultBase(const MySqlResultBase &)            = delete;
    MySqlResultBase &operator=(const MySqlResultBase &) = delete;

    [[nodiscard("Don't forget to use co_await")]]
    virtual auto next() -> IoTask<bool>                              = 0;
    virtual auto countRows() -> size_t                               = 0;
    virtual auto countFields() -> size_t                             = 0;
    virtual auto fieldName(size_t index) -> std::string_view         = 0;
    virtual auto get(size_t index) -> IoResult<SqlCellView>          = 0;
    virtual auto get(std::string_view name) -> IoResult<SqlCellView> = 0;
    auto stmtToValue(MYSQL_FIELD *field, uint8_t *buffer, size_t bufferSize, bool isNull) -> IoResult<SqlCellView>;
    auto toValue(MYSQL_FIELD *field, char *buffer, size_t bufferSize) -> IoResult<SqlCellView>;
    virtual auto nativeResult() -> MYSQL_RES * = 0;

private:
    std::shared_ptr<SqlValueConverterContext> mContext;
};

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
    auto syncAutoCommit(bool autoMode) -> bool;

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
    auto info() -> std::string;

    bool operator==(MySql &other);

    auto valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext>;

private:
    IoContext                                *mCtxt = nullptr;
    MYSQL                                     mMysql;
    Poller                                    mPoller;
    std::shared_ptr<SqlValueConverterContext> mContext;
};

class ILIAS_SQL_API MysqlResultSet final : public IResultSet {
public:
    MysqlResultSet(const MysqlResultSet &)            = delete;
    MysqlResultSet(MysqlResultSet &&)                 = default;
    MysqlResultSet &operator=(const MysqlResultSet &) = delete;
    MysqlResultSet &operator=(MysqlResultSet &&)      = default;
    MysqlResultSet(std::unique_ptr<MySqlResultBase>&& imp);
    ~MysqlResultSet();
    auto next() -> IoTask<bool> override;
    auto rowCount() const -> size_t override;
    auto columnCount() const -> size_t override;
    auto columnName(size_t index) const -> std::string_view override;
    auto getValue(size_t index) -> IoResult<SqlCellView> override;
    auto getValue(std::string_view name) -> IoResult<SqlCellView> override;
    auto nativeHandle() const -> void * override;
    auto native() -> MYSQL_RES *;

private:
    std::unique_ptr<MySqlResultBase> mImp;
};

class ILIAS_SQL_API MysqlStatement final : public IStatement {
public:
    using SqlError                                    = sql::SqlError;
    MysqlStatement(const MysqlStatement &)            = delete;
    MysqlStatement(MysqlStatement &&)                 = default;
    MysqlStatement &operator=(const MysqlStatement &) = delete;
    MysqlStatement &operator=(MysqlStatement &&)      = default;
    MysqlStatement(std::shared_ptr<MySql> mysql);
    ~MysqlStatement();

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

    auto dataBind(size_t index) -> MYSQL_BIND *;
private:
    // this query should like "SELECT * FROM table WHERE name=:name,age=:age;"
    // return query like "SELECT * FROM table WHERE name=?,age=?;"
    auto parser(std::string_view sql) -> std::string;
private:
    std::shared_ptr<MySql>                   mMysql;
    std::shared_ptr<MYSQL_STMT>              mMysqlStmt = nullptr;
    std::vector<MYSQL_BIND>                  mBinds;
    std::unordered_map<std::string, int>     mIndexs;
    std::vector<std::unique_ptr<void, void (*)(void *)>> mDataGuards;
};

class ILIAS_SQL_API MysqlConnection final : public IConnection {
public:
    MysqlConnection(std::shared_ptr<MySql> mysql, ConnectOptions options) : mMysql(mysql), mOptions(options) {}
    ~MysqlConnection();
    auto sqlname() -> std::string override;
    auto sqlinfo() -> std::string override;
    auto connect() -> IoTask<void> override;
    auto disconnect() -> IoTask<void> override;
    auto selectDatabase(std::string_view name) -> IoTask<void> override;
    // 预编译 SQL
    auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> override;
    // 直接执行 SQL (不带参)
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
    auto mysql() -> std::shared_ptr<MySql> { return mMysql; }
    auto nativeHandle() const -> void * override;
    auto valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> override;

private:
    std::shared_ptr<MySql> mMysql;
    ConnectOptions         mOptions;
    bool                   mIsConnected = false;
};
ILIAS_MYSQL_NS_END
