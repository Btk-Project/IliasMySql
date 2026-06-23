#include "ilias/postgres/postgres_context.hpp"
#include "ilias/postgres/postgres_impl.hpp"
#include "ilias/sql/sqlerror.hpp"
#include <charconv>
#include <cstring>
#include <bit> // for std::endian

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

// =========================================================================
// 辅助工具：跨平台、内存安全的网络字节序 -> 本地主机字节序转换
// 解决了大小端问题，以及 reinterpret_cast 导致的内存未对齐和严格别名陷阱
// =========================================================================
template <typename T>
T pg_ntoh(const void *data) {
    static_assert(std::is_integral_v<T>, "Only integral types are supported");
    T value;
    std::memcpy(&value, data, sizeof(T)); // 安全复制，避免未对齐访问

    if constexpr (std::endian::native == std::endian::little) {
        if constexpr (sizeof(T) == 2) {
#if defined(_MSC_VER)
            return _byteswap_ushort(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap16(value);
#else
            return (value << 8) | (value >> 8);
#endif
        }
        else if constexpr (sizeof(T) == 4) {
#if defined(_MSC_VER)
            return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap32(value);
#else
            return ((value << 24) & 0xff000000) | ((value << 8) & 0x00ff0000) | ((value >> 8) & 0x0000ff00) |
                   ((value >> 24) & 0x000000ff);
#endif
        }
        else if constexpr (sizeof(T) == 8) {
#if defined(_MSC_VER)
            return _byteswap_uint64(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap64(value);
#else
            return ((value & 0xff00000000000000ull) >> 56) | ((value & 0x00ff000000000000ull) >> 40) |
                   ((value & 0x0000ff0000000000ull) >> 24) | ((value & 0x000000ff00000000ull) >> 8) |
                   ((value & 0x00000000ff000000ull) << 8) | ((value & 0x0000000000ff0000ull) << 24) |
                   ((value & 0x000000000000ff00ull) << 40) | ((value & 0x00000000000000ffull) << 56);
#endif
        }
    }
    return value;
}

// 【新增】主机字节序 -> 网络字节序转换工具
template <typename T>
T pg_hton(T value) {
    static_assert(std::is_integral_v<T>, "Only integral types are supported");
    if constexpr (std::endian::native == std::endian::little) {
        if constexpr (sizeof(T) == 2) {
#if defined(_MSC_VER)
            return _byteswap_ushort(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap16(value);
#else
            return (value << 8) | (value >> 8);
#endif
        }
        else if constexpr (sizeof(T) == 4) {
#if defined(_MSC_VER)
            return _byteswap_ulong(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap32(value);
#else
            return ((value << 24) & 0xff000000) | ((value << 8) & 0x00ff0000) | ((value >> 8) & 0x0000ff00) |
                   ((value >> 24) & 0x000000ff);
#endif
        }
        else if constexpr (sizeof(T) == 8) {
#if defined(_MSC_VER)
            return _byteswap_uint64(value);
#elif defined(__GNUC__) || defined(__clang__)
            return __builtin_bswap64(value);
#else
            return ((value & 0xff00000000000000ull) >> 56) | ((value & 0x00ff000000000000ull) >> 40) |
                   ((value & 0x0000ff0000000000ull) >> 24) | ((value & 0x000000ff00000000ull) >> 8) |
                   ((value & 0x00000000ff000000ull) << 8) | ((value & 0x0000000000ff0000ull) << 24) |
                   ((value & 0x000000000000ff00ull) << 40) | ((value & 0x00000000000000ffull) << 56);
#endif
        }
    }
    return value; // 在大端机器上无需操作
}

// ============================================================================
// PostgreSQL类型解析器实现
// ============================================================================

/**
 * @brief 从SqlCellView获取PostgresCellMetadata
 */
static auto getPostgresMetadata(const SqlCellView &cell) -> const PostgresCellMetadata * {
    if (cell.raw_type() == std::type_index(typeid(PostgresCellMetadata))) {
        return static_cast<const PostgresCellMetadata *>(cell.raw_value());
    }
    return nullptr;
}

SqlParserResult pq_parse_null(const SqlCellView &cell) {
    if (cell.is_null()) {
        return std::any(g_sql_null);
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析布尔值
 */
SqlParserResult pq_parse_bool(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && meta->format == 0) { // 文本格式
        const char *value_str = static_cast<const char *>(meta->data);
        return std::any(*value_str == 't' ? true : false);
    }
    else if (meta && meta->format == 1) { // 二进制格式
        // bool 只有 1 字节，不存在大小端问题，但最好避免 reinterpret_cast
        uint8_t val;
        std::memcpy(&val, meta->data, 1);
        return std::any(val != 0);
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析整数类型
 */
template <typename T>
    requires std::is_integral_v<T>
SqlParserResult pq_parse_integer(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && meta->format == 0) { // 文本格式
        const char *value_str = static_cast<const char *>(meta->data);
        T           val       = 0;
        auto [p, ec]          = std::from_chars(value_str, value_str + meta->size, val);
        if (ec == std::errc()) {
            return std::any(val);
        }
    }
    else if (meta && meta->format == 1) { // 二进制格式
        switch (meta->size) {
            case 1: {
                int8_t val;
                std::memcpy(&val, meta->data, 1);
                return std::any(T(val));
            }
            case 2:
                return std::any(T(pg_ntoh<int16_t>(meta->data)));
            case 4:
                return std::any(T(pg_ntoh<int32_t>(meta->data)));
            case 8:
                return std::any(T(pg_ntoh<int64_t>(meta->data)));
            default:
                break;
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析浮点类型
 */
template <typename T>
    requires std::is_floating_point_v<T>
SqlParserResult pq_parse_float(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && meta->format == 0) { // 文本格式
        const char *value_str = static_cast<const char *>(meta->data);
        T           val       = 0.0;
        auto [p, ec]          = std::from_chars(value_str, value_str + meta->size, val);
        if (ec == std::errc()) {
            return std::any(val);
        }
    }
    else if (meta && meta->format == 1) { // 二进制格式
        // 浮点数一样适用 IEEE754 的字节序，需要转换整数后 bit_cast / memcpy 过去
        switch (meta->size) {
            case 4: {
                uint32_t net_val = pg_ntoh<uint32_t>(meta->data);
                float    f_val;
                std::memcpy(&f_val, &net_val, 4);
                return std::any(T(f_val));
            }
            case 8: {
                uint64_t net_val = pg_ntoh<uint64_t>(meta->data);
                double   d_val;
                std::memcpy(&d_val, &net_val, 8);
                return std::any(T(d_val));
            }
            default:
                break;
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析字符串
 */
SqlParserResult pq_parse_string(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && (meta->format == 0 || meta->format == 1)) {
        // 无论是文本还是二进制，PostgreSQL 发送的 VARCHAR/TEXT 数据是一样的
        const char *value_str = static_cast<const char *>(meta->data);
        return std::any(SqlText(value_str, meta->size));
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析字符串视图
 */
SqlParserResult pq_parse_string_view(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && (meta->format == 0 || meta->format == 1)) {
        const char *value_str = static_cast<const char *>(meta->data);
        return std::any(SqlTextView(value_str, meta->size));
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析BLOB
 */
SqlParserResult pq_parse_blob(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && meta->format == 0) {
        // 文本格式（bytea 的 hex 格式，以 \x 开头）
        // 文本格式 (bytea 的 hex 格式，以 \x 开头)
        const char *hex_str = static_cast<const char *>(meta->data);
        size_t      hex_len = meta->size;

        if (hex_len < 2 || hex_str[0] != '\\' || hex_str[1] != 'x') {
            return Err(SqlError::Code::UnsupportConvertFromSqlType);
        }
        hex_str += 2; // 跳过 "\x"
        hex_len -= 2;

        if (hex_len % 2 != 0) {
            return Err(SqlError::Code::UnsupportConvertFromSqlType); // 十六进制字符串必须是偶数长度
        }

        SqlBlob blob;
        blob.resize(hex_len / 2);
        for (size_t i = 0; i < blob.size(); ++i) {
            // 简单的十六进制字符转整数
            auto hex_char_to_int = [](char c) -> int {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                return -1;
            };
            int high = hex_char_to_int(hex_str[i * 2]);
            int low  = hex_char_to_int(hex_str[i * 2 + 1]);
            if (high == -1 || low == -1) {
                return Err(SqlError::Code::UnsupportConvertFromSqlType);
            }
            blob[i] = static_cast<std::byte>((high << 4) | low);
        }
        return std::any(blob);
    }
    else if (meta && meta->format == 1) {
        // 【修正点】二进制格式下，返回的就是纯原生数据字节流，不再有 \x 的 hex 编码
        const std::byte *raw_bytes = static_cast<const std::byte *>(meta->data);
        return std::any(SqlBlobView(raw_bytes, meta->size));
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 解析日期时间类型
 */
SqlParserResult pq_parse_date(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Err(SqlError::Code::NullValue);
    }
    auto meta = getPostgresMetadata(cell);
    if (meta && meta->format == 0) { // 文本格式
        const char *value_str = static_cast<const char *>(meta->data);
        SqlDate     date(std::string_view(value_str, meta->size));
        return std::any(date);
    }
    else if (meta && meta->format == 1) { // 二进制格式
        // 【修正点】PG 的 Date 是自 2000-01-01 以来的天数(4字节) 或 微秒数(8字节)
        // Unix纪元 (1970-01-01) 与 PG纪元 (2000-01-01) 相差 10957 天

        if (meta->size == 4) {
            // DATE 类型 (int32_t，自 2000 年起的天数)
            int32_t pg_days   = pg_ntoh<int32_t>(meta->data);
            int64_t unix_days = static_cast<int64_t>(pg_days) + 10957;
            int64_t unix_ms   = unix_days * 86400LL * 1000LL; // 转为 Unix 毫秒

            return std::any(SqlDate(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(
                std::chrono::milliseconds(unix_ms))));
        }
        else if (meta->size == 8) {
            // TIMESTAMP 类型 (int64_t，自 2000 年起的微秒数)
            int64_t pg_micros = pg_ntoh<int64_t>(meta->data);
            // 补偿 10957 天的微秒差值
            int64_t unix_micros = pg_micros + (10957LL * 86400LL * 1000000LL);
            int64_t unix_ms     = unix_micros / 1000LL;

            return std::any(SqlDate(std::chrono::time_point<std::chrono::system_clock, std::chrono::milliseconds>(
                std::chrono::milliseconds(unix_ms))));
        }
    }
    return Err(SqlError::Code::UnsupportConvertFromSqlType);
}
// ============================================================================
// PostgreSQL类型绑定器实现
// ============================================================================

/**
 * @brief 绑定NULL值
 */
SqlBinderResult pq_bind_null(const SqlCellView &cell, const std::any &data) {
    // 对于NULL值，不需要存储数据
    auto  index = cell.index();
    auto *stmt  = std::any_cast<class PostgresStatement *>(data);
    stmt->setBindParam(index, nullptr, 0, 1, stmt->getTypeOid("null"));

    return make_null_sql_binder_result();
}

/**
 * @brief 绑定布尔值
 */
SqlBinderResult pq_bind_bool(const SqlCellView &cell, const std::any &data) {
    if (cell.is_null()) {
        return pq_bind_null(cell, data);
    }
    auto index = cell.index();
    auto stmt  = std::any_cast<class PostgresStatement *>(data);

    // 直接存储指针
    stmt->setBindParam(index, cell.raw_value(), cell.raw_value_size(), 1, stmt->getTypeOid("bool"));

    return make_null_sql_binder_result();
}

/**
 * @brief 绑定整数类型
 */
template <typename T>
    requires std::is_integral_v<T>
SqlBinderResult pq_bind_integer(const SqlCellView &cell, const std::any &data) {
    if (cell.is_null()) {
        return pq_bind_null(cell, data);
    }
    auto stmt = std::any_cast<class PostgresStatement *>(data);

    T host_val = *static_cast<const T *>(cell.raw_value());

    std::string type_name = "int4";
    switch (sizeof(T)) {
        case 2:
            type_name = "int2";
            break;
        case 4:
            type_name = "int4";
            break;
        case 8:
            type_name = "int8";
            break;
        default:
            break;
    }

    // 转换为网络字节序
    auto net_val = make_sql_binder_result_for_store(pg_hton(host_val));
    stmt->setBindParam(cell.index(), net_val.get(), sizeof(T), 1, stmt->getTypeOid(type_name));

    return net_val;
}

/**
 * @brief 绑定浮点类型
 */
template <typename T>
    requires std::is_floating_point_v<T>
SqlBinderResult pq_bind_float(const SqlCellView &cell, const std::any &data) {
    if (cell.is_null()) {
        return pq_bind_null(cell, data);
    }
    auto stmt     = std::any_cast<PostgresStatement *>(data);
    T    host_val = *static_cast<const T *>(cell.raw_value());

    // 浮点数也要转换字节序
    if constexpr (sizeof(T) == 4) {
        uint32_t i_val;
        std::memcpy(&i_val, &host_val, sizeof(i_val));
        auto stored_val = make_sql_binder_result_for_store(pg_hton(i_val));
        stmt->setBindParam(cell.index(), stored_val.get(), sizeof(uint32_t), 1, stmt->getTypeOid("float4"));
        return stored_val;
    }
    else if constexpr (sizeof(T) == 8) {
        uint64_t i_val;
        std::memcpy(&i_val, &host_val, sizeof(i_val));
        auto stored_val = make_sql_binder_result_for_store(pg_hton(i_val));
        stmt->setBindParam(cell.index(), stored_val.get(), sizeof(uint64_t), 1, stmt->getTypeOid("float8"));
        return stored_val;
    }
}

/**
 * @brief 绑定字符串
 */
SqlBinderResult pq_bind_string(const SqlCellView &cell, const std::any &data) {
    if (cell.is_null()) {
        return pq_bind_null(cell, data);
    }
    auto index = cell.index();
    auto stmt  = std::any_cast<class PostgresStatement *>(data);

    // 直接存储指针
    stmt->setBindParam(index, cell.raw_value(), cell.raw_value_size(), 1, stmt->getTypeOid("text"));

    return make_null_sql_binder_result();
}

/**
 * @brief 绑定BLOB
 */
SqlBinderResult pq_bind_blob(const SqlCellView &cell, const std::any &data) {
    if (cell.is_null()) {
        return pq_bind_null(cell, data);
    }
    auto index = cell.index();
    auto stmt  = std::any_cast<class PostgresStatement *>(data);

    // 直接存储指针
    stmt->setBindParam(index, cell.raw_value(), cell.raw_value_size(), 1, stmt->getTypeOid("varchar"));

    return make_null_sql_binder_result();
}

/**
 * @brief 绑定日期时间类型
 */
SqlBinderResult pq_bind_date(const SqlCellView &cell, const std::any &data) {
    if (cell.is_null()) {
        return pq_bind_null(cell, data);
    }
    auto        stmt = std::any_cast<PostgresStatement *>(data);
    const auto &date = *static_cast<const SqlDate *>(cell.raw_value());

    // PG纪元 (2000-01-01) 距离 Unix纪元 (1970-01-01) 的微秒差值
    constexpr int64_t pg_epoch_micros_offset = 10957LL * 86400LL * 1000000LL;

    auto time_point = date.to_time_point();
    auto micros_since_unix_epoch =
        std::chrono::duration_cast<std::chrono::microseconds>(time_point.time_since_epoch()).count();

    int64_t pg_micros = micros_since_unix_epoch - pg_epoch_micros_offset;

    // 转换字节序并分配到堆上
    auto stored_val = make_sql_binder_result_for_store(pg_hton(pg_micros));
    stmt->setBindParam(cell.index(), stored_val.get(), sizeof(int64_t), 1, stmt->getTypeOid("TIMESTAMP"));
    return stored_val;
}

// ============================================================================
// 注册函数
// ============================================================================

/**
 * @brief 注册所有PostgreSQL类型解析器和绑定器
 *
 * @param context PostgreSQL值转换器上下文
 */
void registerPostgresTypeParsers(PostgresValueConverterContext &context) {
    // 注册NULL类型解析器
    context.registerType<SqlNull>(&pq_parse_null);

    // 注册NULL类型绑定器
    context.registerType<SqlNull>(&pq_bind_null);

    // 注册布尔类型解析器
    context.registerType<bool>(&pq_parse_bool);

    // 注册布尔类型绑定器
    context.registerType<bool>(&pq_bind_bool);

    // 注册整数类型解析器
    context.registerType<int8_t>(&pq_parse_integer<int8_t>);
    context.registerType<int16_t>(&pq_parse_integer<int16_t>);
    context.registerType<int32_t>(&pq_parse_integer<int32_t>);
    context.registerType<int64_t>(&pq_parse_integer<int64_t>);

    // 注册整数类型绑定器
    context.registerType<int8_t>(&pq_bind_integer<int8_t>);
    context.registerType<int16_t>(&pq_bind_integer<int16_t>);
    context.registerType<int32_t>(&pq_bind_integer<int32_t>);
    context.registerType<int64_t>(&pq_bind_integer<int64_t>);

    // 注册浮点类型解析器
    context.registerType<float>(&pq_parse_float<float>);
    context.registerType<double>(&pq_parse_float<double>);

    // 注册浮点类型绑定器
    context.registerType<float>(&pq_bind_float<float>);
    context.registerType<double>(&pq_bind_float<double>);

    // 注册字符串类型解析器
    context.registerType<SqlText>(&pq_parse_string);
    context.registerType<SqlTextView>(&pq_parse_string_view);

    // 注册字符串类型绑定器
    context.registerType<const char *>(&pq_bind_string);
    context.registerType<SqlText>(&pq_bind_string);
    context.registerType<SqlTextView>(&pq_bind_string);

    // 注册BLOB类型解析器
    context.registerType<SqlBlob>(&pq_parse_blob);
    context.registerType<SqlBlobView>(&pq_parse_blob);

    // 注册BLOB类型绑定器
    context.registerType<SqlBlob>(&pq_bind_blob);
    context.registerType<SqlBlobView>(&pq_bind_blob);

    // 注册日期时间类型解析器
    context.registerType<SqlDate>(&pq_parse_date);

    // 注册日期时间类型绑定器
    context.registerType<SqlDate>(&pq_bind_date);
}

ILIAS_POSTGRES_NS_END
