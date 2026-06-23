#include "ilias/mysql/mysqlresult.hpp"

#include <charconv>

#define SQL_PRIVATE_SYNC_CODE(OutP, MysqlFunc, ...)                                                                    \
    auto status = MysqlFunc##_start(&OutP, mStmt.get(), ##__VA_ARGS__);                                                \
    while (status) {                                                                                                   \
        ILIAS_TRACE("sql", "{} waiting for status {}", #MysqlFunc, status);                                            \
        auto pret = co_await mMysql->pollStatus(status);                                                               \
        status    = MysqlFunc##_cont(&OutP, mStmt.get(), status);                                                      \
        if (!pret) {                                                                                                   \
            co_return Err(pret.error());                                                                        \
        }                                                                                                              \
    }                                                                                                                  \
    auto _check = [](auto p) {                                                                                         \
        if constexpr (std::is_pointer_v<decltype(p)>) {                                                                \
            if (p != nullptr)                                                                                          \
                return true;                                                                                           \
        }                                                                                                              \
        else if constexpr (std::is_same_v<decltype(p), my_bool>) {                                                     \
            if (p)                                                                                                     \
                return true;                                                                                           \
        }                                                                                                              \
        else if constexpr (std::is_integral_v<decltype(p)>) {                                                          \
            if (p == 0)                                                                                                \
                return true;                                                                                           \
        }                                                                                                              \
        else {                                                                                                         \
            ILIAS_ERROR("sql", "unknow type?");                                                                        \
            return false;                                                                                              \
        }                                                                                                              \
        return false;                                                                                                  \
    };                                                                                                                 \
    if (!_check(OutP)) {                                                                                               \
        auto errCode = mysql_stmt_errno(mStmt.get());                                                                  \
        if (errCode != 0) {                                                                                            \
            auto nativeError = captureStatementNativeError(errCode);                                                   \
            ILIAS_ERROR("sql", "{} failed, error({}): {}", #MysqlFunc, errCode, nativeError.message);                  \
            co_return Err(MySql::nativeErrorCodeToSqlError(errCode));                                                  \
        }                                                                                                              \
    }

ILIAS_MYSQL_NS_BEGIN

auto MySqlResultBase::stmtToValue(MYSQL_FIELD *field, uint8_t *buffer, size_t bufferSize, bool isNull)
    -> IoResult<SqlCellView> {
    // Check the is_null indicator first - this is the proper way to detect NULL values
    if (isNull) {
        return SqlCellView {mContext, &g_sql_null, sizeof(g_sql_null), std::type_index(typeid(g_sql_null)), -1};
    }
    if (buffer == nullptr) {
        return SqlCellView {mContext, &g_sql_null, sizeof(g_sql_null), std::type_index(typeid(g_sql_null)), -1};
    }
    switch (field->type) {
        case MYSQL_TYPE_TINY:  // char
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24:
            return SqlCellView {mContext, buffer, (int)bufferSize, std::type_index(typeid(int32_t)), -1};
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_DOUBLE:
            return SqlCellView {mContext, buffer, (int)bufferSize, std::type_index(typeid(double)), -1};
            break;
        case MYSQL_TYPE_LONGLONG: // long long
            return SqlCellView {mContext, buffer, (int)bufferSize, std::type_index(typeid(int64_t)), -1};
        default:
            return SqlCellView {mContext, std::string_view((char *)buffer, bufferSize),
                                static_cast<uint32_t>(field->type), -1};
    }
}

auto MySqlResultBase::toValue(MYSQL_FIELD *field, char *buffer, size_t bufferSize) -> IoResult<SqlCellView> {
    if (buffer == nullptr) {
        return SqlCellView {mContext, &g_sql_null, sizeof(g_sql_null), std::type_index(typeid(g_sql_null)), -1};
    }
    return SqlCellView {mContext, std::string_view((char *)buffer, bufferSize), static_cast<uint32_t>(field->type), -1};
}

SqlQueryResult::SqlQueryResult(SqlQueryResult &&other) : MySqlResultBase(other.mMysql->valueConverterContext()) {
    mMysql            = std::move(other.mMysql);
    mResult           = std::move(other.mResult);
    mCurrentRow       = other.mCurrentRow;
    mFieldMetas       = std::move(other.mFieldMetas);
    mLastNativeError  = std::move(other.mLastNativeError);
    other.mResult     = nullptr;
    other.mCurrentRow = nullptr;
    other.mFieldMetas.clear();
    other.mLastNativeError.reset();
}

SqlQueryResult &SqlQueryResult::operator=(SqlQueryResult &&other) {
    if (mResult) {
        freeResult();
    }
    if (this != &other) {
        mMysql            = std::move(other.mMysql);
        mResult           = std::move(other.mResult);
        mCurrentRow       = other.mCurrentRow;
        mFieldMetas       = std::move(other.mFieldMetas);
        mLastNativeError  = std::move(other.mLastNativeError);
        other.mResult     = nullptr;
        other.mCurrentRow = nullptr;
        other.mFieldMetas.clear();
        other.mLastNativeError.reset();
    }
    return *this;
}

SqlQueryResult::SqlQueryResult(std::shared_ptr<MySql> sql)
    : MySqlResultBase(sql->valueConverterContext()), mMysql(sql) {
}

SqlQueryResult::~SqlQueryResult() {
    if (mResult) {
        freeResult();
    }
}

auto SqlQueryResult::nativeResult() -> MYSQL_RES * {
    return mResult.get();
}

auto SqlQueryResult::lastNativeError() const -> std::optional<NativeSqlError> {
    if (mLastNativeError) {
        return mLastNativeError;
    }
    return mMysql ? mMysql->lastNativeError() : std::nullopt;
}

auto SqlQueryResult::getResult() -> IoTask<void> {
    ILIAS_CO_TRY(auto ret, co_await (mMysql->storeResult() | unstoppable()));
    mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
        ret, [](MYSQL_RES *res) { mysql_free_result(res); });
    ILIAS_TRACE("ilias-mysql", "Get {} rows", exactRowCount().value_or(0));
    co_return {};
}

auto SqlQueryResult::next() -> IoTask<bool> {
    while (true) {
        ILIAS_CO_TRY(auto hasResult, co_await storeCurrentResultIfNeeded());
        if (!hasResult) {
            ILIAS_CO_TRY(auto hasNextResult, co_await advanceToNextResult());
            if (!hasNextResult) {
                co_return false;
            }
            continue;
        }

        ILIAS_CO_TRY(auto row, co_await fetchRow());
        if (row) {
            mCurrentRow = row;
            co_return true;
        }

        ILIAS_CO_TRY(auto hasNextResult, co_await advanceToNextResult());
        if (!hasNextResult) {
            co_return false;
        }
    }
}

auto SqlQueryResult::get(size_t index) -> IoResult<SqlCellView> {
    if (mCurrentRow == nullptr) {
        return Err(SqlError::Code::NoMoreData);
    }
    if (mResult == nullptr) {
        return Err(SqlError::Code::NoMoreData);
    }
    if (mFieldMetas.empty()) {
        mFieldMetas.resize(mysql_num_fields(mResult.get()));
        auto fieldMetas = mysql_fetch_fields(mResult.get());
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (index >= mFieldMetas.size()) {
        return Err(SqlError::Code::InvalidIndex);
    }
    auto lengths = mysql_fetch_lengths(mResult.get());
    auto result  = toValue(mFieldMetas[index], mCurrentRow[index], lengths[index]);
    return result;
}

// TODO: optimize
auto SqlQueryResult::get(std::string_view name) -> IoResult<SqlCellView> {
    if (mResult == nullptr || mCurrentRow == nullptr) {
        return Err(SqlError::Code::NoMoreData);
    }
    if (mFieldMetas.empty()) {
        mFieldMetas.resize(mysql_num_fields(mResult.get()));
        auto fieldMetas = mysql_fetch_fields(mResult.get());
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (mFieldMetas.empty()) {
        return Err(SqlError::Code::NoMoreData);
    }
    std::size_t index = -1;
    for (size_t i = 0; i < mysql_num_fields(mResult.get()); ++i) {
        if (mFieldMetas[i]->name == name) {
            index = i;
            break;
        }
    }
    if (index == (std::size_t)-1) {
        return Err(SqlError::Code::InvalidIndex);
    }
    return get(index);
}

auto SqlQueryResult::exactRowCount() const -> std::optional<size_t> {
    if (mResult == nullptr) {
        return std::nullopt;
    }
    return mysql_num_rows(mResult.get());
}

auto SqlQueryResult::countFields() -> size_t {
    if (mResult == nullptr) {
        return 0;
    }
    return mysql_num_fields(mResult.get());
}

auto SqlQueryResult::fieldName(size_t index) -> std::string_view {
    if (mFieldMetas.empty() && mResult != nullptr) {
        mFieldMetas.resize(mysql_num_fields(mResult.get()));
        auto fieldMetas = mysql_fetch_fields(mResult.get());
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (index >= mFieldMetas.size()) {
        return {};
    }
    return mFieldMetas[index]->name;
}

auto SqlQueryResult::fetchRow() -> IoTask<MYSQL_ROW> {
    ILIAS_ASSERT(mResult != nullptr);
    MYSQL_ROW row;
    auto      status = mysql_fetch_row_start(&row, mResult.get());
    if (status) {
        while (status) {
            ILIAS_TRACE("sql", "disconnect mysql waiting for status {}", status);
            auto pret = co_await mMysql->pollStatus(status);
            status    = mysql_fetch_row_cont(&row, mResult.get(), status);
            if (!pret) {
                co_return Err(pret.error());
            }
        }
    }
    co_return row;
}

auto SqlQueryResult::storeCurrentResultIfNeeded() -> IoTask<bool> {
    if (mResult) {
        co_return true;
    }

    ILIAS_CO_TRY(auto rawResult, co_await (mMysql->storeResult() | unstoppable()));
    mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
        rawResult, [](MYSQL_RES *ptr) { mysql_free_result(ptr); });
    co_return mResult != nullptr;
}

auto SqlQueryResult::advanceToNextResult() -> IoTask<bool> {
    freeResult();
    ILIAS_CO_TRY(auto status, co_await (mMysql->nextResult() | unstoppable()));
    if (status == 0) {
        co_return true;
    }
    if (status == -1) {
        co_return false;
    }
    mLastNativeError = mMysql ? mMysql->lastNativeError() : std::nullopt;
    co_return Err(mLastNativeError ? MySql::nativeErrorCodeToSqlError(mLastNativeError->code)
                                   : SqlError::Code::UnknownError);
}

auto SqlQueryResult::freeResult() -> void {
    mResult.reset();
    mCurrentRow = nullptr;
    mFieldMetas.clear();
}

SqlStmtResult::SqlStmtResult(SqlStmtResult &&other) : MySqlResultBase(other.mMysql->valueConverterContext()) {
    mMysql  = std::move(other.mMysql);
    mStmt   = std::move(other.mStmt);
    mResult = std::move(other.mResult);
    mLastNativeError = std::move(other.mLastNativeError);
    other.mLastNativeError.reset();
}

SqlStmtResult &SqlStmtResult::operator=(SqlStmtResult &&other) {
    if (mStmt) {
        close().wait();
    }
    if (this != &other) {
        mMysql  = std::move(other.mMysql);
        mStmt   = std::move(other.mStmt);
        mResult = std::move(other.mResult);
        mLastNativeError = std::move(other.mLastNativeError);
        other.mLastNativeError.reset();
    }
    return *this;
}

SqlStmtResult::SqlStmtResult(std::shared_ptr<MySql> sql, std::shared_ptr<MYSQL_STMT> stmt)
    : MySqlResultBase(sql->valueConverterContext()), mMysql(sql), mStmt(stmt) {
}

SqlStmtResult::~SqlStmtResult() {
}

auto SqlStmtResult::nativeResult() -> MYSQL_RES * {
    return mResult.get();
}

auto SqlStmtResult::lastNativeError() const -> std::optional<NativeSqlError> {
    if (mLastNativeError) {
        return mLastNativeError;
    }
    return mMysql ? mMysql->lastNativeError() : std::nullopt;
}

auto SqlStmtResult::getResult() -> IoTask<void> {
    ILIAS_CO_TRY(auto ret, co_await (storeResult() | unstoppable()));
    mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
        ret, [](MYSQL_RES *res) { mysql_free_result(res); });
    co_return {};
}

auto SqlStmtResult::next() -> IoTask<bool> {
    while (true) {
        ILIAS_CO_TRY(auto hasResult, co_await storeCurrentResultIfNeeded());
        if (!hasResult) {
            ILIAS_CO_TRY(auto hasNextResult, co_await advanceToNextResult());
            if (!hasNextResult) {
                co_return false;
            }
            continue;
        }

        ILIAS_CO_TRY(auto status, co_await fetchRow());
        if (status == 0 || status == MYSQL_DATA_TRUNCATED) {
            co_return true;
        }
        if (status != MYSQL_NO_DATA) {
            auto nativeError = captureStatementNativeError(status);
            co_return Err(MySql::nativeErrorCodeToSqlError(nativeError.code));
        }

        ILIAS_CO_TRY(auto hasNextResult, co_await advanceToNextResult());
        if (!hasNextResult) {
            co_return false;
        }
    }
}

auto SqlStmtResult::get(size_t index) -> IoResult<SqlCellView> {
    if (mFields.empty() || mFieldMetas.empty()) {
        return Err(SqlError::Code::NoMoreData);
    }
    if (index >= mFieldMetas.size()) {
        return Err(SqlError::Code::InvalidIndex);
    }
    auto &currentRow = mFields[index];

    auto result = stmtToValue(mFieldMetas[index], currentRow.get(), mLengths[index], mIsNull[index]);
    return result;
}

// TODO: optimize
auto SqlStmtResult::get(std::string_view name) -> IoResult<SqlCellView> {
    if (mResult == nullptr || mFieldMetas.empty()) {
        return Err(SqlError::Code::NoMoreData);
    }
    if (mFieldMetas.empty()) {
        return Err(SqlError::Code::NoMoreData);
    }
    auto index = -1;
    for (size_t i = 0; i < mysql_num_fields(mResult.get()); ++i) {
        if (mFieldMetas[i]->name == name) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return Err(SqlError::Code::InvalidIndex);
    }
    return get(index);
}

auto SqlStmtResult::exactRowCount() const -> std::optional<size_t> {
    if (mResult == nullptr) {
        return std::nullopt;
    }
    return mysql_stmt_num_rows(mStmt.get());
}

auto SqlStmtResult::countFields() -> size_t {
    return mysql_stmt_field_count(mStmt.get());
}

auto SqlStmtResult::fieldName(size_t index) -> std::string_view {
    if (index >= mFieldMetas.size()) {
        return std::string_view();
    }
    return std::string_view(mFieldMetas[index]->name, mFieldMetas[index]->name_length);
}

auto SqlStmtResult::fetchRow() -> IoTask<int> {
    // ILIAS_TRACE("sql", "stmt fetch row");
    ILIAS_ASSERT(mStmt != nullptr);
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_fetch);
    co_return ret;
}

auto SqlStmtResult::storeCurrentResultIfNeeded() -> IoTask<bool> {
    if (mResult) {
        co_return true;
    }

    ILIAS_CO_TRY(auto rawResult, co_await (storeResult() | unstoppable()));
    mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
        rawResult, [](MYSQL_RES *res) { mysql_free_result(res); });
    co_return mResult != nullptr;
}

