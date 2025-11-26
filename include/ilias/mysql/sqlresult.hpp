#pragma once

#include <unordered_map>
#include <mariadb/mysql.h>
#include <mariadb/mysqld_error.h>

#include "global.hpp"
#include "mysql.hpp"

ILIAS_MYSQL_NS_BEGIN

class SqlQuery;
class ILIAS_SQL_API SqlResultBase {
public:
    using SqlValue                             = sql::SqlValue;
    SqlResultBase()                            = default;
    SqlResultBase(SqlResultBase &&)            = default;
    SqlResultBase &operator=(SqlResultBase &&) = default;
    virtual ~SqlResultBase()                   = default;

    SqlResultBase(const SqlResultBase &)            = delete;
    SqlResultBase &operator=(const SqlResultBase &) = delete;

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

class ILIAS_SQL_API SqlQueryResult final : public SqlResultBase {
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
    std::shared_ptr<MySql>     mMysql;
    MYSQL_RES                 *mResult     = nullptr;
    MYSQL_ROW                  mCurrentRow = nullptr;
    std::vector<MYSQL_FIELD *> mFieldMetas = {};

    friend class ::ILIAS_MYSQL_COMPLETE_NAMESPACE::SqlQuery;
};

class ILIAS_SQL_API SqlStmtResult final : public SqlResultBase {
    using SqlError = sql::SqlError;
    struct BindConfig {
        enum_field_types bufferType;
        size_t           bufferSize;
        bool             isUnsigned;
    };

public:
    SqlStmtResult(std::shared_ptr<MySql> sql, MYSQL_STMT *stmt);
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
    std::shared_ptr<MySql>                                      mMysql;
    MYSQL_STMT                                                 *mStmt       = nullptr;
    MYSQL_RES                                                  *mResult     = nullptr;
    std::vector<MYSQL_FIELD *>                                  mFieldMetas = {};
    std::unordered_map<std::string, std::unique_ptr<uint8_t[]>> mFields;
    std::unique_ptr<MYSQL_BIND[]>                               mBinds;
    std::unique_ptr<unsigned long[]>                            mLengths;

    friend class ::ILIAS_MYSQL_COMPLETE_NAMESPACE::SqlQuery;
};
ILIAS_MYSQL_NS_END