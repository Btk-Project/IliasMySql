#include "ilias/mysql/mysqlresult.hpp"

#include <charconv>

#define SQL_PRIVATE_SYNC_CODE(OutP, MysqlFunc, ...)                                                                    \
    auto status = MysqlFunc##_start(&OutP, mStmt.get(), ##__VA_ARGS__);                                                \
    while (status) {                                                                                                   \
        ILIAS_TRACE("sql", "{} waiting for status {}", #MysqlFunc, status);                                            \
        auto pret = co_await mMysql->pollStatus(status);                                                               \
        status    = MysqlFunc##_cont(&OutP, mStmt.get(), status);                                                      \
        if (!pret) {                                                                                                   \
            co_return Unexpected(pret.error());                                                                        \
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
        auto errCode = mMysql->lastError();                                                                            \
        if (errCode != 0) {                                                                                            \
            auto error = mMysql->lastErrorMessage();                                                                   \
            sql::SqlErrorCategory::instance().registerMessage(errCode, error);                                         \
            ILIAS_ERROR("sql", "{} failed, error({}): {}", #MysqlFunc, errCode, error);                                \
            co_return Unexpected(SqlError::Code(errCode));                                                             \
        }                                                                                                              \
    }

ILIAS_MYSQL_NS_BEGIN
auto MySqlResultBase::stmtToValue(MYSQL_FIELD *field, uint8_t *buffer, size_t bufferSize)
    -> Result<SqlValue, std::error_code> {
    SqlValue result;
    if (buffer == nullptr) {
        result.emplace<sql::SqlNull>();
        return result;
    }
    switch (field->type) {
        case MYSQL_TYPE_TINY: // char
            result.emplace<char>(*reinterpret_cast<int32_t *>(buffer));
            break;
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24:
            result.emplace<int32_t>(*reinterpret_cast<int32_t *>(buffer));
            break;
        case MYSQL_TYPE_FLOAT:
            result.emplace<float>(*reinterpret_cast<double *>(buffer));
            break;
        case MYSQL_TYPE_DOUBLE:
            result.emplace<double>(*reinterpret_cast<double *>(buffer));
            break;
        case MYSQL_TYPE_NULL:
            result.emplace<sql::SqlNull>();
            break;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
            result.emplace<sql::SqlDate>(std::string_view((char *)buffer, bufferSize));
            std::get<sql::SqlDate>(result).type = sql::SqlDate::kDateTime;
            break;
        case MYSQL_TYPE_DATE:
            result.emplace<sql::SqlDate>(std::string_view((char *)buffer, bufferSize));
            std::get<sql::SqlDate>(result).type = sql::SqlDate::kDate;
            break;
        case MYSQL_TYPE_TIME:
            result.emplace<sql::SqlDate>(std::string_view((char *)buffer, bufferSize));
            std::get<sql::SqlDate>(result).type = sql::SqlDate::kTime;
            break;
        case MYSQL_TYPE_LONGLONG: // long long
            result.emplace<int64_t>(*reinterpret_cast<int64_t *>(buffer));
            break;
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            result.emplace<std::string>(reinterpret_cast<char *>(buffer), bufferSize);
            break;
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_TINY_BLOB:
            result.emplace<std::vector<std::byte>>(
                std::vector<std::byte> {reinterpret_cast<std::byte *>((char *)buffer),
                                        reinterpret_cast<std::byte *>((char *)buffer) + bufferSize});
            break;
        default:
            return Unexpected(sql::SqlError::Code::UnknownError);
    }
    return result;
}

auto MySqlResultBase::toValue(MYSQL_FIELD *field, char *buffer, size_t bufferSize)
    -> Result<SqlValue, std::error_code> {
    SqlValue result;
    if (buffer == nullptr) {
        result.emplace<sql::SqlNull>();
        return result;
    }
    switch (field->type) {
        case MYSQL_TYPE_TINY: { // char
            int res;
            auto [ptr, ec] = std::from_chars(buffer, buffer + bufferSize, res);
            if (ec != std::errc() || ptr != buffer + bufferSize) {
                return Unexpected(sql::SqlError::UnknownError);
            }
            result.emplace<char>(res);
            break;
        }
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24: {
            int32_t res;
            auto [ptr, ec] = std::from_chars(buffer, buffer + bufferSize, res);
            if (ec != std::errc() || ptr != buffer + bufferSize) {
                return Unexpected(sql::SqlError::UnknownError);
            }
            result.emplace<int32_t>(res);
            break;
        }
        case MYSQL_TYPE_FLOAT: {
            float res;
            auto [ptr, ec] = std::from_chars(buffer, buffer + bufferSize, res);
            if (ec != std::errc() || ptr != buffer + bufferSize) {
                return Unexpected(sql::SqlError::UnknownError);
            }
            result.emplace<float>(res);
            break;
        }
        case MYSQL_TYPE_DOUBLE: {
            double res;
            auto [ptr, ec] = std::from_chars(buffer, buffer + bufferSize, res);
            if (ec != std::errc() || ptr != buffer + bufferSize) {
                return Unexpected(sql::SqlError::UnknownError);
            }
            result.emplace<double>(res);
            break;
        }
        case MYSQL_TYPE_NULL:
            result.emplace<sql::SqlNull>();
            break;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
            result.emplace<sql::SqlDate>(std::string_view(buffer));
            std::get<sql::SqlDate>(result).type = sql::SqlDate::kDateTime;
            break;
        case MYSQL_TYPE_DATE:
            result.emplace<sql::SqlDate>(std::string_view(buffer));
            std::get<sql::SqlDate>(result).type = sql::SqlDate::kDate;
            break;
        case MYSQL_TYPE_TIME:
            result.emplace<sql::SqlDate>(std::string_view(buffer));
            std::get<sql::SqlDate>(result).type = sql::SqlDate::kTime;
            break;
        case MYSQL_TYPE_LONGLONG: // long long
            result.emplace<int64_t>(std::stoll(std::string(buffer), nullptr, 10));
            break;
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            result.emplace<std::string>(reinterpret_cast<char *>(buffer), bufferSize);
            break;
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_TINY_BLOB:
            result.emplace<std::vector<std::byte>>(std::vector<std::byte> {
                reinterpret_cast<std::byte *>(buffer), reinterpret_cast<std::byte *>(buffer) + bufferSize});
            break;
        default:
            return Unexpected(sql::SqlError::Code::UnknownError);
    }
    return result;
}

SqlQueryResult::SqlQueryResult(SqlQueryResult &&other) {
    mMysql            = std::move(other.mMysql);
    mResult           = std::move(other.mResult);
    mCurrentRow       = other.mCurrentRow;
    mFieldMetas       = std::move(other.mFieldMetas);
    other.mResult     = nullptr;
    other.mCurrentRow = nullptr;
    other.mFieldMetas.clear();
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
        other.mResult     = nullptr;
        other.mCurrentRow = nullptr;
        other.mFieldMetas.clear();
    }
    return *this;
}

SqlQueryResult::SqlQueryResult(std::shared_ptr<MySql> sql) : mMysql(sql) {
}

SqlQueryResult::~SqlQueryResult() {
    if (mResult) {
        freeResult();
    }
}

auto SqlQueryResult::nativeResult() -> MYSQL_RES * {
    return mResult.get();
}

auto SqlQueryResult::getResult() -> IoTask<void> {
    auto ret = co_await (mMysql->storeResult() | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
        ret.value(), [](MYSQL_RES *res) { mysql_free_result(res); });
    ILIAS_TRACE("ilias-mysql", "Get {} rows", countRows());
    co_return {};
}

auto SqlQueryResult::next() -> IoTask<bool> {
    while (true) {
        // 1. 获取结果集
        if (!mResult) {
            auto resRet = co_await (mMysql->storeResult() | unstoppable());

            if (!resRet)
                co_return Unexpected(resRet.error()); // 真正的网络/数据库错误

            mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
                resRet.value(), [](MYSQL_RES *ptr) { mysql_free_result(ptr); });
            if (!mResult) {
                goto CHECK_NEXT;
            }
        }
        // 2. 读取行
        {
            auto rowRet = co_await fetchRow(); // fetchRow 不需要改，保持原样
            if (!rowRet)
                co_return Unexpected(rowRet.error());

            if (rowRet.value()) {
                mCurrentRow = rowRet.value();
                co_return true; // 拿到数据
            }
        }

    CHECK_NEXT:
        // 3. 当前结果集读完，释放
        freeResult();
        // 4. 检查下一个结果
        auto nextRet = co_await (mMysql->nextResult() | unstoppable());

        if (!nextRet)
            co_return Unexpected(nextRet.error()); // 真正的错误

        int status = nextRet.value();
        if (status == 0) {
            continue; // 有下一个，回到循环头部 storeResult
        }
        else if (status == -1) {
            co_return false; // 全部结束
        }
        else {
            co_return Unexpected(SqlError::Code(status));
        }
    }
}

auto SqlQueryResult::get(size_t index) -> IoResult<SqlValue> {
    if (mCurrentRow == nullptr) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    if (mResult == nullptr) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    if (mFieldMetas.empty()) {
        mFieldMetas.resize(mysql_num_fields(mResult.get()));
        auto fieldMetas = mysql_fetch_fields(mResult.get());
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (index >= mFieldMetas.size()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    auto lengths = mysql_fetch_lengths(mResult.get());
    auto result  = toValue(mFieldMetas[index], mCurrentRow[index], lengths[index]);
    return result;
}

// TODO: optimize
auto SqlQueryResult::get(std::string_view name) -> IoResult<SqlValue> {
    if (mResult == nullptr || mCurrentRow == nullptr) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    if (mFieldMetas.empty()) {
        mFieldMetas.resize(mysql_num_fields(mResult.get()));
        auto fieldMetas = mysql_fetch_fields(mResult.get());
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    std::size_t index = -1;
    for (size_t i = 0; i < mysql_num_fields(mResult.get()); ++i) {
        if (mFieldMetas[i]->name == name) {
            index = i;
            break;
        }
    }
    if (index == (std::size_t)-1) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return get(index);
}

auto SqlQueryResult::countRows() -> size_t {
    if (mResult == nullptr) {
        return 0;
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
                co_return Unexpected(pret.error());
            }
        }
    }
    co_return row;
}

auto SqlQueryResult::freeResult() -> void {
    if (mResult != nullptr) {
        mResult.reset();
        mCurrentRow = nullptr;
        mFieldMetas.clear();
    }
}

SqlStmtResult::SqlStmtResult(SqlStmtResult &&other) {
    mMysql  = std::move(other.mMysql);
    mStmt   = std::move(other.mStmt);
    mResult = std::move(other.mResult);
}

SqlStmtResult &SqlStmtResult::operator=(SqlStmtResult &&other) {
    if (mStmt) {
        close().wait();
    }
    if (this != &other) {
        mMysql  = std::move(other.mMysql);
        mStmt   = std::move(other.mStmt);
        mResult = std::move(other.mResult);
    }
    return *this;
}

SqlStmtResult::SqlStmtResult(std::shared_ptr<MySql> sql, std::shared_ptr<MYSQL_STMT> stmt) : mMysql(sql), mStmt(stmt) {
}

SqlStmtResult::~SqlStmtResult() {
}

auto SqlStmtResult::nativeResult() -> MYSQL_RES * {
    return mResult.get();
}

auto SqlStmtResult::getResult() -> IoTask<void> {
    auto ret = co_await (storeResult() | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
        ret.value(), [](MYSQL_RES *res) { mysql_free_result(res); });
    co_return {};
}

auto SqlStmtResult::next() -> IoTask<bool> {
    while (true) {
        // 1. 准备结果集 (Store & Bind)
        if (mResult == nullptr) {
            auto resRet = co_await (storeResult() | unstoppable());

            if (!resRet) {
                co_return Unexpected(resRet.error());
            }

            mResult = std::unique_ptr<MYSQL_RES, std::function<void(MYSQL_RES *)>>(
                resRet.value(), [](MYSQL_RES *res) { mysql_free_result(res); });
            if (mResult == nullptr) {
                goto CHECK_NEXT_RESULT;
            }
        }

        // 2. 抓取数据 (Fetch)
        {
            auto fetchRet = co_await fetchRow();
            if (!fetchRet)
                co_return Unexpected(fetchRet.error());

            int status = fetchRet.value();

            if (status == 0 || status == MYSQL_DATA_TRUNCATED) {
                co_return true;
            }
            else if (status == MYSQL_NO_DATA) {
                // 当前结果集读取完毕，准备进入清理流程
                // (Fall through to cleanup)
            }
            else {
                co_return Unexpected(sql::SqlError::Code(mMysql->lastError()));
            }
        }

    // 4. 检查下一个结果集 (Next Result)
    CHECK_NEXT_RESULT:
        // 3. 释放当前结果集 (Free)
        freeResult();
        // 调用 MySql::nextResult，返回 0 (有) 或 -1 (无)
        auto nextRet = co_await (nextResult() | unstoppable());

        if (!nextRet)
            co_return Unexpected(nextRet.error());

        int nextStatus = nextRet.value();
        if (nextStatus == 0) {
            continue;
        }
        else if (nextStatus == -1) {
            co_return false;
        }
        else {
            co_return Unexpected(SqlError::Code(nextStatus));
        }
    }
}

auto SqlStmtResult::get(size_t index) -> IoResult<SqlValue> {
    if (mFields.empty() || mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    if (index >= mFieldMetas.size()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    auto &currentRow = mFields[index];
    // ILIAS_TRACE("sql", "{}({}) raw data {}: {}",
    //             std::string_view(mFieldMetas[index]->name, mFieldMetas[index]->name_length),
    //             (int)mFieldMetas[index]->type, mLengths[index], (char *)currentRow.get());
    auto result = stmtToValue(mFieldMetas[index], currentRow.get(), mLengths[index]);
    return result;
}

// TODO: optimize
auto SqlStmtResult::get(std::string_view name) -> IoResult<SqlValue> {
    if (mResult == nullptr || mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    if (mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    auto index = -1;
    for (size_t i = 0; i < mysql_num_fields(mResult.get()); ++i) {
        if (mFieldMetas[i]->name == name) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return get(index);
}

auto SqlStmtResult::countRows() -> size_t {
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
    ILIAS_TRACE("sql", "stmt fetch row");
    ILIAS_ASSERT(mStmt != nullptr);
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_fetch);
    co_return ret;
}

auto SqlStmtResult::freeResult() -> void {
    ILIAS_TRACE("sql", "stmt free result");
    mResult.reset();
    // 清理绑定缓冲区
    mFields.clear(); // 释放具体数据内存
}

auto SqlStmtResult::execStoreResultAsync() -> IoTask<int> {
    int ret = 0;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_store_result);
    co_return ret;
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
            return Unexpected(SqlError::Code::UnknownError);
    }
    return config;
}

auto SqlStmtResult::allocateBindBuffers(MYSQL_RES *meta) -> IoResult<void> {
    unsigned int numFields = mysql_num_fields(meta);
    if (mFieldMetas.size() != numFields) {
        mFieldMetas.resize(numFields);
        mBinds   = std::make_unique<MYSQL_BIND[]>(numFields);
        mLengths = std::make_unique<unsigned long[]>(numFields);
    }
    // 同样调整 Buffer 容器的大小
    if (mFields.size() != numFields) {
        mFields.resize(numFields);
    }
    auto rawFields = mysql_fetch_fields(meta);
    // 清零是个好习惯，防止野指针
    std::memset(mBinds.get(), 0, sizeof(MYSQL_BIND) * numFields);
    std::memset(mLengths.get(), 0, sizeof(unsigned long) * numFields);

    for (unsigned int i = 0; i < numFields; ++i) {
        mFieldMetas[i] = &rawFields[i];

        // 1. 获取类型配置
        auto cfgRet = getBindConfig(mFieldMetas[i]);
        if (!cfgRet)
            return Unexpected(cfgRet.error());
        auto cfg = cfgRet.value();

        // 2. 填充 MYSQL_BIND 结构
        mBinds[i].buffer_type   = cfg.bufferType;
        mBinds[i].is_unsigned   = cfg.isUnsigned;
        mBinds[i].length        = &mLengths[i]; // 输出长度的指针
        mBinds[i].buffer_length = cfg.bufferSize;

        // 3. 分配实际的数据缓冲区
        if (cfg.bufferSize > 0) {
            // 分配内存
            mFields[i] = std::make_unique<uint8_t[]>(cfg.bufferSize + 1); // +1 为了字符串 safe
            std::memset(mFields[i].get(), 0, cfg.bufferSize + 1);

            // 绑定 Buffer 指针
            mBinds[i].buffer = mFields[i].get();
        }
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
        co_return Unexpected(storeRet.error());
    }
    // 如果 mysql_stmt_store_result 返回非0，且 errno!=0，宏会拦截返回 Unexpected。
    // 如果 mysql_stmt_store_result 返回非0，但 errno==0 (极少见)，这里做一个额外检查：
    if (storeRet.value() != 0) {
        co_return Unexpected((SqlError::Code)mMysql->lastError());
    }
    // 2. 获取元数据
    MYSQL_RES *res = mysql_stmt_result_metadata(mStmt.get());
    if (res == nullptr) {
        if (mMysql->lastError() == SqlError::Code::OK) {
            co_return nullptr; // 无结果集 (INSERT/UPDATE)
        }
        co_return Unexpected((SqlError::Code)mMysql->lastError());
    }
    // 3. 准备缓冲区
    auto allocRet = allocateBindBuffers(res);
    if (!allocRet) {
        mysql_free_result(res);
        co_return Unexpected(allocRet.error());
    }
    // 4. 绑定
    if (mysql_stmt_bind_result(mStmt.get(), mBinds.get()) != 0) {
        mysql_free_result(res);
        co_return Unexpected((SqlError::Code)mMysql->lastError());
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