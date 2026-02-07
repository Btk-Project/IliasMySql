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
#include <typeindex>
#include <any>
#include <map>

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/sqlerror.hpp"
#include "nekoproto/global/reflect.hpp"

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
    SqlDate() {}
    SqlDate(int year, int month, int day, int hour = 0, int minute = 0, int second = 0,
            int time_zone_offset_minutes = 0) {
        setTime(year, month, day, hour, minute, second, 0, time_zone_offset_minutes);
    }
    explicit SqlDate(struct tm *timeinfo) { setTime(timeinfo); }
    explicit SqlDate(std::chrono::system_clock::time_point tp) { setTime(tp); }
    explicit SqlDate(std::chrono::milliseconds timestamp) { setTime(timestamp); }
    explicit SqlDate(std::string_view str) { fromUTCString(str); }
    explicit SqlDate(const std::string str) { fromUTCString(str); }
    SqlDate(const SqlDate &)            = default;
    SqlDate(SqlDate &&)                 = default;
    SqlDate &operator=(const SqlDate &) = default;
    SqlDate &operator=(SqlDate &&)      = default;
    ~SqlDate()                          = default;

    explicit operator time_t() const { return toTimestamp(); }
    explicit operator struct tm() const {
        struct tm timeinfo = {};
        timeinfo.tm_year   = year - 1900;
        timeinfo.tm_mon    = month - 1;
        timeinfo.tm_mday   = day;
        timeinfo.tm_hour   = hour;
        timeinfo.tm_min    = minute;
        timeinfo.tm_sec    = second;
        return timeinfo;
    }
    explicit operator std::string() const { return toUTCString(); }
    explicit operator bool() const { return type != kErrorTime; }

    auto setTime(std::chrono::system_clock::time_point tp) -> void;
    auto setTime(std::chrono::milliseconds timestamp) -> void;
    auto setTime(int year, int month, int day, int hour, int minute, int second, int microsecond = 0,
                 int time_zone_offset_minutes = 0) -> void;
    auto setDate(int year, int month, int day) -> void;
    auto setTime(int hour, int minute, int second) -> void;
    auto setTime(struct tm *timeinfo) -> void;
    auto setTimeType(TimeType type) -> void;

    auto        fromUTCString(std::string_view str) -> void;
    auto        fromLocalString(std::string_view str) -> void;
    auto        toUTCString() const -> std::string;
    auto        toLocalString() const -> std::string;
    auto        toTimestamp() const -> uint64_t;
    void        clear();
    static auto now() -> SqlDate;

    uint32_t year        = 0;
    uint32_t month       = 0;
    uint32_t day         = 0;
    uint32_t hour        = 0;
    uint32_t minute      = 0;
    uint32_t second      = 0;
    uint32_t microsecond = 0;
    TimeType type        = kErrorTime;
};

// 通用默认支持类型
struct SqlNull {};
using SqlBool     = bool;
using SqlTinyInt  = int8_t;
using SqlInt      = int32_t;
using SqlBigInt   = int64_t;
using SqlFloat    = float;
using SqlDouble   = double;
using SqlText     = std::string;
using SqlTextView = std::string_view;
using SqlBlobView = std::span<const std::byte>;
using SqlBlobRef  = std::span<std::byte>;
using SqlBlob     = std::vector<std::byte>;

// 全局静态 SqlNull 实例，用于返回指向 NULL 的指针（避免返回 nullptr）
inline SqlNull g_sql_null {};

// 数据库值的通用变体
using SqlValue = std::variant<SqlNull,    // NULL
                              SqlBool,    // Bool
                              SqlTinyInt, // TinyInt
                              SqlInt,     // Int
                              SqlBigInt,  // BigInt
                              SqlFloat,   // Float
                              SqlDouble,  // Double
                              SqlText,    // Text
                              SqlBlob,    // Binary
                              SqlDate     // Timestamp/Date
                              >;
// 用于参数绑定的轻量级变体 (避免拷贝 string/blob)
using SqlValueView = std::variant<SqlNull, SqlBool, SqlTinyInt, SqlInt, SqlBigInt, SqlFloat, SqlDouble, SqlTextView,
                                  SqlBlobView, SqlDate>;

