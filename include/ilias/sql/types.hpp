/**
 * @file types.hpp
 * @brief Backend-agnostic types
 */
#pragma once
#include <string>
#include <vector>
#include <variant>
#include <chrono>
#include <span>

#include "ilias/sql/global/global.hpp"

ILIAS_SQL_NS_BEGIN

// 统一的时间类型
struct ILIAS_SQL_API SqlDate {
    /**
     * @brief 时间类型枚举类
     *
     * 定义了不同时间格式的枚举值，用于表示和区分不同的时间类型
     */
    enum TimeType {
        kErrorTime = 0, // 错误时间 - 表示无效或错误的时间类型
        kDate,          // 2022-01-01
        kDateTime,      // 2022-01-01 00:00:00.000000
        kTime,          // 00:00:00.000000
    };
    SqlDate(int year = 0, int month = 1, int day = 1, int hour = 0, int minute = 0, int second = 0) {
        setTime(year, month, day, hour, minute, second);
    }
    explicit SqlDate(struct tm *timeinfo) { setTime(timeinfo); }
    explicit SqlDate(std::chrono::system_clock::time_point tp) { setTime(tp); }
    explicit SqlDate(std::chrono::milliseconds timestamp) { setTime(timestamp); }
    explicit SqlDate(std::string_view str) { setTime(str); }
    explicit SqlDate(const std::string str) { setTime(str); }

    auto setTime(std::chrono::system_clock::time_point tp) -> void;
    auto setTime(std::chrono::milliseconds timestamp) -> void;
    auto setTime(int year, int month, int day, int hour, int minute, int second, int microsecond = 0) -> void;
    auto setDate(int year, int month, int day) -> void;
    auto setTime(int hour, int minute, int second) -> void;
    auto setTime(struct tm *timeinfo) -> void;
    auto setTime(std::string_view str) -> void;
    auto setTimeType(TimeType type) -> void;

    auto toString() const -> std::string;
    auto toTimestamp() const -> uint64_t;
    void clear();

    uint32_t year        = 0;
    uint32_t month       = 0;
    uint32_t day         = 0;
    uint32_t hour        = 0;
    uint32_t minute      = 0;
    uint32_t second      = 0;
    uint32_t microsecond = 0;
    TimeType type        = kErrorTime;
};

// 空值类型
struct SqlNull {};
// 全局静态 SqlNull 实例，用于返回指向 NULL 的指针（避免返回 nullptr）
inline SqlNull g_sql_null {};

// 二进制视图
using SqlBlobView = std::span<const std::byte>;
using SqlBlobRef  = std::span<std::byte>;
// 二进制拥有权对象
using SqlBlob = std::vector<std::byte>;

// 数据库值的通用变体
using SqlValue = std::variant<SqlNull,     // NULL
                              bool,        // Bool
                              char,        // TinyInt
                              int32_t,     // Int
                              int64_t,     // BigInt
                              float,       // Float
                              double,      // Double
                              std::string, // Text
                              SqlBlob,     // Binary
                              SqlDate      // Timestamp/Date
                              >;
// 类型索引对应的枚举
enum class SqlValueType { kNull = 0, kBool, kChar, kInt, kBigInt, kFloat, kDouble, kText, kBlob, kDate, kMax };

// 用于参数绑定的轻量级变体 (避免拷贝 string/blob)
using SqlValueView =
    std::variant<SqlNull, bool, char, int32_t, int64_t, float, double, std::string_view, SqlBlobView, SqlDate>;

using SqlValuePointer = std::variant<SqlNull *, bool *, char *, int32_t *, int64_t *, float *, double *,
                                     std::string_view, SqlBlobView, SqlDate *>;
using SqlValueRef =
    std::variant<SqlNull, bool &, char &, int32_t &, int64_t &, float &, double &, std::string &, SqlBlob &, SqlDate &>;

// 明确处理 nullptr_t，确保绑定 nullptr 时返回指向 g_sql_null 的指针
inline auto to_sql_pointer(std::nullptr_t &) -> SqlValuePointer {
    return SqlValuePointer {&g_sql_null};
}
inline auto to_sql_pointer(const std::nullptr_t &) -> SqlValuePointer {
    return SqlValuePointer {&g_sql_null};
}
inline auto to_sql_pointer(const std::nullopt_t &) -> SqlValuePointer {
    return SqlValuePointer {&g_sql_null};
}
inline auto to_sql_pointer(std::nullopt_t &) -> SqlValuePointer {
    return SqlValuePointer {&g_sql_null};
}

template <SqlValueType T, class enable = void>
struct SqlValueTraits {
    static_assert(T == SqlValueType::kNull, "Invalid SqlValueType");
};

