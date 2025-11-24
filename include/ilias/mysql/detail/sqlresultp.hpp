#pragma once

#include <unordered_map>
#include <variant>
#include <iomanip>
#include <charconv>
#include <chrono>
#include <mariadb/mysql.h>
#include <mariadb/mysqld_error.h>

#include "global.hpp"
#include "mysql.hpp"

ILIAS_MYSQL_NS_BEGIN

#define SQL_PRIVATE_SYNC_CODE(OutP, MysqlFunc, ...)                                                                    \
    auto status = MysqlFunc##_start(&OutP, mStmt, ##__VA_ARGS__);                                                      \
    while (status) {                                                                                                   \
        ILIAS_TRACE("sql", "{} waiting for status {}", #MysqlFunc, status);                                            \
        auto pret = co_await mMysql->pollStatus(status);                                                               \
        status    = MysqlFunc##_cont(&OutP, mStmt, status);                                                            \
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
        auto errCode = mMysql->lastError().value();                                                                    \
        if (errCode != 0) {                                                                                            \
            [[maybe_unused]] auto error = mMysql->lastErrorMessage();                                                  \
            ILIAS_ERROR("sql", "{} failed, error({}): {}", #MysqlFunc, errCode, error);                                \
            co_return Unexpected(SqlError::Code(errCode));                                                             \
        }                                                                                                              \
    }

class SqlQuery;

struct SqlDate {
    inline SqlDate(int year = 0, int month = 0, int day = 0, int hour = 0, int minute = 0, int second = 0) {
        setTime(year, month, day, hour, minute, second);
    }
    inline SqlDate(struct tm *timeinfo) { setTime(timeinfo); }
    inline SqlDate(std::chrono::system_clock::time_point tp) { setTime(tp); }
    inline SqlDate(std::chrono::milliseconds timestamp) { setTime(timestamp); }
    inline SqlDate(std::string_view str, std::string_view fmt = "%Y-%m-%d %H:%M:%S") { setTime(str, fmt); }
    inline SqlDate(const MYSQL_TIME &time) { this->time = time; };

    auto setTime(std::chrono::system_clock::time_point tp) -> void;
    auto setTime(std::chrono::milliseconds timestamp) -> void;
    auto setTime(int year, int month, int day, int hour, int minute, int second) -> void;
    auto setDate(int year, int month, int day) -> void;
    auto setTime(int hour, int minute, int second) -> void;
    auto setTime(struct tm *timeinfo) -> void;
    auto setTime(std::string_view str, std::string_view fmt = "%Y-%m-%d %H:%M:%S") -> void;

    auto toString() const -> std::string;
    auto toTimestamp() const -> uint64_t;

    SqlDate operator=(const MYSQL_TIME &time) {
        this->time = time;
        return *this;
    }

    operator MYSQL_TIME() const { return time; }

    MYSQL_TIME time = {};
};

namespace detail {

using SqlArrayBuffer = std::vector<std::byte>;
using SqlResultType =
    std::variant<std::monostate, char, int32_t, int64_t, double, float, std::string, SqlDate, SqlArrayBuffer>;

class SqlResultBase {
public:
    SqlResultBase()                            = default;
    SqlResultBase(SqlResultBase &&)            = default;
    SqlResultBase &operator=(SqlResultBase &&) = default;
    ~SqlResultBase()                           = default;

    SqlResultBase(const SqlResultBase &)            = delete;
    SqlResultBase &operator=(const SqlResultBase &) = delete;

    [[nodiscard("Don't forget to use co_await")]]
    virtual auto next() -> IoTask<bool>                                = 0;
    virtual auto countRows() -> size_t                                 = 0;
    virtual auto countFields() -> size_t                               = 0;
    virtual auto fieldName(size_t index) -> std::string_view           = 0;
    virtual auto get(size_t index) -> IoResult<SqlResultType>          = 0;
    virtual auto get(std::string_view name) -> IoResult<SqlResultType> = 0;
};

class SqlQueryResult final : public SqlResultBase {
public:
    SqlQueryResult(std::shared_ptr<detail::MySql> sql);
    SqlQueryResult(SqlQueryResult &&);
    SqlQueryResult &operator=(SqlQueryResult &&);
    ~SqlQueryResult();

    [[nodiscard("Don't forget to use co_await")]]
    auto next() -> IoTask<bool> override;
    auto get(size_t index) -> IoResult<SqlResultType> override;
    auto get(std::string_view name) -> IoResult<SqlResultType> override;
    auto countRows() -> size_t override;
    auto countFields() -> size_t override;
    auto fieldName(size_t index) -> std::string_view override;

protected:
    [[nodiscard("Don't forget to use co_await")]]
    auto getResult() -> IoTask<void>;
    [[nodiscard("Don't forget to use co_await")]]
    auto fetchRow() -> IoTask<MYSQL_ROW>;
    auto freeResult() -> void;

private:
    std::shared_ptr<detail::MySql> mMysql;
    MYSQL_RES                     *mResult     = nullptr;
    MYSQL_ROW                      mCurrentRow = nullptr;
    std::vector<MYSQL_FIELD *>     mFieldMetas = {};

    friend class ::ILIAS_MYSQL_COMPLETE_NAMESPACE::SqlQuery;
};

class SqlStmtResult final : public SqlResultBase {
    struct BindConfig {
        enum_field_types bufferType;
        size_t           bufferSize;
        bool             isUnsigned;
    };

public:
    SqlStmtResult(std::shared_ptr<detail::MySql> sql, MYSQL_STMT *stmt);
    SqlStmtResult(SqlStmtResult &&);
    SqlStmtResult &operator=(SqlStmtResult &&);
    ~SqlStmtResult();

    auto next() -> IoTask<bool> override;
    auto get(size_t index) -> IoResult<SqlResultType> override;
    auto get(std::string_view name) -> IoResult<SqlResultType> override;
    auto countRows() -> size_t override;
    auto countFields() -> size_t override;
    auto fieldName(size_t index) -> std::string_view override;

protected:
    auto getResult() -> IoTask<void>;
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
    std::shared_ptr<detail::MySql>                              mMysql;
    MYSQL_STMT                                                 *mStmt       = nullptr;
    MYSQL_RES                                                  *mResult     = nullptr;
    std::vector<MYSQL_FIELD *>                                  mFieldMetas = {};
    std::unordered_map<std::string, std::unique_ptr<uint8_t[]>> mFields;
    std::unique_ptr<MYSQL_BIND[]>                               mBinds;
    std::unique_ptr<unsigned long[]>                            mLengths;

    friend class ::ILIAS_MYSQL_COMPLETE_NAMESPACE::SqlQuery;
};

inline SqlQueryResult::SqlQueryResult(SqlQueryResult &&other) {
    mMysql            = std::move(other.mMysql);
    mResult           = other.mResult;
    mCurrentRow       = other.mCurrentRow;
    mFieldMetas       = std::move(other.mFieldMetas);
    other.mResult     = nullptr;
    other.mCurrentRow = nullptr;
    other.mFieldMetas.clear();
}

inline SqlQueryResult &SqlQueryResult::operator=(SqlQueryResult &&other) {
    if (mResult) {
        freeResult();
    }
    if (this != &other) {
        mMysql            = std::move(other.mMysql);
        mResult           = other.mResult;
        mCurrentRow       = other.mCurrentRow;
        mFieldMetas       = std::move(other.mFieldMetas);
        other.mResult     = nullptr;
        other.mCurrentRow = nullptr;
        other.mFieldMetas.clear();
    }
    return *this;
}

inline SqlQueryResult::SqlQueryResult(std::shared_ptr<detail::MySql> sql) : mMysql(sql) {
}

inline SqlQueryResult::~SqlQueryResult() {
    if (mResult) {
        freeResult();
    }
}

inline auto SqlQueryResult::getResult() -> IoTask<void> {
    auto ret = co_await (mMysql->storeResult() | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mResult = ret.value();
    co_return {};
}

inline auto SqlQueryResult::next() -> IoTask<bool> {
    while (true) {
        // 1. 获取结果集
        if (!mResult) {
            auto resRet = co_await (mMysql->storeResult() | unstoppable());

            if (!resRet)
                co_return Unexpected(resRet.error()); // 真正的网络/数据库错误

            mResult = resRet.value();
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

inline auto SqlQueryResult::get(size_t index) -> IoResult<SqlResultType> {
    if (mCurrentRow == nullptr) {
        return Unexpected(SqlError::Code::NO_MORE_DATA);
    }
    if (mFieldMetas.empty()) {
        mFieldMetas.resize(mysql_num_fields(mResult));
        auto fieldMetas = mysql_fetch_fields(mResult);
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (index >= mFieldMetas.size()) {
        return Unexpected(SqlError::Code::INVALID_INDEX);
    }
    // ILIAS_TRACE("sql", "index({})({}) raw data {}", index, (int)mFieldMetas[index]->type, mCurrentRow[index]);
    SqlResultType result;
    auto          lengths = mysql_fetch_lengths(mResult);
    switch (mFieldMetas[index]->type) {
        case MYSQL_TYPE_TINY: { // char
            int res;
            auto [ptr, ec] = std::from_chars(mCurrentRow[index], mCurrentRow[index] + lengths[index], res);
            if (ec != std::errc() || ptr != mCurrentRow[index] + lengths[index]) {
                return Unexpected(SqlError::UNKNOWN_ERROR);
            }
            result.emplace<char>(res);
            break;
        }
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24: {
            int32_t res;
            auto [ptr, ec] = std::from_chars(mCurrentRow[index], mCurrentRow[index] + lengths[index], res);
            if (ec != std::errc() || ptr != mCurrentRow[index] + lengths[index]) {
                return Unexpected(SqlError::UNKNOWN_ERROR);
            }
            result.emplace<int32_t>(res);
            break;
        }
        case MYSQL_TYPE_FLOAT: {
            float res;
            auto [ptr, ec] = std::from_chars(mCurrentRow[index], mCurrentRow[index] + lengths[index], res);
            if (ec != std::errc() || ptr != mCurrentRow[index] + lengths[index]) {
                return Unexpected(SqlError::UNKNOWN_ERROR);
            }
            result.emplace<float>(res);
            break;
        }
        case MYSQL_TYPE_DOUBLE: {
            double res;
            auto [ptr, ec] = std::from_chars(mCurrentRow[index], mCurrentRow[index] + lengths[index], res);
            if (ec != std::errc() || ptr != mCurrentRow[index] + lengths[index]) {
                return Unexpected(SqlError::UNKNOWN_ERROR);
            }
            result.emplace<double>(res);
            break;
        }
        case MYSQL_TYPE_NULL:
            result.emplace<std::monostate>();
            break;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
            result.emplace<SqlDate>(std::string_view(mCurrentRow[index]));
            std::get<SqlDate>(result).time.time_type = MYSQL_TIMESTAMP_DATETIME;
            break;
        case MYSQL_TYPE_DATE:
            result.emplace<SqlDate>(std::string_view(mCurrentRow[index]));
            std::get<SqlDate>(result).time.time_type = MYSQL_TIMESTAMP_DATE;
            break;
        case MYSQL_TYPE_TIME:
            result.emplace<SqlDate>(std::string_view(mCurrentRow[index]), "%H:%M:%S");
            std::get<SqlDate>(result).time.time_type = MYSQL_TIMESTAMP_TIME;
            break;
        case MYSQL_TYPE_LONGLONG: // long long
            result.emplace<int64_t>(std::stoll(std::string(mCurrentRow[index]), nullptr, 10));
            break;
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            result.emplace<std::string>(reinterpret_cast<char *>(mCurrentRow[index]), lengths[index]);
            break;
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_TINY_BLOB:
            result.emplace<std::vector<std::byte>>(
                std::vector<std::byte> {reinterpret_cast<std::byte *>(mCurrentRow[index]),
                                        reinterpret_cast<std::byte *>(mCurrentRow[index]) + lengths[index]});
            break;
        default:
            return Unexpected(SqlError::Code::UNKNOWN_ERROR);
    }
    return result;
}

// TODO: optimize
inline auto SqlQueryResult::get(std::string_view name) -> IoResult<SqlResultType> {
    if (mResult == nullptr || mCurrentRow == nullptr) {
        return Unexpected(SqlError::Code::NO_MORE_DATA);
    }
    if (mFieldMetas.empty()) {
        mFieldMetas.resize(mysql_num_fields(mResult));
        auto fieldMetas = mysql_fetch_fields(mResult);
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NO_MORE_DATA);
    }
    std::size_t index = -1;
    for (size_t i = 0; i < mysql_num_fields(mResult); ++i) {
        if (mFieldMetas[i]->name == name) {
            index = i;
            break;
        }
    }
    if (index == (std::size_t)-1) {
        return Unexpected(SqlError::Code::INVALID_INDEX);
    }
    return get(index);
}

inline auto SqlQueryResult::countRows() -> size_t {
    if (mResult == nullptr) {
        return 0;
    }
    return mysql_num_rows(mResult);
}

inline auto SqlQueryResult::countFields() -> size_t {
    if (mResult == nullptr) {
        return 0;
    }
    return mysql_num_fields(mResult);
}

inline auto SqlQueryResult::fieldName(size_t index) -> std::string_view {
    if (mFieldMetas.empty() && mResult != nullptr) {
        mFieldMetas.resize(mysql_num_fields(mResult));
        auto fieldMetas = mysql_fetch_fields(mResult);
        for (size_t i = 0; i < mFieldMetas.size(); ++i) {
            mFieldMetas[i] = &fieldMetas[i];
        }
    }
    if (index >= mFieldMetas.size()) {
        return {};
    }
    return mFieldMetas[index]->name;
}

inline auto SqlQueryResult::fetchRow() -> IoTask<MYSQL_ROW> {
    ILIAS_ASSERT(mResult != nullptr);
    MYSQL_ROW row;
    auto      status = mysql_fetch_row_start(&row, mResult);
    if (status) {
        while (status) {
            ILIAS_TRACE("sql", "disconnect mysql waiting for status {}", status);
            auto pret = co_await mMysql->pollStatus(status);
            status    = mysql_fetch_row_cont(&row, mResult, status);
            if (!pret) {
                co_return Unexpected(pret.error());
            }
        }
    }
    co_return row;
}

inline auto SqlQueryResult::freeResult() -> void {
    if (mResult != nullptr) {
        mysql_free_result(mResult);
        mResult     = nullptr;
        mCurrentRow = nullptr;
        mFieldMetas.clear();
    }
}

inline SqlStmtResult::SqlStmtResult(SqlStmtResult &&other) {
    mMysql        = std::move(other.mMysql);
    mStmt         = other.mStmt;
    mResult       = other.mResult;
    other.mStmt   = nullptr;
    other.mResult = nullptr;
}

inline SqlStmtResult &SqlStmtResult::operator=(SqlStmtResult &&other) {
    if (mStmt) {
        close().wait();
    }
    if (this != &other) {
        mMysql        = std::move(other.mMysql);
        mStmt         = other.mStmt;
        mResult       = other.mResult;
        other.mStmt   = nullptr;
        other.mResult = nullptr;
    }
    return *this;
}

inline SqlStmtResult::SqlStmtResult(std::shared_ptr<detail::MySql> sql, MYSQL_STMT *stmt) : mMysql(sql), mStmt(stmt) {
}

inline SqlStmtResult::~SqlStmtResult() {
    if (mResult)
        mysql_free_result(mResult);
}

inline auto SqlStmtResult::getResult() -> IoTask<void> {
    auto ret = co_await (storeResult() | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mResult = ret.value();
    co_return {};
}

inline auto SqlStmtResult::next() -> IoTask<bool> {
    while (true) {
        // 1. 准备结果集 (Store & Bind)
        if (mResult == nullptr) {
            auto resRet = co_await (storeResult() | unstoppable());

            if (!resRet) {
                co_return Unexpected(resRet.error());
            }

            mResult = resRet.value();
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
                co_return Unexpected(mMysql->lastError().error());
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

inline auto SqlStmtResult::get(size_t index) -> IoResult<SqlResultType> {
    if (mFields.empty() || mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NO_MORE_DATA);
    }
    if (index >= mFieldMetas.size()) {
        return Unexpected(SqlError::Code::INVALID_INDEX);
    }
    auto &currentRow = mFields[mFieldMetas[index]->name];
    // ILIAS_TRACE("sql", "{}({}) raw data {}: {}",
    //             std::string_view(mFieldMetas[index]->name, mFieldMetas[index]->name_length),
    //             (int)mFieldMetas[index]->type, mLengths[index], (char *)currentRow.get());
    SqlResultType result;
    switch (mFieldMetas[index]->type) {
        case MYSQL_TYPE_TINY: // char
            result.emplace<char>(*reinterpret_cast<int32_t *>(currentRow.get()));
            break;
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24:
            result.emplace<int32_t>(*reinterpret_cast<int32_t *>(currentRow.get()));
            break;
        case MYSQL_TYPE_FLOAT:
            result.emplace<float>(*reinterpret_cast<double *>(currentRow.get()));
            break;
        case MYSQL_TYPE_DOUBLE:
            result.emplace<double>(*reinterpret_cast<double *>(currentRow.get()));
            break;
        case MYSQL_TYPE_NULL:
            result.emplace<std::monostate>();
            break;
        case MYSQL_TYPE_TIMESTAMP:
        case MYSQL_TYPE_DATETIME:
            result.emplace<SqlDate>(std::string_view((char *)currentRow.get(), mLengths[index]));
            std::get<SqlDate>(result).time.time_type = MYSQL_TIMESTAMP_DATETIME;
            break;
        case MYSQL_TYPE_DATE:
            result.emplace<SqlDate>(std::string_view((char *)currentRow.get(), mLengths[index]));
            std::get<SqlDate>(result).time.time_type = MYSQL_TIMESTAMP_DATE;
            break;
        case MYSQL_TYPE_TIME:
            result.emplace<SqlDate>(std::string_view((char *)currentRow.get(), mLengths[index]), "%H:%M:%S");
            std::get<SqlDate>(result).time.time_type = MYSQL_TIMESTAMP_TIME;
            break;
        case MYSQL_TYPE_LONGLONG: // long long
            result.emplace<int64_t>(*reinterpret_cast<int64_t *>(currentRow.get()));
            break;
        case MYSQL_TYPE_VARCHAR:
        case MYSQL_TYPE_VAR_STRING:
        case MYSQL_TYPE_STRING:
            result.emplace<std::string>(reinterpret_cast<char *>(currentRow.get()), mLengths[index]);
            break;
        case MYSQL_TYPE_BLOB:
        case MYSQL_TYPE_LONG_BLOB:
        case MYSQL_TYPE_MEDIUM_BLOB:
        case MYSQL_TYPE_TINY_BLOB:
            result.emplace<std::vector<std::byte>>(
                std::vector<std::byte> {reinterpret_cast<std::byte *>((char *)currentRow.get()),
                                        reinterpret_cast<std::byte *>((char *)currentRow.get()) + mLengths[index]});
            break;
        default:
            return Unexpected(SqlError::Code::UNKNOWN_ERROR);
    }
    return result;
}

// TODO: optimize
inline auto SqlStmtResult::get(std::string_view name) -> IoResult<SqlResultType> {
    if (mResult == nullptr || mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NO_MORE_DATA);
    }
    if (mFieldMetas.empty()) {
        return Unexpected(SqlError::Code::NO_MORE_DATA);
    }
    auto index = -1;
    for (size_t i = 0; i < mysql_num_fields(mResult); ++i) {
        if (mFieldMetas[i]->name == name) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return Unexpected(SqlError::Code::INVALID_INDEX);
    }
    return get(index);
}

inline auto SqlStmtResult::countRows() -> size_t {
    return mysql_stmt_num_rows(mStmt);
}

inline auto SqlStmtResult::countFields() -> size_t {
    return mysql_stmt_field_count(mStmt);
}

inline auto SqlStmtResult::fieldName(size_t index) -> std::string_view {
    if (index >= mFieldMetas.size()) {
        return std::string_view();
    }
    return std::string_view(mFieldMetas[index]->name, mFieldMetas[index]->name_length);
}

inline auto SqlStmtResult::fetchRow() -> IoTask<int> {
    ILIAS_INFO("sql", "stmt fetch row");
    ILIAS_ASSERT(mStmt != nullptr);
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_fetch);
    co_return ret;
}

inline auto SqlStmtResult::freeResult() -> void {
    ILIAS_INFO("sql", "stmt free result");
    if (mResult) {
        mysql_free_result(mResult);
        mResult = nullptr;
    }

    if (mStmt) {
        mysql_stmt_free_result(mStmt);
    }

    // 清理绑定缓冲区
    mBinds.reset();
    mLengths.reset();
    mFieldMetas.clear();
    mFields.clear(); // 释放具体数据内存
}

inline auto SqlStmtResult::execStoreResultAsync() -> IoTask<int> {
    int ret = 0;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_store_result);
    co_return ret;
}

inline auto SqlStmtResult::getBindConfig(const MYSQL_FIELD *field) -> IoResult<BindConfig> {
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
            return Unexpected(SqlError::Code::UNKNOWN_ERROR);
    }
    return config;
}

inline auto SqlStmtResult::allocateBindBuffers(MYSQL_RES *meta) -> IoResult<void> {
    unsigned int numFields = mysql_num_fields(meta);
    mFieldMetas.resize(numFields);
    auto rawFields = mysql_fetch_fields(meta);

    // 重新分配绑定数组
    mBinds   = std::make_unique<MYSQL_BIND[]>(numFields);
    mLengths = std::make_unique<unsigned long[]>(numFields);

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
            // 建议：mFields 如果是 map<string, ...> 会比较慢且内存不连续
            // 这里仅仅是复原你的逻辑
            auto &fieldName = mFieldMetas[i]->name;

            // 分配内存
            mFields[fieldName] = std::make_unique<uint8_t[]>(cfg.bufferSize + 1); // +1 为了字符串 safe
            std::memset(mFields[fieldName].get(), 0, cfg.bufferSize + 1);

            // 绑定 Buffer 指针
            mBinds[i].buffer = mFields[fieldName].get();
        }
    }
    return {};
}

inline auto SqlStmtResult::storeResult() -> IoTask<MYSQL_RES *> {
    ILIAS_INFO("sql", "stmt store result");
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
        co_return Unexpected(mMysql->lastError().error());
    }
    // 2. 获取元数据
    MYSQL_RES *res = mysql_stmt_result_metadata(mStmt);
    if (res == nullptr) {
        if (mMysql->lastError().isOk()) {
            co_return nullptr; // 无结果集 (INSERT/UPDATE)
        }
        co_return Unexpected(mMysql->lastError().error());
    }
    // 3. 准备缓冲区
    auto allocRet = allocateBindBuffers(res);
    if (!allocRet) {
        mysql_free_result(res);
        co_return Unexpected(allocRet.error());
    }
    // 4. 绑定
    if (mysql_stmt_bind_result(mStmt, mBinds.get()) != 0) {
        mysql_free_result(res);
        co_return Unexpected(mMysql->lastError().error());
    }
    co_return res;
}

inline auto SqlStmtResult::nextResult() -> IoTask<int> {
    ILIAS_INFO("sql", "stmt next result");
    ILIAS_ASSERT(mStmt != nullptr);
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_next_result);
    co_return ret;
}

inline auto SqlStmtResult::close() -> IoTask<bool> {
    if (mStmt == nullptr) {
        co_return true;
    }
    freeResult();
    my_bool ret = 0;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_close);
    mStmt = nullptr;
    co_return ret;
}

inline auto SqlStmtResult::reset() -> IoTask<bool> {
    ILIAS_ASSERT(mStmt != nullptr);
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stmt_reset);
    co_return ret;
}
} // namespace detail

