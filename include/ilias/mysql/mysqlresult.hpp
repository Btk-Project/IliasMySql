#pragma once

#include <unordered_map>
#include <mariadb/mysql.h>
#include <mariadb/mysqld_error.h>

#include "global.hpp"
#include "mysql.hpp"

ILIAS_MYSQL_NS_BEGIN

class ILIAS_SQL_API MySqlResultBase {
public:
    using SqlValue                                 = sql::SqlValue;
    MySqlResultBase()                              = default;
    MySqlResultBase(MySqlResultBase &&)            = default;
    MySqlResultBase &operator=(MySqlResultBase &&) = default;
    virtual ~MySqlResultBase()                     = default;

    MySqlResultBase(const MySqlResultBase &)            = delete;
    MySqlResultBase &operator=(const MySqlResultBase &) = delete;

    [[nodiscard("Don't forget to use co_await")]]
    virtual auto next() -> IoTask<bool>                           = 0;
    virtual auto countRows() -> size_t                            = 0;
    virtual auto countFields() -> size_t                          = 0;
    virtual auto fieldName(size_t index) -> std::string_view      = 0;
    virtual auto get(size_t index) -> IoResult<SqlValue>          = 0;
    virtual auto get(std::string_view name) -> IoResult<SqlValue> = 0;
    static auto  stmtToValue(MYSQL_FIELD *field, uint8_t *buffer, size_t bufferSize)
        -> Result<SqlValue, std::error_code>;
    static auto  toValue(MYSQL_FIELD *field, char *buffer, size_t bufferSize) -> Result<SqlValue, std::error_code>;
    virtual auto nativeResult() -> MYSQL_RES * = 0;
};

class ILIAS_SQL_API SqlQueryResult final : public MySqlResultBase {
    using SqlError = sql::SqlError;

public:
    SqlQueryResult(std::shared_ptr<MySql> sql);
    SqlQueryResult(SqlQueryResult &&);
    SqlQueryResult &operator=(SqlQueryResult &&);
    ~SqlQueryResult();

    [[nodiscard("Don't forget to use co_await")]]
    auto next() -> IoTask<bool> override;
    auto get(size_t index) -> IoResult<SqlValue> override;
    auto get(std::string_view name) -> IoResult<SqlValue> override;
    auto countRows() -> size_t override;
    auto countFields() -> size_t override;
    auto fieldName(size_t index) -> std::string_view override;
    auto nativeResult() -> MYSQL_RES * override;
    auto getResult() -> IoTask<void>;

protected:
    auto fetchRow() -> IoTask<MYSQL_ROW>;
    auto freeResult() -> void;

private:
    std::shared_ptr<MySql>                                       mMysql;
    std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>> mResult     = nullptr;
    MYSQL_ROW                                                    mCurrentRow = nullptr;
    std::vector<MYSQL_FIELD *>                                   mFieldMetas = {};
};

class ILIAS_SQL_API SqlStmtResult final : public MySqlResultBase {
    using SqlError = sql::SqlError;
    struct BindConfig {
        enum_field_types bufferType;
        size_t           bufferSize;
        bool             isUnsigned;
    };

public:
    SqlStmtResult(std::shared_ptr<MySql> sql, std::shared_ptr<MYSQL_STMT> stmt);
    SqlStmtResult(SqlStmtResult &&);
    SqlStmtResult &operator=(SqlStmtResult &&);
    ~SqlStmtResult();

    auto next() -> IoTask<bool> override;
    auto get(size_t index) -> IoResult<SqlValue> override;
    auto get(std::string_view name) -> IoResult<SqlValue> override;
    auto countRows() -> size_t override;
    auto countFields() -> size_t override;
    auto fieldName(size_t index) -> std::string_view override;
    auto nativeResult() -> MYSQL_RES * override;
    auto getResult() -> IoTask<void>;

protected:
    auto fetchRow() -> IoTask<int>;
    auto freeResult() -> void;
    auto storeResult() -> IoTask<MYSQL_RES *>;
    auto nextResult() -> IoTask<int>;
    auto close() -> IoTask<bool>;
    auto reset() -> IoTask<bool>;

private:
    auto        execStoreResultAsync() -> IoTask<int>;
    static auto getBindConfig(const MYSQL_FIELD *field) -> IoResult<BindConfig>;
    auto        allocateBindBuffers(MYSQL_RES *meta) -> IoResult<void>;

private:
    std::shared_ptr<MySql>                                       mMysql;
    std::shared_ptr<MYSQL_STMT>                                  mStmt       = nullptr;
    std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>> mResult     = nullptr;
    std::vector<MYSQL_FIELD *>                                   mFieldMetas = {};
    std::vector<std::unique_ptr<uint8_t[]>>                      mFields;
    std::unique_ptr<MYSQL_BIND[]>                                mBinds;
    std::unique_ptr<unsigned long[]>                             mLengths;
};
ILIAS_MYSQL_NS_END