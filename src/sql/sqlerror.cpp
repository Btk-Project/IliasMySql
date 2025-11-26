#include "ilias/sql/sqlerror.hpp"

ILIAS_SQL_NS_BEGIN

SqlError::SqlError(int64_t err, std::error_category &c) : mErr(err), mCategory(dynamic_cast<SqlErrorCategory *>(&c)) {
}
SqlError::SqlError(Code err, const std::string &message) : mErr(err), mCategory(&SqlErrorCategory::instance()) {
    if (mCategory) {
        mCategory->registerMessage(err, message);
    }
}
SqlError::SqlError(const SqlError &err) : mErr(err.mErr), mCategory(err.mCategory) {
}
SqlError::SqlError() : mErr(OK), mCategory(&SqlErrorCategory::instance()) {
}
SqlError::~SqlError() {
}
auto SqlError::isOk() const -> bool {
    return mErr == OK;
}
auto SqlError::value() const -> int64_t {
    return mErr;
}

auto SqlError::error() const -> Code {
    return (Code)mErr;
}

auto SqlError::message() const -> std::string {
    return toString();
}

auto SqlError::category() const -> const std::error_category & {
    return *mCategory;
}

auto SqlError::toString() const -> std::string {
    if (mCategory) {
        auto message = mCategory->message(mErr);
        if (message != "") {
            return message;
        }
    }
    return std::string("Unknow SQL error") + "(" + std::to_string(mErr) + ")";
}

SqlErrorCategory::SqlErrorCategory() {
}

SqlErrorCategory::~SqlErrorCategory() {
}

auto SqlErrorCategory::instance() -> SqlErrorCategory & {
    static SqlErrorCategory c;
    return c;
}

auto SqlErrorCategory::name() const noexcept -> const char * {
    return "sql";
}
ILIAS_SQL_NS_END