auto SqlStmtResult::advanceToNextResult() -> IoTask<bool> {
    freeResult();
    ILIAS_CO_TRY(auto status, co_await (nextResult() | unstoppable()));
    if (status == 0) {
        co_return true;
    }
    if (status == -1) {
        co_return false;
    }
    auto nativeError = captureStatementNativeError(status);
    co_return Err(MySql::nativeErrorCodeToSqlError(nativeError.code));
}

auto SqlStmtResult::freeResult() -> void {
    ILIAS_TRACE("sql", "stmt free result");
    mResult.reset();
    mFieldMetas.clear();
    mFields.clear();
    mBinds.reset();
    mLengths.reset();
    mIsNull.reset();
}

auto SqlStmtResult::execStoreResultAsync() -> IoTask<int> {
    int ret = 0;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_store_result);
    co_return ret;
}

auto SqlStmtResult::captureStatementNativeError(int fallbackCode) -> NativeSqlError {
    NativeSqlError error;
    error.backend = "mysql";
    if (mStmt) {
        error.code = mysql_stmt_errno(mStmt.get());
        if (const char *sqlstate = mysql_stmt_sqlstate(mStmt.get()); sqlstate) {
            error.sqlstate = sqlstate;
        }
        if (const char *message = mysql_stmt_error(mStmt.get()); message) {
            error.message = message;
        }
    }
    if (error.code == 0) {
        error.code = fallbackCode;
    }
    mLastNativeError = error;
    if (mMysql) {
        mMysql->setLastNativeError(error);
    }
    return error;
}