template <>
struct SqlValueTraits<SqlValueType::kNull, void> {
    using type                              = SqlNull;
    using viewType                          = SqlNull;
    constexpr static SqlValueType valueType = SqlValueType::kNull;
    constexpr static auto         name      = "NULL";
};

template <>
struct SqlValueTraits<SqlValueType::kBool, void> {
    using type                              = bool;
    using viewType                          = bool;
    constexpr static SqlValueType valueType = SqlValueType::kBool;
    constexpr static auto         name      = "Bool";
};

template <>
struct SqlValueTraits<SqlValueType::kChar, void> {
    using type                              = char;
    using viewType                          = char;
    constexpr static SqlValueType valueType = SqlValueType::kChar;
    constexpr static auto         name      = "Char";
};

template <>
struct SqlValueTraits<SqlValueType::kInt, void> {
    using type                              = int32_t;
    using viewType                          = int32_t;
    constexpr static SqlValueType valueType = SqlValueType::kInt;
    constexpr static auto         name      = "Int";
};

template <>
struct SqlValueTraits<SqlValueType::kBigInt, void> {
    using type                              = int64_t;
    using viewType                          = int64_t;
    constexpr static SqlValueType valueType = SqlValueType::kBigInt;
    constexpr static auto         name      = "BigInt";
};

template <>
struct SqlValueTraits<SqlValueType::kFloat, void> {
    using type                              = float;
    using viewType                          = float;
    constexpr static SqlValueType valueType = SqlValueType::kFloat;
    constexpr static auto         name      = "Float";
};

template <>
struct SqlValueTraits<SqlValueType::kDouble, void> {
    using type                              = double;
    using viewType                          = double;
    constexpr static SqlValueType valueType = SqlValueType::kDouble;
    constexpr static auto         name      = "Double";
};

template <>
struct SqlValueTraits<SqlValueType::kText, void> {
    using type                              = std::string;
    using viewType                          = std::string_view;
    constexpr static SqlValueType valueType = SqlValueType::kText;
    constexpr static auto         name      = "Text";
};

template <>
struct SqlValueTraits<SqlValueType::kBlob, void> {
    using type                              = SqlBlob;
    using viewType                          = SqlBlobView;
    constexpr static SqlValueType valueType = SqlValueType::kBlob;
    constexpr static auto         name      = "Blob";
};

template <>
struct SqlValueTraits<SqlValueType::kDate, void> {
    using type                              = SqlDate;
    using viewType                          = SqlDate;
    constexpr static SqlValueType valueType = SqlValueType::kDate;
    constexpr static auto         name      = "Date";
};

template <typename T>
    requires(std::is_same_v<std::decay_t<T>, SqlValue> || std::is_same_v<std::decay_t<T>, SqlValueView> ||
             std::is_same_v<std::decay_t<T>, SqlValuePointer> || std::is_same_v<std::decay_t<T>, SqlValueRef>)
inline auto getSqltypeName(T &&v) -> std::string_view {
    int type = v.index();
    switch ((SqlValueType)type) {
        case SqlValueType::kNull:
            return SqlValueTraits<SqlValueType::kNull>::name;
        case SqlValueType::kBool:
            return SqlValueTraits<SqlValueType::kBool>::name;
        case SqlValueType::kChar:
            return SqlValueTraits<SqlValueType::kChar>::name;
        case SqlValueType::kInt:
            return SqlValueTraits<SqlValueType::kInt>::name;
        case SqlValueType::kBigInt:
            return SqlValueTraits<SqlValueType::kBigInt>::name;
        case SqlValueType::kFloat:
            return SqlValueTraits<SqlValueType::kFloat>::name;
        case SqlValueType::kDouble:
            return SqlValueTraits<SqlValueType::kDouble>::name;
        case SqlValueType::kText:
            return SqlValueTraits<SqlValueType::kText>::name;
        case SqlValueType::kBlob:
            return SqlValueTraits<SqlValueType::kBlob>::name;
        case SqlValueType::kDate:
            return SqlValueTraits<SqlValueType::kDate>::name;
        default:
            return "Unknown";
    }
}

template <SqlValueType T>
auto &get(SqlValue &v) {
    return *std::get_if<typename SqlValueTraits<T>::type>(&v);
}

template <SqlValueType T>
auto &get(SqlValueView &v) {
    return *std::get_if<typename SqlValueTraits<T>::viewType>(&v);
}

template <SqlValueType T>
auto &get(SqlValuePointer &v) {
    if constexpr (T == SqlValueType::kText) {
        return std::get<std::string_view>(v);
    }
    else if constexpr (T == SqlValueType::kBlob) {
        return std::get<SqlBlobView>(v);
    }
    else {
        return *std::get<(int)T>(v);
    }
}

template <typename T>
concept ISqlValue = requires(T t) {
    { t } -> std::convertible_to<SqlValue>;
};