using SqlValuePointer = std::variant<SqlNull *, SqlBool *, SqlTinyInt *, SqlInt *, SqlBigInt *, SqlFloat *, SqlDouble *,
                                     SqlTextView, SqlBlobView, SqlDate *>;

using SqlValueRef = std::variant<SqlNull, SqlBool &, SqlTinyInt &, SqlInt &, SqlBigInt &, SqlFloat &, SqlDouble &,
                                 SqlText &, SqlBlob &, SqlDate &>;

// 类型索引对应的枚举
enum class SqlValueType { kNull = 0, kBool, kChar, kInt, kBigInt, kFloat, kDouble, kText, kBlob, kDate, kMax };

class SqlCellView;
using SqlParserFunc = std::function<IoResult<std::any>(const SqlCellView &)>;

class SqlValueConverterContext {
public:
    virtual ~SqlValueConverterContext() = default;
    virtual auto findTypeParser(std::type_index type) const -> SqlParserFunc {
        auto item = mParsers.find(type);
        if (item != mParsers.end()) {
            return item->second;
        }
        return nullptr;
    }
    virtual auto parserTypes() const -> const std::map<std::type_index, SqlParserFunc> & { return mParsers; }

    virtual auto registerType(std::type_index type, SqlParserFunc func) -> void { mParsers[type] = func; }

    template <typename T>
    auto registerType(SqlParserFunc func) -> void {
        registerType(std::type_index(typeid(T)), func);
    }

protected:
    std::map<std::type_index, SqlParserFunc> mParsers;
};

/**
 * @brief 数据库单元格视图
 * 该数据视图在Result::next()和Result释放数据前有效
 */
class SqlCellView {
public:
    using ValueString  = std::tuple<std::string_view, int>;
    using NativeValue  = std::any;
    using ValuePointer = std::tuple<const void *, std::type_index>;
    enum class DataFormat { kUnknown = 0, kString, kNativeValue, kValuePointer };

    SqlCellView()                               = default;
    SqlCellView(const SqlCellView &)            = default;
    SqlCellView(SqlCellView &&)                 = default;
    SqlCellView &operator=(const SqlCellView &) = default;
    SqlCellView &operator=(SqlCellView &&)      = default;
    ~SqlCellView()                              = default;
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt, std::string_view data, uint32_t native_type, int index)
        : mContext(ctxt), mSqlValue(ValueString(data, native_type)), mIndex(index) {}
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt, NativeValue data, int index)
        : mContext(ctxt), mSqlValue(data), mIndex(index) {}
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt, const void *data, std::type_index type, int index)
        : mContext(ctxt), mSqlValue(ValuePointer(data, type)), mIndex(index) {}
    /**
     * @brief 将数据转换为指定类型
     * 通过查找构造时提供的上下文中注册的解析器函数，将单元格数据解析为指定类型，如果解析失败，则返回错误
     * @tparam T 期望的类型
     * @return IoResult<T>
     */
    template <typename T>
    auto as() const -> IoResult<T>;
    /**
     * @brief SqlCellView中的数据格式
     * @par DataFormat::kString
     * 可读的字符串数据
     * @par DataFormat::kNativeValue
     * 指向底层数据库自行封装的value对象指针，如 MySQL::MYSQL_BIND*， sqlite3_value* 等,
     * @par DataFormat::kValuePointer
     *  指向原生类型的指针，如 int*、double* 等，具体类型请通过native_type()获取数据库后端的类型枚举并确认。
     * @return DataFormat
     */
    auto format() const -> DataFormat {
        switch (mSqlValue.index()) {
            case 0:
                return DataFormat::kUnknown;
            case 1:
                return DataFormat::kString;
            case 2:
                return DataFormat::kNativeValue;
            case 3:
                return DataFormat::kValuePointer;
        }
        return DataFormat::kUnknown;
    }
    /**
     * @brief 该单元格数据是否为空
     *
     * @return true
     * @return false
     */
    auto is_null() const -> bool { return mSqlValue.index() == 0; }
    /**
     * @brief 获取单元格在列中的索引
     *
     * @return int
     */
    auto index() const -> int { return mIndex; }
    /**
     * @brief 获取来自后端实现的通用类型指针
     *
     * @return const std::any&
     */
    auto sql_value() const -> const std::any & { return std::get<NativeValue>(mSqlValue); }
    /**
     * @brief 获取原始数据指针
     *
     * @return const void*
     */
    auto raw_value() const -> const void * { return std::get<0>(std::get<ValuePointer>(mSqlValue)); }
    /**
     * @brief 获取原始数据类型
     *
     * @return std::type_index
     */
    auto raw_type() const -> std::type_index { return std::get<1>(std::get<ValuePointer>(mSqlValue)); }
    /**
     * @brief 获取格式化后的数据
     *
     * @return std::string
     */
    auto formatted_value() const -> std::string_view { return std::get<0>(std::get<ValueString>(mSqlValue)); }
    /**
     * @brief   获取格式化后的数据类型
     *
     * @return  int
     */
    auto formatted_type() const -> int { return std::get<1>(std::get<ValueString>(mSqlValue)); }

