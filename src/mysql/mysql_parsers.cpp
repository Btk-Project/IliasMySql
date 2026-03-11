#include "ilias/mysql/mysql_parsers.hpp"

#include "ilias/mysql/mysql.hpp"

ILIAS_MYSQL_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

// ----------------------- parser and binder -----------------------
SqlParserResult mysql_parse_null(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return std::any(g_sql_null);
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlBinderResult mysql_bind_null(const SqlCellView &cell, std::any data) {
    auto index = cell.index();
    // 无视cell内部的值
    auto mysqlstmt      = std::any_cast<MysqlStatement *>(data);
    auto bind           = mysqlstmt->dataBind(index);
    bind->buffer_type   = MYSQL_TYPE_NULL;
    bind->is_null_value = true;
    bind->buffer_length = 0;
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_integral_v<T>
SqlParserResult mysql_parse_interage(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    // mysql返回的可能是字符串，也可能是数据的指针
    auto string = cell.formatted_value();
    switch (cell.formatted_type()) {
        case MYSQL_TYPE_TINY: {
            int res;
            auto [ptr, ec] = std::from_chars(string.data(), string.data() + string.size(), res);
            if (ec != std::errc() || ptr != (string.data() + string.size())) {
                // 可能是数字 或者 true/false
                if (string == "true" || string == "false") {
                    return std::any(static_cast<T>(string == "true"));
                } // if is a boolean
                return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
            }
            return std::any(static_cast<T>(res));
        }
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_LONGLONG:
        case MYSQL_TYPE_NEWDECIMAL: {
            int64_t res;
            auto [ptr, ec] = std::from_chars(string.data(), string.data() + string.size(), res);
            if (ec != std::errc() || ptr != (string.data() + string.size())) {
                return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
            }
            return std::any(static_cast<T>(res));
        }
    }

    if (cell.raw_value() == nullptr) {
        return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
    }
    if (cell.raw_type() == std::type_index(typeid(int32_t))) {
        int32_t value = *reinterpret_cast<const int32_t *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    else if (cell.raw_type() == std::type_index(typeid(int64_t))) {
        int64_t value = *reinterpret_cast<const int64_t *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
}

template <typename T>
    requires std::is_integral_v<T>
SqlBinderResult mysql_bind_interage(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind = mysqlstmt->dataBind(index);
    switch (sizeof(T)) {
        case sizeof(char):
            bind->buffer_type = MYSQL_TYPE_TINY;
            break;
        case sizeof(int32_t):
            bind->buffer_type = MYSQL_TYPE_LONG;
            break;
        case sizeof(int64_t):
            bind->buffer_type = MYSQL_TYPE_LONGLONG;
            break;
        default:
            return Unexpected(SqlError::Code::UnsupportBindType);
    }
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_floating_point_v<T>
SqlParserResult mysql_parse_real(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    // mysql返回的可能是字符串，也可能是数据的指针
    auto string = cell.formatted_value();
    switch (cell.formatted_type()) {
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_SHORT:    // short
        case MYSQL_TYPE_LONG:     // int
        case MYSQL_TYPE_LONGLONG: // long long
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL: {
            T res;
            auto [ptr, ec] = std::from_chars(string.data(), string.data() + string.size(), res);
            if (ec != std::errc() || ptr != (string.data() + string.size())) {
                return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
            }
            return std::any(static_cast<T>(res));
        }
    }

    if (cell.raw_value() == nullptr) {
        return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
    }
    if (cell.raw_type() == std::type_index(typeid(float))) {
        float value = *reinterpret_cast<const float *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    else if (cell.raw_type() == std::type_index(typeid(double))) {
        double value = *reinterpret_cast<const double *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
}

template <typename T>
    requires std::is_floating_point_v<T>
SqlBinderResult mysql_bind_real(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind = mysqlstmt->dataBind(index);
    switch (sizeof(T)) {
        case sizeof(float):
            bind->buffer_type = MYSQL_TYPE_FLOAT;
            break;
        case sizeof(double):
            bind->buffer_type = MYSQL_TYPE_DOUBLE;
            break;
        default:
            return Unexpected(SqlError::Code::UnsupportBindType);
    }
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

SqlParserResult mysql_parse_string(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        return std::any(SqlText(cell.formatted_value()));
    }
    if (cell.format() == SqlCellView::DataFormat::kValuePointer) {
        if (cell.raw_type() == std::type_index(typeid(float))) {
            float value = *reinterpret_cast<const float *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
        else if (cell.raw_type() == std::type_index(typeid(double))) {
            double value = *reinterpret_cast<const double *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
        else if (cell.raw_type() == std::type_index(typeid(int32_t))) {
            int32_t value = *reinterpret_cast<const int32_t *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
        else if (cell.raw_type() == std::type_index(typeid(int64_t))) {
            int64_t value = *reinterpret_cast<const int64_t *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlParserResult mysql_parse_string_view(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        return std::any(SqlTextView(cell.formatted_value()));
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlBinderResult mysql_bind_string(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const char *))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind           = mysqlstmt->dataBind(index);
    bind->buffer_type   = MYSQL_TYPE_STRING;
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

SqlParserResult mysql_parse_blob_view(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        auto value = cell.formatted_value();
        return std::any(std::span<const std::byte>(reinterpret_cast<const std::byte *>(value.data()), value.size()));
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlParserResult mysql_parse_blob(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        auto value = cell.formatted_value();
        return std::any(std::vector<std::byte>(reinterpret_cast<const std::byte *>(value.data()),
                                               reinterpret_cast<const std::byte *>(value.data()) + value.size()));
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlBinderResult mysql_bind_blob(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const std::byte *))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind           = mysqlstmt->dataBind(index);
    bind->buffer_type   = MYSQL_TYPE_BLOB;
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

SqlParserResult mysql_parse_date(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        auto type  = cell.formatted_type();
        auto value = cell.formatted_value();
        switch (type) {
            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_TIME:
            case MYSQL_TYPE_DATE: {
                sql::SqlDate date;
                date.fromUTCString(value);
                return std::any(date);
            }
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

MYSQL_TIME toMysqlTime(const SqlDate &dt) {
    MYSQL_TIME time;
    memset(&time, 0, sizeof(time));
    switch (dt.type) {
        case SqlDate::kDate:
            time.year      = dt.year;
            time.month     = dt.month;
            time.day       = dt.day;
            time.time_type = MYSQL_TIMESTAMP_DATE;
            break;
        case SqlDate::kDateTime:
            time.year        = dt.year;
            time.month       = dt.month;
            time.day         = dt.day;
            time.hour        = dt.hour;
            time.minute      = dt.minute;
            time.second      = dt.second;
            time.second_part = dt.microsecond * 1000;
            time.time_type   = MYSQL_TIMESTAMP_DATETIME;
            break;
        case SqlDate::kTime:
            time.hour        = dt.hour;
            time.minute      = dt.minute;
            time.second      = dt.second;
            time.second_part = dt.microsecond * 1000;
            time.time_type   = MYSQL_TIMESTAMP_TIME;
            break;
        default:
            time.time_type = MYSQL_TIMESTAMP_NONE;
    }
    return time;
}

SqlBinderResult mysql_bind_date(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const SqlDate))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    std::unique_ptr<void, void (*)(void *)> ptr {malloc(sizeof(MYSQL_TIME)), free};

    auto mtime = reinterpret_cast<MYSQL_TIME *>(ptr.get());
    *mtime     = toMysqlTime(*reinterpret_cast<const SqlDate *>(cell.raw_value()));
    auto bind  = mysqlstmt->dataBind(index);
    switch (mtime->time_type) {
        case MYSQL_TIMESTAMP_DATE:
            bind->buffer_type = MYSQL_TYPE_DATE;
            break;
        case MYSQL_TIMESTAMP_DATETIME:
            bind->buffer_type = MYSQL_TYPE_DATETIME;
            break;
        case MYSQL_TIMESTAMP_TIME:
            bind->buffer_type = MYSQL_TYPE_TIME;
            break;
        default:
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    bind->buffer        = const_cast<void *>(ptr.get());
    bind->buffer_length = sizeof(MYSQL_TIME);
    return ptr;
}

void registerMysqlTypeParsers(SqlValueConverterContext &context) {
    context.registerType<SqlNull>(mysql_parse_null);
    context.registerType<SqlNull>(mysql_bind_null);
    context.registerType<SqlBool>(mysql_parse_interage<SqlBool>);
    context.registerType<SqlBool>(mysql_bind_interage<SqlBool>);
    context.registerType<SqlTinyInt>(mysql_parse_interage<SqlTinyInt>);
    context.registerType<char>(mysql_parse_interage<char>);
    context.registerType<SqlTinyInt>(mysql_bind_interage<SqlTinyInt>);
    context.registerType<char>(mysql_bind_interage<char>);
    context.registerType<SqlInt>(mysql_parse_interage<SqlInt>);
    context.registerType<SqlInt>(mysql_bind_interage<SqlInt>);
    context.registerType<SqlBigInt>(mysql_parse_interage<SqlBigInt>);
    context.registerType<SqlBigInt>(mysql_bind_interage<SqlBigInt>);
    context.registerType<SqlFloat>(mysql_parse_real<SqlFloat>);
    context.registerType<SqlFloat>(mysql_bind_real<SqlFloat>);
    context.registerType<double>(mysql_parse_real<double>);
    context.registerType<double>(mysql_bind_real<double>);
    context.registerType<SqlText>(mysql_parse_string);
    context.registerType<const char *>(mysql_bind_string);
    context.registerType<SqlTextView>(mysql_parse_string_view);
    context.registerType<SqlBlob>(mysql_parse_blob);
    context.registerType<SqlBlobView>(mysql_parse_blob_view);
    context.registerType<const std::byte *>(mysql_bind_blob);
    context.registerType<SqlDate>(mysql_parse_date);
    context.registerType<SqlDate>(mysql_bind_date);
}

ILIAS_MYSQL_NS_END