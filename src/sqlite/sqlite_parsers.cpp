#include "ilias/sqlite/sqlite_parsers.hpp"

#include "ilias/sqlite/sqlite.hpp"

#include "ilias/sql/sqlerror.hpp"

ILIAS_SQLITE_NS_BEGIN

static auto sqliteBinderErrorCode(int code) -> SqlError::Code {
    switch (code & 0xff) {
        case SQLITE_RANGE:
            return SqlError::Code::InvalidIndex;
        case SQLITE_MISUSE:
            return SqlError::Code::InvalidParameter;
        case SQLITE_TOOBIG:
            return SqlError::Code::DataTruncated;
        default:
            return SqlError::Code::UnknownError;
    }
}

SqlParserResult sqlite_parse_null(const SqlCellView &cell) {
    if (cell.is_null()) {
        return std::any(g_sql_null);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_NULL) {
            return std::any(g_sql_null);
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief (兜底) 将任何 SQLite 单元格的值解析为 std::string。
 *
 * @param cell 单元格视图。
 * @return 包含 std::string 的 Result，如果单元格为 NULL 则返回错误。
 */
SqlParserResult sqlite_parse_string(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        switch (type) {
            case SQLITE_TEXT:
                return std::any(SqlText(reinterpret_cast<const char *>(sqlite3_value_text(value))));
            case SQLITE_INTEGER:
                return std::any((SqlText)std::to_string(sqlite3_value_int64(value)));
            case SQLITE_FLOAT:
                return std::any((SqlText)std::to_string(sqlite3_value_double(value)));
            case SQLITE_BLOB:
                return std::any(
                    SqlText(reinterpret_cast<const char *>(sqlite3_value_blob(value)), sqlite3_value_bytes(value)));
            case SQLITE_NULL:
                return Err(SqlError::Code::NullValue);
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief (兜底) 将任何 SQLite 单元格的值解析为 std::string_view。
 *
 * @param cell 单元格视图。
 * @return 包含 std::string_view 的 Result，如果单元格为 NULL 则返回错误。
 */
SqlParserResult sqlite_parse_string_view(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        switch (type) {
            case SQLITE_TEXT:
                return std::any(SqlTextView(reinterpret_cast<const char *>(sqlite3_value_text(value))));
            case SQLITE_BLOB:
                return std::any(
                    SqlTextView(reinterpret_cast<const char *>(sqlite3_value_blob(value)), sqlite3_value_bytes(value)));
            case SQLITE_NULL:
                return Err(SqlError::Code::NullValue);
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的文本表示解析为 int。
 *
 * @param cell 单元格视图，应为 kString 格式。
 * @return 包含 int 的 Result，如果格式错误或为 NULL 则返回错误。
 */
template <typename T>
    requires std::is_integral_v<T>
SqlParserResult sqlite_parse_int(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_INTEGER) {
            return std::any(static_cast<T>(sqlite3_value_int64(value)));
        }
        else if (type == SQLITE_NULL) {
            return Err(SqlError::Code::NullValue);
        }
        else if (type == SQLITE_FLOAT) {
            return std::any(static_cast<T>(sqlite3_value_double(value)));
        }
        else if (type == SQLITE_TEXT) {
            auto text = reinterpret_cast<const char *>(sqlite3_value_text(value));
            auto len  = sqlite3_value_bytes(value);
            T    rvalue;
            auto ret = std::from_chars(text, text + len, rvalue);
            if (ret.ec == std::errc()) {
                return std::any(rvalue);
            }
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的文本表示解析为 double。
 *
 * @param cell 单元格视图，应为 kString 格式。
 * @return 包含 double 的 Result，如果格式错误或为 NULL 则返回错误。
 */
template <typename T>
    requires std::is_floating_point_v<T>
SqlParserResult sqlite_parse_real(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_FLOAT) {
            return std::any(static_cast<T>(sqlite3_value_double(value)));
        }
        else if (type == SQLITE_INTEGER) {
            return std::any(static_cast<T>(sqlite3_value_int64(value)));
        }
        else if (type == SQLITE_NULL) {
            return Err(SqlError::Code::NullValue);
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的二进制数据解析为 BLOB (std::span<const std::byte>)。
 *
 * @param cell 单元格视图，应为 kBinary 格式。
 * @return 包含 std::span<const std::byte> 的 Result，如果为 NULL 或格式错误则返回错误。
 */
SqlParserResult sqlite_parse_blob(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_BLOB) {
            auto  blob_size = sqlite3_value_bytes(value);
            auto *blob_ptr  = reinterpret_cast<const std::byte *>(sqlite3_value_blob(value));
            return std::any(std::span<const std::byte>(blob_ptr, blob_size));
        }
        else if (type == SQLITE_NULL) {
            return Err(SqlError::Code::NullValue);
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的文本表示解析为 bool。
 *        SQLite 中 bool 通常存储为整数 0 或 1。
 *
 * @param cell 单元格视图，应为 kString 格式。
 * @return 包含 bool 的 Result，如果格式错误或为 NULL 则返回错误。
 */
SqlParserResult sqlite_parse_bool(const SqlCellView &cell) {
    auto ret = sqlite_parse_int<int>(cell);
    if (!ret) {
        return Err(ret.error());
    }
    return std::any(std::any_cast<int>(ret.value()) != 0);
}

SqlParserResult sqlite_parse_date(const SqlCellView &cell) {
    auto value_str = sqlite_parse_string_view(cell);
    if (!value_str) {
        return Err(value_str.error());
    }
    auto any_string = std::any_cast<std::string_view>(&(*value_str));
    if (any_string == nullptr) {
        return Err(SqlError::Code::UnsupportConvertFromSqlType);
    }
    SqlDate date;
    date.fromUTCString(*any_string);
    return std::any(date);
}

SqlBinderResult sqlite_bind_null(const SqlCellView &cell, std::any data) {
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Err(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (int ret = sqlite3_bind_null(nativeSqliteStmt, cell.index()); ret != SQLITE_OK) {
        return Err(sqliteBinderErrorCode(ret));
    }
    return make_null_sql_binder_result();
}

SqlBinderResult sqlite_bind_string(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Err(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const char *))) {
        return Err(SqlError::Code::InvalidDataFormat);
    }
    if (int ret = sqlite3_bind_text(nativeSqliteStmt, cell.index(), reinterpret_cast<const char *>(cell.raw_value()),
                                    cell.raw_value_size(), SQLITE_STATIC);
        ret != SQLITE_OK) {
        return Err(sqliteBinderErrorCode(ret));
    }
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_integral_v<T>
SqlBinderResult sqlite_bind_int(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Err(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Err(SqlError::Code::InvalidDataFormat);
    }
    if (int ret = sqlite3_bind_int64(nativeSqliteStmt, cell.index(), *reinterpret_cast<const T *>(cell.raw_value()));
        ret != SQLITE_OK) {
        return Err(sqliteBinderErrorCode(ret));
    }
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_floating_point_v<T>
SqlBinderResult sqlite_bind_real(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Err(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Err(SqlError::Code::InvalidDataFormat);
    }
    if (int ret = sqlite3_bind_double(nativeSqliteStmt, cell.index(), *reinterpret_cast<const T *>(cell.raw_value()));
        ret != SQLITE_OK) {
        return Err(sqliteBinderErrorCode(ret));
    }
    return make_null_sql_binder_result();
}

SqlBinderResult sqlite_bind_blob(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Err(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const std::byte *))) {
        return Err(SqlError::Code::InvalidDataFormat);
    }
    if (int ret =
            sqlite3_bind_blob(nativeSqliteStmt, cell.index(), cell.raw_value(), cell.raw_value_size(), SQLITE_STATIC);
        ret != SQLITE_OK) {
        return Err(sqliteBinderErrorCode(ret));
    }
    return make_null_sql_binder_result();
}

SqlBinderResult sqlite_bind_date(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Err(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const SqlDate))) {
        return Err(SqlError::Code::InvalidDataFormat);
    }
    auto date = reinterpret_cast<const SqlDate *>(cell.raw_value());
    if (date == nullptr || date->type == SqlDate::kErrorTime) {
        return sqlite_bind_null(cell, data);
    }
    auto dateStr = date->toUTCString();
    if (int ret = sqlite3_bind_text(nativeSqliteStmt, cell.index(), dateStr.c_str(), dateStr.size(), SQLITE_TRANSIENT);
        ret != SQLITE_OK) {
        return Err(sqliteBinderErrorCode(ret));
    }
    return make_null_sql_binder_result();
}

void registerSqliteTypeParsers(SqlValueConverterContext &context) {
    context.registerType<SqlNull>(sqlite_parse_null);
    context.registerType<SqlNull>(sqlite_bind_null);
    context.registerType<SqlBool>(sqlite_parse_bool);
    context.registerType<SqlBool>(sqlite_bind_int<SqlBool>);
    context.registerType<SqlTinyInt>(sqlite_parse_int<SqlTinyInt>);
    context.registerType<char>(sqlite_parse_int<char>);
    context.registerType<SqlTinyInt>(sqlite_bind_int<SqlTinyInt>);
    context.registerType<char>(sqlite_bind_int<char>);
    context.registerType<SqlInt>(sqlite_parse_int<SqlInt>);
    context.registerType<SqlInt>(sqlite_bind_int<SqlInt>);
    context.registerType<SqlBigInt>(sqlite_parse_int<SqlBigInt>);
    context.registerType<SqlBigInt>(sqlite_bind_int<SqlBigInt>);
    context.registerType<SqlFloat>(sqlite_parse_real<SqlFloat>);
    context.registerType<SqlFloat>(sqlite_bind_real<SqlFloat>);
    context.registerType<double>(sqlite_parse_real<double>);
    context.registerType<double>(sqlite_bind_real<double>);
    context.registerType<SqlText>(sqlite_parse_string);
    context.registerType<const char *>(sqlite_bind_string);
    context.registerType<SqlTextView>(sqlite_parse_string_view);
    context.registerType<SqlBlob>(sqlite_parse_blob);
    context.registerType<const std::byte *>(sqlite_bind_blob);
    context.registerType<SqlDate>(sqlite_parse_date);
    context.registerType<SqlDate>(sqlite_bind_date);
}

ILIAS_SQLITE_NS_END