auto SqlStmtResult::getBindConfig(const MYSQL_FIELD *field) -> IoResult<BindConfig> {
    BindConfig config;
    config.isUnsigned = (field->flags & UNSIGNED_FLAG) ? 1 : 0;

    switch (field->type) {
        case MYSQL_TYPE_NULL:
            config.bufferType = MYSQL_TYPE_NULL;
            config.bufferSize = 0;
            break;

        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_FLOAT:
            config.bufferType = MYSQL_TYPE_DOUBLE;
            config.bufferSize = sizeof(double);
            break;

        case MYSQL_TYPE_LONGLONG:
#if MYSQL_VERSION_ID > 50002
        case MYSQL_TYPE_BIT:
#endif
        case MYSQL_TYPE_LONG:
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_SHORT:
        case MYSQL_TYPE_YEAR:
        case MYSQL_TYPE_TINY:
            config.bufferType = MYSQL_TYPE_LONGLONG;
            config.bufferSize = sizeof(int64_t);
            break;

        case MYSQL_TYPE_DATE:
        case MYSQL_TYPE_NEWDATE:
        case MYSQL_TYPE_DATETIME:
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_TIME:
        case MYSQL_TYPE_STRING:
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_ENUM:
        case MYSQL_TYPE_SET:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_TINY_BLOB:
        case MYSQL_TYPE_GEOMETRY:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
            config.bufferType = MYSQL_TYPE_STRING;
            // 注意：max_length 只有在 store_result 之后才准确
            config.bufferSize = field->max_length ? field->max_length : field->length;
            break;

        default:
            ILIAS_TRACE("ilias-mysql", "unsupported sql type {}", (int)field->type);
            return Err(SqlError::Code::UnsupportSqlType);
    }
    return config;
}

