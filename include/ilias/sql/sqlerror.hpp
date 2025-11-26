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

#include "ilias/sql/global/global.hpp"

ILIAS_SQL_NS_BEGIN
class SqlErrorCategory;

#define SQL_ERROR_TABLE                                                                                                \
    SQL_ERROR_ENTRY(OK, "OK")                                                                                          \
    SQL_ERROR_ENTRY(UNKNOWN_ERROR, "Unknown error")                                                                    \
    SQL_ERROR_ENTRY(DriverNotFound, "Driver not found")                                                                \
    SQL_ERROR_ENTRY(NO_MORE_DATA, "No more data")                                                                      \
    SQL_ERROR_ENTRY(INVALID_INDEX, "Invalid index")                                                                    \
    SQL_ERROR_ENTRY(NOT_PREPARED, "Statement not prepared")                                                            \
    SQL_ERROR_ENTRY(INVALID_PARAMETER, "Invalid parameter")                                                            \
    SQL_ERROR_ENTRY(ALREADY_CONNECTED, "Already connected")                                                            \
    SQL_ERROR_ENTRY(NOT_CONNECTED, "Not connected")                                                                    \
    SQL_ERROR_ENTRY(UNSUPPORTED_API, "unsupported api")

class ILIAS_SQL_API SqlError {
public:
    enum Code : uint32_t {
#define SQL_ERROR_ENTRY(code, message) code,
        SQL_ERROR_TABLE
#undef SQL_ERROR_ENTRY
    };
    SqlError(int64_t err, std::error_category &c);
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
    int               mErr      = OK;
    SqlErrorCategory *mCategory = nullptr;
};

class ILIAS_SQL_API SqlErrorCategory final : public std::error_category {
public:
    SqlErrorCategory();
    ~SqlErrorCategory();
    auto        message(int value) const -> std::string override;
    auto        name() const noexcept -> const char        *override;
    static auto instance() -> SqlErrorCategory &;

    auto registerMessage(int code, const std::string &message) -> void { mMessageTable[code] = message; }

private:
    std::unordered_map<int, std::string> mMessageTable;
};

inline auto SqlErrorCategory::message(int value) const -> std::string {
    auto it = mMessageTable.find(value);
    if (it != mMessageTable.end()) {
        return it->second;
    }
    switch (value) {
#define SQL_ERROR_ENTRY(code, message)                                                                                 \
    case SqlError::code:                                                                                               \
        return message;
        SQL_ERROR_TABLE
#undef SQL_ERROR_ENTRY
        default:
            return "";
    }
}

inline auto make_error_code(SqlError::Code t) noexcept -> std::error_code {
    return {static_cast<int>(t), SqlErrorCategory::instance()};
}

ILIAS_SQL_NS_END
#undef SQL_ERROR_TABLE
template <>
struct std::is_error_code_enum<ILIAS_SQL_COMPLETE_NAMESPACE::SqlError::Code> : std::true_type {};