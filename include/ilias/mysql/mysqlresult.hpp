#pragma once

#include <mariadb/mysql.h>
#include <mariadb/mysqld_error.h>

#include "global.hpp"
#include "mysql.hpp"

ILIAS_MYSQL_NS_BEGIN

class ILIAS_SQL_API SqlQueryResult final : public MySqlResultBase {
    using SqlError = sql::SqlError;

public:
    SqlQueryResult(std::shared_ptr<MySql> sql);
    SqlQueryResult(SqlQueryResult &&);
    SqlQueryResult &operator=(SqlQueryResult &&);
    ~SqlQueryResult();

    [[nodiscard("Don't forget to use co_await")]]
    auto next() -> IoTask<bool> override;
    auto get(size_t index) -> IoResult<SqlCellView> override;
    auto get(std::string_view name) -> IoResult<SqlCellView> override;
    auto exactRowCount() const -> std::optional<size_t> override;
    auto countFields() -> size_t override;
    auto fieldName(size_t index) -> std::string_view override;
    auto nativeResult() -> MYSQL_RES * override;
    auto lastNativeError() const -> std::optional<NativeSqlError> override;
    auto getResult() -> IoTask<void>;

protected:
    auto storeCurrentResultIfNeeded() -> IoTask<bool>;
    auto advanceToNextResult() -> IoTask<bool>;
    auto fetchRow() -> IoTask<MYSQL_ROW>;
    auto freeResult() -> void;

private:
    std::shared_ptr<MySql>                                       mMysql;
    std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>> mResult     = nullptr;
    MYSQL_ROW                                                    mCurrentRow = nullptr;
    std::vector<MYSQL_FIELD *>                                   mFieldMetas = {};
    std::optional<NativeSqlError>                                mLastNativeError;
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
    auto get(size_t index) -> IoResult<SqlCellView> override;
    auto get(std::string_view name) -> IoResult<SqlCellView> override;
    auto exactRowCount() const -> std::optional<size_t> override;
    auto countFields() -> size_t override;
    auto fieldName(size_t index) -> std::string_view override;
    auto nativeResult() -> MYSQL_RES * override;
    auto lastNativeError() const -> std::optional<NativeSqlError> override;
    auto getResult() -> IoTask<void>;

protected:
    auto storeCurrentResultIfNeeded() -> IoTask<bool>;
    auto advanceToNextResult() -> IoTask<bool>;
    auto fetchRow() -> IoTask<int>;
    auto freeResult() -> void;
    auto storeResult() -> IoTask<MYSQL_RES *>;
    auto nextResult() -> IoTask<int>;
    auto close() -> IoTask<bool>;
    auto reset() -> IoTask<bool>;

private:
    auto        execStoreResultAsync() -> IoTask<int>;
    auto        captureStatementNativeError(int fallbackCode = 0) -> NativeSqlError;
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
    std::unique_ptr<my_bool[]>                                   mIsNull;
    std::optional<NativeSqlError>                                mLastNativeError;
};
ILIAS_MYSQL_NS_END
