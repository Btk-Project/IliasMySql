/**
 * @file sqlresult.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-01-12
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

#include <chrono>

#include "detail/global.hpp"

ILIAS_SQL_NS_BEGIN

class SqlQuery;

class SqlResult {
public:
    using Date = std::chrono::system_clock::time_point;
    using ResultType =
        std::variant<std::monostate, int8_t, int32_t, int64_t, double, float, std::string, Date, std::vector<uint8_t>>;

public:
    SqlResult(SqlResult &&)            = default;
    SqlResult &operator=(SqlResult &&) = default;
    ~SqlResult()                       = default;
    SqlResult(const SqlResult &)            = delete;
    SqlResult &operator=(const SqlResult &) = delete;

    virtual auto next() -> IoTask<void>                                  = 0;
    virtual auto countRows() -> size_t                                   = 0;
    virtual auto countColumns() -> size_t                                = 0;
    virtual auto getValue(size_t index) -> IoResult<ResultType>          = 0;
    virtual auto getValue(std::string_view name) -> IoResult<ResultType> = 0;
    template <typename T>
    auto get(size_t index) -> IoResult<T>;
    template <typename T>
    auto get(std::string_view name) -> IoResult<T>;

protected:
    inline SqlResult() {}
};

template <typename T>
auto SqlResult::get(size_t index) -> IoResult<T> {
    auto val = co_await getValue(index);
    if (!val) {
        ILIAS_TRACE("sql", "no column value: {}", index);
        co_return Unexpected(val.error());
    }
    if (auto *p = std::get_if<T>(&val.value()); p) {
        co_return *p;
    }
    ILIAS_TRACE("sql", "wrong column type: {}", index);
    co_return Unexpected(std::make_error_code(std::errc::invalid_argument));
}

template <typename T>
auto SqlResult::get(std::string_view name) -> IoResult<T> {
    auto val = co_await getValue(name);
    if (!val) {
        ILIAS_TRACE("sql", "no column value: {}", name);
        co_return Unexpected(val.error());
    }
    if (auto *p = std::get_if<T>(&val.value()); p) {
        co_return *p;
    }
    ILIAS_TRACE("sql", "wrong column type: {}", name);
    co_return Unexpected(std::make_error_code(std::errc::invalid_argument));
}

ILIAS_SQL_NS_END