template <typename T>
concept ISqlValueView = requires(T t) {
    { t } -> std::convertible_to<SqlValueView>;
};

template <typename T>
concept ISqlValueCovervable = requires(T t) {
    { t.toSqlValue() } -> std::convertible_to<SqlValue>;
};

template <typename T>
concept ISqlValueViewCovervable = requires(T t) {
    { t.toSqlValueView() } -> std::convertible_to<SqlValueView>;
};

template <typename T>
    requires ISqlValueCovervable<T> || ISqlValue<T>
auto to_sql_value(T &&t) -> SqlValue {
    if constexpr (ISqlValueCovervable<T>) {
        return t.toSqlValue();
    }
    else {
        return std::forward<T>(t);
    }
}

template <typename T>
    requires ISqlValueViewCovervable<T> || ISqlValueView<T>
auto to_sql_value_view(T &&t) -> SqlValueView {
    if constexpr (ISqlValueViewCovervable<T>) {
        return t.toSqlValueView();
    }
    else {
        return std::forward<T>(t);
    }
}

template <typename T>
    requires(ISqlValue<T> && !std::is_integral_v<T>)
auto to_sql_pointer(T &t) -> SqlValuePointer {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, std::string_view> ||
                  std::is_convertible_v<std::decay_t<T>, std::string_view>) {
        if constexpr (std::is_pointer_v<std::decay_t<T>>) {
            if (t == nullptr) {
                return SqlValuePointer {&g_sql_null};
            }
        }
        return SqlValuePointer {std::string_view {t}};
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, SqlBlob> ||
                       std::is_convertible_v<std::decay_t<T>, SqlBlobView>) {
        return SqlValuePointer {SqlBlobView {t}};
    }
    else {
        return std::addressof(t);
    }
}

template <typename T>
    requires(ISqlValue<T> && !std::is_integral_v<T>)
auto to_sql_pointer(const T &t) -> SqlValuePointer {
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, std::string_view> ||
                  std::is_convertible_v<std::decay_t<T>, std::string_view>) {
        if constexpr (std::is_pointer_v<std::decay_t<T>>) {
            if (t == nullptr) {
                return SqlValuePointer {&g_sql_null};
            }
        }
        return SqlValuePointer {std::string_view {t}};
    }
    else if constexpr (std::is_same_v<std::decay_t<T>, SqlBlob> ||
                       std::is_convertible_v<std::decay_t<T>, SqlBlobView>) {
        return SqlValuePointer {SqlBlobView {t}};
    }
    else {
        return const_cast<T *>(std::addressof(t));
    }
}

template <typename T>
    requires(std::is_integral_v<T> && sizeof(T) <= sizeof(int64_t))
auto to_sql_pointer(T &t) -> SqlValuePointer {
    using type = std::decay_t<T>;
    if constexpr (std::is_same_v<type, bool>) {
        return SqlValuePointer {(bool *)std::addressof(t)};
    }
    else if constexpr (sizeof(T) <= sizeof(char) && std::is_signed_v<type>) {
        return SqlValuePointer {(char *)std::addressof(t)};
    }
    else if constexpr ((sizeof(T) <= sizeof(int32_t) && std::is_signed_v<type>) || sizeof(T) < sizeof(int32_t)) {
        return SqlValuePointer {(int32_t *)std::addressof(t)};
    }
    else if constexpr ((sizeof(T) <= sizeof(int64_t) && std::is_signed_v<type>) || sizeof(T) < sizeof(int64_t)) {
        return SqlValuePointer {(int64_t *)std::addressof(t)};
    }
    else {
        static_assert(sizeof(T) <= sizeof(int32_t), "Integral type too large");
    }
}

ILIAS_SQL_NS_END