auto SqlStmtResult::allocateBindBuffers(MYSQL_RES *meta) -> IoResult<void> {
    unsigned int numFields = mysql_num_fields(meta);
    if (mFieldMetas.size() != numFields) {
        mFieldMetas.resize(numFields);
        mBinds   = std::make_unique<MYSQL_BIND[]>(numFields);
        mLengths = std::make_unique<unsigned long[]>(numFields);
        mIsNull  = std::make_unique<my_bool[]>(numFields);
    }
    // 同样调整 Buffer 容器的大小
    if (mFields.size() != numFields) {
        mFields.resize(numFields);
    }
    auto rawFields = mysql_fetch_fields(meta);
    // 清零是个好习惯，防止野指针
    std::memset(mBinds.get(), 0, sizeof(MYSQL_BIND) * numFields);
    std::memset(mLengths.get(), 0, sizeof(unsigned long) * numFields);
    std::memset(mIsNull.get(), 0, sizeof(my_bool) * numFields);
    std::memset(mFieldMetas.data(), 0, sizeof(st_mysql_field *) * mFieldMetas.size());

    for (unsigned int i = 0; i < numFields; ++i) {
        mFieldMetas[i] = &rawFields[i];
        // 1. 获取类型配置
        auto cfgRet = getBindConfig(mFieldMetas[i]);
        if (!cfgRet) {
            ILIAS_TRACE("ilias-mysql", "error config {}", cfgRet.error().message());
            return Err(cfgRet.error());
        }
        auto cfg = cfgRet.value();

        // 2. 填充 MYSQL_BIND 结构
        mBinds[i].buffer_type   = cfg.bufferType;
        mBinds[i].is_unsigned   = cfg.isUnsigned;
        mBinds[i].length        = &mLengths[i]; // 输出长度的指针
        mBinds[i].buffer_length = cfg.bufferSize;
        mBinds[i].is_null       = &mIsNull[i]; // 设置 NULL 指示器
        // 3. 分配实际的数据缓冲区
        // 分配内存
        mFields[i] = std::make_unique<uint8_t[]>(cfg.bufferSize + 1); // +1 为了字符串 safe
        std::memset(mFields[i].get(), 0, cfg.bufferSize + 1);

        // 绑定 Buffer 指针
        mBinds[i].buffer = mFields[i].get();
    }
    return {};
}

