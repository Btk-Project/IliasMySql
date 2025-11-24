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
#include "sqlresult.hpp"
#include "sqlerror.hpp"

ILIAS_SQL_NS_BEGIN

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

class SqlQuery {
public:
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
    auto set(int index, const SqlDate& value) -> SqlError;
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
};
ILIAS_SQL_NS_END