inline auto SqlDate::toString() const -> std::string {
    if (time.time_type == MYSQL_TIMESTAMP_DATETIME) {
        return std::to_string(time.year) + "-" + std::to_string(time.month) + "-" + std::to_string(time.day) + " " +
               std::to_string(time.hour) + ":" + std::to_string(time.minute) + ":" + std::to_string(time.second);
    }
    else if (time.time_type == MYSQL_TIMESTAMP_TIME) {
        return std::to_string(time.hour) + ":" + std::to_string(time.minute) + ":" + std::to_string(time.second);
    }
    else if (time.time_type == MYSQL_TIMESTAMP_DATE) {
        return std::to_string(time.year) + "-" + std::to_string(time.month) + "-" + std::to_string(time.day);
    }
    else {
        return "error time";
    }
}

inline auto SqlDate::toTimestamp() const -> uint64_t {
    return time.year * 31536000 + time.month * 2592000 + time.day * 86400 + time.hour * 3600 + time.minute * 60 +
           time.second;
}

inline auto SqlDate::setTime(std::chrono::system_clock::time_point tp) -> void {
    auto     tt  = std::chrono::system_clock::to_time_t(tp);
    std::tm *now = gmtime(&tt);
    setTime(now);
}

inline auto SqlDate::setTime(std::chrono::milliseconds timestamp) -> void {
    auto     milliseconds = std::chrono::milliseconds(timestamp);
    auto     tp           = std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(milliseconds);
    auto     tt           = std::chrono::system_clock::to_time_t(tp);
    std::tm *now          = gmtime(&tt);
    setTime(now);
}