// clang-format off
ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlDate) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(ILIAS_SQL_COMPLETE_NAMESPACE::SqlDate & date, Context &ctx) const {
        return format_to(ctx.out(), "{}", date.toString());
    }
};

ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlBlob) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(ILIAS_SQL_COMPLETE_NAMESPACE::SqlBlob & blob, Context &ctx) const {
        std::string hex;
        hex.resize(blob.size() * 2);
        for (size_t i = 0; i < blob.size(); ++i) {
            auto c = blob[i];
            hex[i * 2] = "0123456789ABCDEF"[(int)c >> 4];
            hex[i * 2 + 1] = "0123456789ABCDEF"[(int)c & 0xF];
        }
        return format_to(ctx.out(), "{}", hex);
    }
};

ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlBlobView) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(ILIAS_SQL_COMPLETE_NAMESPACE::SqlBlobView & blob, Context &ctx) const {
        std::string hex;
        hex.resize(blob.size() * 2);
        for (size_t i = 0; i < blob.size(); ++i) {
            auto c = blob[i];
            hex[i * 2] = "0123456789ABCDEF"[(int)c >> 4];
            hex[i * 2 + 1] = "0123456789ABCDEF"[(int)c & 0xF];
        }
        return format_to(ctx.out(), "bytes({}):[{}]", blob.size_bytes(), hex);
    }
};

ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlValue) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(ILIAS_SQL_COMPLETE_NAMESPACE::SqlValue & value, Context &ctx) const {
        using SqlValueType = ILIAS_SQL_COMPLETE_NAMESPACE::SqlValueType;
        switch ((SqlValueType)value.index()) {
            case SqlValueType::kNull:
                return format_to(ctx.out(), "{}", "NULL");
            case SqlValueType::kBool:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBool>(value));
            case SqlValueType::kChar:
                return format_to(ctx.out(), "{}", (int)get<SqlValueType::kChar>(value));
            case SqlValueType::kInt:
                return format_to(ctx.out(), "{}", get<SqlValueType::kInt>(value));
            case SqlValueType::kBigInt:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBigInt>(value));
            case SqlValueType::kFloat:
                return format_to(ctx.out(), "{}", get<SqlValueType::kFloat>(value));
            case SqlValueType::kDouble:
                return format_to(ctx.out(), "{}", get<SqlValueType::kDouble>(value));
            case SqlValueType::kText:
                return format_to(ctx.out(), "\"{}\"", get<SqlValueType::kText>(value));
            case SqlValueType::kBlob:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBlob>(value));
            case SqlValueType::kDate:
                return format_to(ctx.out(), "{}", get<SqlValueType::kDate>(value));
            default:
                return format_to(ctx.out(), "{}", "Unknown");
        }
    }
};

ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlValueView) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(ILIAS_SQL_COMPLETE_NAMESPACE::SqlValueView & value, Context &ctx) const {
        using SqlValueType = ILIAS_SQL_COMPLETE_NAMESPACE::SqlValueType;
        switch ((SqlValueType)value.index()) {
            case SqlValueType::kNull:
                return format_to(ctx.out(), "{}", "NULL");
            case SqlValueType::kBool:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBool>(value));
            case SqlValueType::kChar:
                return format_to(ctx.out(), "{}", (int)get<SqlValueType::kChar>(value));
            case SqlValueType::kInt:
                return format_to(ctx.out(), "{}", get<SqlValueType::kInt>(value));
            case SqlValueType::kBigInt:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBigInt>(value));
            case SqlValueType::kFloat:
                return format_to(ctx.out(), "{}", get<SqlValueType::kFloat>(value));
            case SqlValueType::kDouble:
                return format_to(ctx.out(), "{}", get<SqlValueType::kDouble>(value));
            case SqlValueType::kText:
                return format_to(ctx.out(), "\"{}\"", get<SqlValueType::kText>(value));
            case SqlValueType::kBlob:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBlob>(value));
            case SqlValueType::kDate:
                return format_to(ctx.out(), "{}", get<SqlValueType::kDate>(value));
            default:
                return format_to(ctx.out(), "{}", "Unknown");
        }
    }
};

ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlValuePointer) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(ILIAS_SQL_COMPLETE_NAMESPACE::SqlValuePointer & value, Context &ctx) const {
        using SqlValueType = ILIAS_SQL_COMPLETE_NAMESPACE::SqlValueType;
        switch ((SqlValueType)value.index()) {
            case SqlValueType::kNull:
                return format_to(ctx.out(), "{}", "NULL");
            case SqlValueType::kBool:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBool>(value));
            case SqlValueType::kChar:
                return format_to(ctx.out(), "{}", (int)get<SqlValueType::kChar>(value));
            case SqlValueType::kInt:
                return format_to(ctx.out(), "{}", get<SqlValueType::kInt>(value));
            case SqlValueType::kBigInt:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBigInt>(value));
            case SqlValueType::kFloat:
                return format_to(ctx.out(), "{}", get<SqlValueType::kFloat>(value));
            case SqlValueType::kDouble:
                return format_to(ctx.out(), "{}", get<SqlValueType::kDouble>(value));
            case SqlValueType::kText:
                return format_to(ctx.out(), "\"{}\"", get<SqlValueType::kText>(value));
            case SqlValueType::kBlob:
                return format_to(ctx.out(), "{}", get<SqlValueType::kBlob>(value));
            case SqlValueType::kDate:
                return format_to(ctx.out(), "{}", get<SqlValueType::kDate>(value));
            default:
                return format_to(ctx.out(), "{}", "Unknown");
        }
    }
};
ILIAS_FORMATTER_T_RAW(,std::span<const char>) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(std::span<const char> & value, Context &ctx) const {
        return format_to(ctx.out(), "{}", std::string_view{value.data(), value.size()});
    }
};
// clang-format on
