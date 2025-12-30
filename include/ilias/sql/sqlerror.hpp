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
    SQL_ERROR_ENTRY(UnknownError, "Unknown error")                                                                     \
    SQL_ERROR_ENTRY(DriverNotFound, "Driver not found")                                                                \
    SQL_ERROR_ENTRY(NoMoreData, "No more data")                                                                        \
    SQL_ERROR_ENTRY(InvalidIndex, "Invalid index")                                                                     \
    SQL_ERROR_ENTRY(NotPrepared, "Statement not prepared")                                                             \
    SQL_ERROR_ENTRY(AlreadyConnected, "Already connected")                                                             \
    SQL_ERROR_ENTRY(NotConnected, "Not connected")                                                                     \
    SQL_ERROR_ENTRY(UnsupportedApi, "unsupported api")                                                                 \
    SQL_ERROR_ENTRY(DialectNotSupported, "Dialect not supported")                                                      \
    SQL_ERROR_ENTRY(InvalidParameter, "Invalid parameter")                                                             \
    SQL_ERROR_ENTRY(NullValue, "Null value")                                                                           \
    SQL_ERROR_ENTRY(DriverAlreadyRegistered, "Driver already registered")                                              \
    SQL_ERROR_ENTRY(TypeNotMatched, "Type not matched")                                                                \
    SQL_ERROR_ENTRY(DataTruncated, "Data truncated")                                                                   \
    SQL_ERROR_ENTRY(ConstraintViolation, "Constraint violation")                                                       \
    SQL_ERROR_ENTRY(PrimaryKeyViolation, "Primary key constraint violation")                                           \
    SQL_ERROR_ENTRY(UniqueConstraintViolation, "Unique constraint violation")                                          \
    SQL_ERROR_ENTRY(NotNullViolation, "Not null constraint violation")                                                 \
    SQL_ERROR_ENTRY(ForeignKeyViolation, "Foreign key constraint violation")                                           \
    SQL_ERROR_ENTRY(CheckConstraintViolation, "Check constraint violation")

class ILIAS_SQL_API SqlError {
public:
    enum Code : uint32_t {
        CustomStart = 5 << 10,
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
            return "error(" + std::to_string(value) + ")";
    }
}

inline auto make_error_code(SqlError::Code t) noexcept -> std::error_code {
    return {static_cast<int>(t), SqlErrorCategory::instance()};
}

ILIAS_SQL_NS_END
#undef SQL_ERROR_TABLE
template <>
struct std::is_error_code_enum<ILIAS_SQL_COMPLETE_NAMESPACE::SqlError::Code> : std::true_type {};