inline auto SqlDate::setTime(int year_, int month_, int day_, int hour_, int minute_, int second_) -> void {
    if (year_ < 0 || month_ < 0 || month_ > 12 || day_ < 0 || day_ > 31 || hour_ < 0 || hour_ > 23 || minute_ < 0 ||
        minute_ > 59 || second_ < 0 || second_ > 59) {
        time.time_type = MYSQL_TIMESTAMP_ERROR;
        ILIAS_ERROR("sql", "error date time set {}-{}-{} {}:{}:{}", year_, month_, day_, hour_, minute_, second_);
        return;
    }
    time.year        = year_;
    time.month       = month_;
    time.day         = day_;
    time.hour        = hour_;
    time.minute      = minute_;
    time.second      = second_;
    time.time_type   = MYSQL_TIMESTAMP_DATETIME;
    time.second_part = 0;
    time.neg         = 0;
}

inline auto SqlDate::setDate(int year, int month, int day) -> void {
    if (year < 0 || month < 0 || month > 12 || day < 0 || day > 31) {
        time.time_type = MYSQL_TIMESTAMP_ERROR;
        ILIAS_ERROR("sql", "error date set {}-{}-{}", year, month, day);
        return;
    }
    memset(&time, 0, sizeof(time));
    time.year      = year;
    time.month     = month;
    time.day       = day;
    time.time_type = MYSQL_TIMESTAMP_DATE;
}

