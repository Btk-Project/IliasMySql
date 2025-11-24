/**
 * @file sqlerror.hpp
 * @author llhsdmd(llhsdmd@gmail.com)
 * @brief
 * @version 0.1
 * @date 2025-01-09
 *
 * @copyright Copyright (c) 2025
 *
 */
#pragma once
#include <system_error>
#include <stdint.h>

#include "detail/global.hpp"

ILIAS_SQL_NS_BEGIN
class SqlErrorCategory;

#define SQL_ERROR_TABLE                                                                                                \
    SQL_ERROR_ENTRY(OK, "OK")                                                                                          \
    SQL_ERROR_ENTRY(DriverNotFound, "Driver not found")

class SqlError {
public:
    enum Code : uint32_t {
#define SQL_ERROR_ENTRY(code, message) code,
        SQL_ERROR_TABLE
#undef SQL_ERROR_ENTRY
    };
    SqlError(int64_t err, const std::error_category &c);
    SqlError(Code err, const std::string &message = "");
    SqlError(const SqlError &err);
    SqlError();
    ~SqlError();
    auto isOk() const -> bool;
    auto value() const -> int64_t;
    auto message() const -> std::string;
    auto category() const -> const std::error_category &;
    auto toString() const -> std::string;
    auto error() const -> Code;
    auto operator=(const SqlError &) -> SqlError & = default;

private:
    int                        mErr      = OK;
    const std::error_category *mCategory = nullptr;
    std::string                mMessage;
};

class SqlErrorCategory final : public std::error_category {
public:
    SqlErrorCategory();
    ~SqlErrorCategory();
    auto        message(int value) const -> std::string override;
    auto        name() const noexcept -> const char        *override;
    static auto instance() -> const SqlErrorCategory &;
};

inline SqlError::SqlError(int64_t err, const std::error_category &c) : mErr(err), mCategory(&c) {
    switch (mErr) {
#define SQL_ERROR_ENTRY(code, message)                                                                                 \
    case code:                                                                                                         \
        mMessage = message;                                                                                            \
        break;
        SQL_ERROR_TABLE
#undef SQL_ERROR_ENTRY
        default:
            mMessage = "Unknow SQL error";
            break;
    }
}
inline SqlError::SqlError(Code err, const std::string &message)
    : mErr(err), mCategory(&SqlErrorCategory::instance()), mMessage(message) {
}
inline SqlError::SqlError(const SqlError &err) : mErr(err.mErr), mCategory(err.mCategory), mMessage(err.mMessage) {
}
inline SqlError::SqlError() : mErr(OK), mCategory(&SqlErrorCategory::instance()) {
}
inline SqlError::~SqlError() {
}
inline auto SqlError::isOk() const -> bool {
    return mErr == OK;
}
inline auto SqlError::value() const -> int64_t {
    return mErr;
}

inline auto SqlError::error() const -> Code {
    return (Code)mErr;
}

inline auto SqlError::message() const -> std::string {
    if (mMessage != "") {
        return mMessage;
    }
    return std::string("Unknow SQL error") + "(" + std::to_string(mErr) + ")";
}

inline auto SqlError::category() const -> const std::error_category & {
    return *mCategory;
}

inline auto SqlError::toString() const -> std::string {
    if (mMessage != "") {
        return mMessage;
    }
    return std::string("Unknow SQL error") + "(" + std::to_string(mErr) + ")";
}

inline SqlErrorCategory::SqlErrorCategory() {
}

inline SqlErrorCategory::~SqlErrorCategory() {
}

inline auto SqlErrorCategory::instance() -> const SqlErrorCategory & {
    static SqlErrorCategory c;
    return c;
}

inline auto SqlErrorCategory::message(int value) const -> std::string {
    return SqlError(value, *this).message();
}

inline auto SqlErrorCategory::name() const noexcept -> const char * {
    return "sql";
}

inline auto make_error_code(SqlError::Code t) noexcept -> std::error_code {
    return {static_cast<int>(t), SqlErrorCategory::instance()};
}

ILIAS_SQL_NS_END
#undef SQL_ERROR_TABLE
template <>
struct std::is_error_code_enum<ILIAS_SQL_COMPLETE_NAMESPACE::SqlError::Code> : std::true_type {};