private:
    std::shared_ptr<SqlValueConverterContext>                            mContext;
    std::variant<std::monostate, ValueString, NativeValue, ValuePointer> mSqlValue;
    int                                                                  mIndex = 0;
};

template <typename T>
auto SqlCellView::as() const -> IoResult<T> {
    constexpr bool is_optional = NEKO_NAMESPACE::detail::is_optional<T>::value;
    constexpr bool is_pointer  = std::is_pointer_v<T>;
    constexpr bool is_nullable = is_optional || is_pointer || std::is_same_v<T, SqlDate>;
    auto           type_index  = std::type_index(typeid(std::remove_cvref_t<T>));
    if constexpr (is_optional) {
        type_index = std::type_index(typeid(typename T::value_type));
    }
    auto parser_func = mContext->findTypeParser(type_index);
    if (!parser_func) {
        ILIAS_ERROR("ilias-sql", "Unsupport convert from sql type: {}", type_index.name());
        return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
    }
    if constexpr (is_nullable) {
        if (is_null()) {
            return T {};
        }
    }
    IoResult<std::any> result_any = parser_func(*this);
    if constexpr (is_nullable) {
        if (!result_any && result_any.error() == SqlError::NullValue) {
            return T {};
        }
    }
    if (!result_any) {
        return Unexpected(result_any.error());
    }
    if constexpr (is_optional) {
        using value_type = typename T::value_type;
        if (const value_type *ptr = std::any_cast<value_type>(&result_any.value())) {
            return T {std::move(*ptr)};
        }
    }
    else {
        if (const T *ptr = std::any_cast<T>(&result_any.value())) {
            return *ptr;
        }
    }
    ILIAS_ERROR("ilias-sql", "Parser for type {} returned a mismatched type.", typeid(T).name());
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

// =========================================================
// 3. 运行时 NULL 检测 (修复 -Waddress 警告)
// =========================================================
template <typename T>
constexpr bool is_sql_null(const T &val) {
    using DecayT = std::decay_t<T>;

    if constexpr (std::is_same_v<DecayT, std::nullptr_t> || std::is_same_v<DecayT, SqlNull> ||
                  std::is_same_v<DecayT, SqlNull *>) {
        return true;
    }
    // 数组类型 (e.g. "literal") 永远不为空
    else if constexpr (std::is_array_v<DecayT> || std::is_array_v<T>) {
        return false;
    }
    // 指针类型
    else if constexpr (std::is_pointer_v<DecayT>) {
        return val == nullptr;
    }
    else if constexpr (std::is_same_v<DecayT, SqlDate>) {
        return val.type == SqlDate::kErrorTime;
    }
    // 智能指针 / Optional (拥有 operator bool)
    else if constexpr (requires { static_cast<bool>(val); } && !std::is_arithmetic_v<DecayT>) {
        return !static_cast<bool>(val);
    }
    // 基础类型 (int, float) 视为非空
    else {
        return false;
    }
}

// 明确处理 nullptr_t，确保绑定 nullptr 时返回指向 g_sql_null 的指针
inline auto to_sql_pointer(std::nullptr_t) -> SqlValuePointer {
    return SqlValuePointer {&g_sql_null};
}
inline auto to_sql_pointer(std::nullopt_t) -> SqlValuePointer {
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
    using type                              = SqlBool;
    using viewType                          = SqlBool;
    constexpr static SqlValueType valueType = SqlValueType::kBool;
    constexpr static auto         name      = "Bool";
};

template <>
struct SqlValueTraits<SqlValueType::kChar, void> {
    using type                              = SqlTinyInt;
    using viewType                          = SqlTinyInt;
    constexpr static SqlValueType valueType = SqlValueType::kChar;
    constexpr static auto         name      = "Char";
};

template <>
struct SqlValueTraits<SqlValueType::kInt, void> {
    using type                              = SqlInt;
    using viewType                          = SqlInt;
    constexpr static SqlValueType valueType = SqlValueType::kInt;
    constexpr static auto         name      = "Int";
};

template <>
struct SqlValueTraits<SqlValueType::kBigInt, void> {
    using type                              = SqlBigInt;
    using viewType                          = SqlBigInt;
    constexpr static SqlValueType valueType = SqlValueType::kBigInt;
    constexpr static auto         name      = "BigInt";
};

template <>
struct SqlValueTraits<SqlValueType::kFloat, void> {
    using type                              = SqlFloat;
    using viewType                          = SqlFloat;
    constexpr static SqlValueType valueType = SqlValueType::kFloat;
    constexpr static auto         name      = "Float";
};

template <>
struct SqlValueTraits<SqlValueType::kDouble, void> {
    using type                              = SqlDouble;
    using viewType                          = SqlDouble;
    constexpr static SqlValueType valueType = SqlValueType::kDouble;
    constexpr static auto         name      = "Double";
};

template <>
struct SqlValueTraits<SqlValueType::kText, void> {
    using type                              = SqlText;
    using viewType                          = SqlTextView;
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
auto &get(const SqlValue &v) {
    return *std::get_if<typename SqlValueTraits<T>::type>(&v);
}

template <SqlValueType T>
auto &get(SqlValue &v) {
    return *std::get_if<typename SqlValueTraits<T>::type>(&v);
}

template <SqlValueType T>
auto &get(const SqlValueView &v) {
    return *std::get_if<typename SqlValueTraits<T>::viewType>(&v);
}

template <SqlValueType T>
auto &get(SqlValueView &v) {
    return *std::get_if<typename SqlValueTraits<T>::viewType>(&v);
}

template <SqlValueType T>
auto &get(const SqlValuePointer &v) {
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
    if (is_sql_null(t)) {
        return SqlValuePointer {&g_sql_null};
    }
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, std::string_view> ||
                  std::is_convertible_v<std::decay_t<T>, std::string_view>) {
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
    if (is_sql_null(t)) {
        return SqlValuePointer {&g_sql_null};
    }
    if constexpr (std::is_same_v<std::decay_t<T>, std::string> || std::is_same_v<std::decay_t<T>, std::string_view> ||
                  std::is_convertible_v<std::decay_t<T>, std::string_view>) {
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
    auto format(const ILIAS_SQL_COMPLETE_NAMESPACE::SqlDate & date, Context &ctx) const {
        return format_to(ctx.out(), "{}", date.toLocalString());
    }
};

ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::SqlBlob) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(const ILIAS_SQL_COMPLETE_NAMESPACE::SqlBlob & blob, Context &ctx) const {
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
    auto format(const ILIAS_SQL_COMPLETE_NAMESPACE::SqlBlobView & blob, Context &ctx) const {
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
    auto format(const ILIAS_SQL_COMPLETE_NAMESPACE::SqlValue & value, Context &ctx) const {
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
    auto format(const ILIAS_SQL_COMPLETE_NAMESPACE::SqlValueView & value, Context &ctx) const {
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
    auto format(const ILIAS_SQL_COMPLETE_NAMESPACE::SqlValuePointer & value, Context &ctx) const {
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
    auto format(std::span<const char> value, Context &ctx) const {
        return format_to(ctx.out(), "{}", std::string_view{value.data(), value.size()});
    }
};
// clang-format on