inline auto SqlDate::setTime(int hour, int minute, int second) -> void {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) {
        time.time_type = MYSQL_TIMESTAMP_ERROR;
        ILIAS_ERROR("sql", "error time set {}:{}:{}", hour, minute, second);
        return;
    }
    memset(&time, 0, sizeof(time));
    time.hour      = hour;
    time.minute    = minute;
    time.second    = second;
    time.time_type = MYSQL_TIMESTAMP_TIME;
}

inline auto SqlDate::setTime(struct tm *timeinfo) -> void {
    time.year        = timeinfo->tm_year;
    time.month       = timeinfo->tm_mon;
    time.day         = timeinfo->tm_mday;
    time.hour        = timeinfo->tm_hour;
    time.minute      = timeinfo->tm_min;
    time.second      = timeinfo->tm_sec;
    time.time_type   = MYSQL_TIMESTAMP_DATETIME;
    time.second_part = 0;
    time.neg         = 0;
}

inline auto SqlDate::setTime(std::string_view str, std::string_view fmt) -> void {
    if (fmt == "") {
        fmt = "%Y-%m-%d %H:%M:%S";
    }
    struct tm timeinfo;
    memset(&timeinfo, 0, sizeof(struct tm));
    std::istringstream istr((std::string(str)));
    istr >> std::get_time(&timeinfo, std::string(fmt).c_str());
    timeinfo.tm_year += 1900;
    timeinfo.tm_mon += 1;
    setTime(&timeinfo);
}
#undef SQL_PRIVATE_SYNC_CODE
ILIAS_MYSQL_NS_END