auto SqlStmtResult::storeResult() -> IoTask<MYSQL_RES *> {
    ILIAS_TRACE("sql", "stmt store result");
    ILIAS_ASSERT(mStmt != nullptr);
    // 1. 异步执行 store_result
    // 注意：execStoreResultAsync 现在使用新宏，如果 errno=0，它会返回非0值(如1)但不报错
    auto storeRet = co_await execStoreResultAsync();
    if (!storeRet) {
        co_return Err(storeRet.error());
    }
    // 如果 mysql_stmt_store_result 返回非0，且 errno!=0，宏会拦截返回 Err。
    // 如果 mysql_stmt_store_result 返回非0，但 errno==0 (极少见)，这里做一个额外检查：
    if (storeRet.value() != 0) {
        auto nativeError = captureStatementNativeError(storeRet.value());
        co_return Err(MySql::nativeErrorCodeToSqlError(nativeError.code));
    }
    // 2. 获取元数据
    MYSQL_RES *res = mysql_stmt_result_metadata(mStmt.get());
    if (res == nullptr) {
        auto stmtError = mysql_stmt_errno(mStmt.get());
        if (stmtError == 0) {
            co_return nullptr; // 无结果集 (INSERT/UPDATE)
        }
        auto nativeError = captureStatementNativeError(stmtError);
        co_return Err(MySql::nativeErrorCodeToSqlError(nativeError.code));
    }
    // 3. 准备缓冲区
    auto allocRet = allocateBindBuffers(res);
    if (!allocRet) {
        mysql_free_result(res);
        co_return Err(allocRet.error());
    }
    // 4. 绑定
    if (mysql_stmt_bind_result(mStmt.get(), mBinds.get()) != 0) {
        mysql_free_result(res);
        auto nativeError = captureStatementNativeError();
        co_return Err(MySql::nativeErrorCodeToSqlError(nativeError.code));
    }
    co_return res;
}

auto SqlStmtResult::nextResult() -> IoTask<int> {
    ILIAS_TRACE("sql", "stmt next result");
    ILIAS_ASSERT(mStmt != nullptr);
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_next_result);
    co_return ret;
}

auto SqlStmtResult::close() -> IoTask<bool> {
    freeResult();
    my_bool ret = 0;
    mStmt       = nullptr;
    co_return ret;
}

auto SqlStmtResult::reset() -> IoTask<bool> {
    ILIAS_ASSERT(mStmt != nullptr);
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_reset);
    co_return ret;
}

#undef SQL_PRIVATE_SYNC_CODE
ILIAS_MYSQL_NS_END
