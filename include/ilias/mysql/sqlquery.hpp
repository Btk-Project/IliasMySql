/**
 * @file sqlquery.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-01-09
 *
 * @copyright Copyright (c) 2025
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
#include <mariadb/mysqld_error.h>

#include "detail/global.hpp"
#include "detail/mysql.hpp"
#include "detail/sqlresultp.hpp"
#include "sqldatabase.hpp"
#include "sqlresult.hpp"

ILIAS_MYSQL_NS_BEGIN

class SqlQuery {
public:
    SqlQuery(SqlDatabase &mysql);
    ~SqlQuery();

    SqlQuery(SqlQuery &&) noexcept;
    SqlQuery &operator=(SqlQuery &&) noexcept;

    SqlQuery(const SqlQuery &)            = delete;
    SqlQuery &operator=(const SqlQuery &) = delete;

    auto execute(std::string_view query) -> IoTask<SqlResult>;

    auto prepare(std::string_view query) -> IoTask<void>;
    auto execute() -> IoTask<SqlResult>;
    ///> set TINYINT
    auto set(int index, signed char value) -> SqlError;
    ///> set SMALLINT
    auto set(int index, short int value) -> SqlError;
    ///> set INT
    auto set(int index, int value) -> SqlError;
    ///> set BIGINT / LONGLONG
    auto set(int index, long long int value) -> SqlError;
    ///> set FLOAT
    auto set(int index, float value) -> SqlError;
    ///> set DOUBLE
    auto set(int index, double value) -> SqlError;
    ///> set TEXT, CHAR, VARCHAR
    auto set(int index, const std::string &value) -> SqlError;
    ///> set TIME, DATE, DATETIME, TIMESTAMP
    auto set(int index, SqlDate value) -> SqlError;
    ///> set TEXT, CHAR, VARCHAR
    auto set(int index, const std::u8string &value) -> SqlError;
    ///> set value
    template <typename T>
    auto set(const std::string &name, const T &value) -> SqlError;

    ///> set TEXT, CHAR, VARCHAR, this api will not copy, Please ensure that the data is valid during execute.
    auto setView(int index, std::string_view value) -> SqlError;
    ///> set TEXT, CHAR, VARCHAR, this api will not copy, Please ensure that the data is valid during execute.
    auto setView(int index, const std::u8string_view &value) -> SqlError;
    ///> set BLOB, BINARY, VARBINARY, this api will not copy, Please ensure that the data is voalid during execute.
    auto setView(int index, std::span<const std::byte> value) -> SqlError;
    ///> set valueViwe, this api will not copy, Please ensure that the data is voalid during execute.
    template <typename T>
    auto setView(const std::string &name, const T &value) -> SqlError;

    auto clearBinds() -> void;

private:
    auto pareser(std::string_view query) -> std::string;

private:
    std::shared_ptr<detail::MySql> mMysql;
    MYSQL_STMT                    *mMysqlStmt = nullptr;
    std::vector<
        std::variant<signed char, short int, int, long long int, float, double, std::string, std::u8string, SqlDate>>
                                         mBindBuffer; // save var to continue it is life.
    std::vector<MYSQL_BIND>              mBinds;
    std::unordered_map<std::string, int> mIndexs;
};

inline SqlQuery::SqlQuery(SqlDatabase &db) : mMysql(db.mysql()) {
}

inline SqlQuery::~SqlQuery() {
    if (mMysqlStmt) {
        mysql_stmt_close(mMysqlStmt);
    }
    if (mMysql.use_count() == 1) {
        mMysql->close();
    }
}

inline SqlQuery::SqlQuery(SqlQuery &&other) noexcept : mMysql(other.mMysql) {
    mMysqlStmt       = other.mMysqlStmt;
    other.mMysqlStmt = nullptr;
}

inline SqlQuery &SqlQuery::operator=(SqlQuery &&other) noexcept {
    ILIAS_ASSERT(mMysql == other.mMysql);
    mMysqlStmt       = other.mMysqlStmt;
    other.mMysqlStmt = nullptr;
    return *this;
}

inline auto SqlQuery::execute(std::string_view query) -> IoTask<SqlResult> {
    ILIAS_ASSERT(mMysql != nullptr);
    ILIAS_TRACE("sql", "exec query {}", query);
    auto ret = co_await (mMysql->query(query) | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto sqlResult = std::make_unique<detail::SqlQueryResult>(mMysql);
    auto ret1      = co_await sqlResult->getResult();
    if (!ret1) {
        co_return Unexpected(ret1.error());
    }
    co_return SqlResult(std::move(sqlResult));
}

// this query should like "SELECT * FROM table WHERE name=:name,age=:age;"
// return query like "SELECT * FROM table WHERE name=?,age=?;"
inline auto SqlQuery::pareser(std::string_view query) -> std::string {
    mIndexs.clear();
    mBindBuffer.clear();
    mBinds.clear();
    std::string ret;
    auto        start = query.find_first_of(':');
    if (start != std::string::npos) {
        ret = query.substr(0, start);
    }
    else {
        return std::string(query);
    }
    auto end = start;
    while (start != std::string::npos) {
        end = start;
        while (end < query.size() && query[end] != ' ' && query[end] != ',' && query[end] != '\t' &&
               query[end] != '\n' && query[end] != '\r' && query[end] != ')' && query[end] != '(' &&
               query[end] != '"') {
            end++;
        }
        auto name = query.substr(start + 1, end - start - 1);
        mIndexs.emplace(name, mIndexs.size());
        ret += '?';
        start = query.find_first_of(':', end);
        ret += query.substr(end, start - end);
    }
    mBinds.resize(mIndexs.size());
    memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
    for (int i = 0; i < (int)mBinds.size(); ++i) {
        mBinds[i].buffer_type = MYSQL_TYPE_NULL;
    }
    mBindBuffer.resize(mIndexs.size());
    return ret;
}

inline auto SqlQuery::prepare(std::string_view query) -> IoTask<void> {
    if (mMysqlStmt == nullptr) {
        mMysqlStmt = mMysql->stmtInit();
    }
    int  ret;
    auto queryp = pareser(query);
    ILIAS_TRACE("sql", "prepare :{}", queryp);
    auto status = mysql_stmt_prepare_start(&ret, mMysqlStmt, queryp.data(), (unsigned long)queryp.size());
    while (status) {
        ILIAS_TRACE("sql", "stmt prepare waiting for status {}", status);
        auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
        if (!pret) {
            co_return Unexpected(pret.error());
        }
        status = mysql_stmt_prepare_cont(&ret, mMysqlStmt, status);
    }
    if (ret != 0) {
        ILIAS_ERROR("sql", "stmt failed, error: {}", mMysql->lastErrorMessage());
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return {};
}

template <typename T>
inline auto SqlQuery::set(const std::string &name, const T &value) -> SqlError {
    auto index = mIndexs.find(name);
    if (index == mIndexs.end()) {
        return SqlError::INVALID_INDEX;
    }
    return set(index->second, value);
}

template <typename T>
inline auto SqlQuery::setView(const std::string &name, const T &value) -> SqlError {
    auto index = mIndexs.find(name);
    if (index == mIndexs.end()) {
        return SqlError::INVALID_INDEX;
    }
    return setView(index->second, value);
}

inline auto SqlQuery::setView(int index, std::string_view value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type   = MYSQL_TYPE_STRING;
    bind.buffer        = const_cast<char *>(value.data());
    bind.buffer_length = (unsigned long)value.size();
    mBinds[index]      = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, const std::string &value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_STRING;
    ILIAS_ASSERT(std::get_if<std::string>(&mBindBuffer[index]));
    bind.buffer        = std::get<std::string>(mBindBuffer[index]).data();
    bind.buffer_length = (unsigned long)std::get<std::string>(mBindBuffer[index]).size();
    mBinds[index]      = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, const std::u8string &value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_STRING;
    ILIAS_ASSERT(std::get_if<std::string>(&mBindBuffer[index]));
    bind.buffer        = std::get<std::u8string>(mBindBuffer[index]).data();
    bind.buffer_length = (unsigned long)std::get<std::u8string>(mBindBuffer[index]).size();
    mBinds[index]      = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, signed char value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_TINY; // what's c++ type it will be reinterpreted?
    ILIAS_ASSERT(std::get_if<signed char>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<signed char>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, short int value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_SHORT; // what's c++ type it will be reinterpreted?
    ILIAS_ASSERT(std::get_if<short int>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<short int>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, int value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_LONG; // what's c++ type it will be reinterpreted?
    ILIAS_ASSERT(std::get_if<int>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<int>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, long long int value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_LONGLONG; // what's c++ type it will be reinterpreted?
    ILIAS_ASSERT(std::get_if<long long int>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<long long int>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, float value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_FLOAT;
    ILIAS_ASSERT(std::get_if<float>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<float>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, double value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    bind.buffer_type   = MYSQL_TYPE_DOUBLE;
    ILIAS_ASSERT(std::get_if<double>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<double>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::set(int index, SqlDate value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    mBindBuffer[index] = value;
    switch (value.time.time_type) {
        case MYSQL_TIMESTAMP_DATE:
            bind.buffer_type = MYSQL_TYPE_DATE;
            break;
        case MYSQL_TIMESTAMP_DATETIME:
            bind.buffer_type = MYSQL_TYPE_DATETIME;
            break;
        case MYSQL_TIMESTAMP_TIME:
            bind.buffer_type = MYSQL_TYPE_TIME;
            break;
        default:
            return SqlError::INVALID_PARAMETER;
    }
    ILIAS_ASSERT(std::get_if<SqlDate>(&mBindBuffer[index]));
    bind.buffer   = std::get_if<SqlDate>(&mBindBuffer[index]);
    mBinds[index] = bind;
    return SqlError::OK;
}

inline auto SqlQuery::setView(int index, const std::u8string_view &value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type   = MYSQL_TYPE_STRING;
    bind.buffer        = const_cast<char8_t *>(value.data());
    bind.buffer_length = (unsigned long)value.size();
    mBinds[index]      = bind;
    return SqlError::OK;
}

inline auto SqlQuery::setView(int index, std::span<const std::byte> value) -> SqlError {
    if (mMysqlStmt == nullptr) {
        return SqlError::NOT_PREPARED;
    }
    if (index < 0 || index >= (int)mBinds.size()) {
        return SqlError::INVALID_INDEX;
    }
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    bind.buffer_type   = MYSQL_TYPE_BLOB;
    bind.buffer        = const_cast<std::byte *>(value.data());
    bind.buffer_length = (unsigned long)value.size();
    mBinds[index]      = bind;
    return SqlError::OK;
}

inline auto SqlQuery::execute() -> IoTask<SqlResult> {
    if (mMysqlStmt == nullptr) {
        co_return Unexpected(SqlError::Code::NOT_PREPARED);
    }
    int ret = 0;
    if (mBinds.size() > 0) {
        ret = mysql_stmt_bind_param(mMysqlStmt, mBinds.data());
    }
    if (ret != 0) {
        ILIAS_ERROR("sql", "stmt bind failed. (error {}:{})", ret, mMysql->lastErrorMessage());
        co_return Unexpected((SqlError::Code)ret);
    }
    auto status = mysql_stmt_execute_start(&ret, mMysqlStmt);
    while (status) {
        ILIAS_TRACE("sql", "stmt execute waiting for status {}", status);
        auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
        if (!pret) {
            ILIAS_ERROR("sql", "stmt execute failed. (error: {})", pret.error().message());
            co_return Unexpected(pret.error());
        }
        status = mysql_stmt_execute_cont(&ret, mMysqlStmt, status);
    }
    if (ret != 0) {
        ILIAS_ERROR("sql", "stmt execute failed. (error {}:{})", ret, mMysql->lastErrorMessage());
        co_return Unexpected((SqlError::Code)ret);
    }
    auto sqlResult = std::make_unique<detail::SqlStmtResult>(mMysql, mMysqlStmt);
    auto ret1      = co_await sqlResult->getResult();
    clearBinds();
    mMysqlStmt = nullptr;
    if (!ret1) {
        co_return Unexpected(ret1.error());
    }
    co_return SqlResult(std::move(sqlResult));
}

inline auto SqlQuery::clearBinds() -> void {
    memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
    for (int i = 0; i < (int)mBinds.size(); ++i) {
        mBinds[i].buffer_type = MYSQL_TYPE_NULL;
    }
}

ILIAS_MYSQL_NS_END