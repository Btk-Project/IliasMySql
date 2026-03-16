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
#include <typeindex>
#ifdef __GNUC__
#include <cxxabi.h>
#endif

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
    auto        to_time_point() const -> std::chrono::system_clock::time_point;
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

// 类型索引对应的枚举
enum class SqlValueType { kNull = 0, kBool, kChar, kInt, kBigInt, kFloat, kDouble, kText, kBlob, kDate, kMax };

template <typename T>
concept DereferenceableAndNullable = requires(T &t) {
    { t.has_value() } -> std::convertible_to<bool>; // optional-like
    { t.reset() };                                  // optional-like
    { *t };                                         // dereferenceable
};
template <typename T>
struct OptionalLikeType;
template <typename T>
    requires(!DereferenceableAndNullable<T>)
struct OptionalLikeType<T> {
    constexpr static bool value = false;
    using type                  = T;
};

template <typename T>
    requires DereferenceableAndNullable<T>
struct OptionalLikeType<T> {
    constexpr static bool value = true;
    using type                  = std::decay_t<decltype(*std::declval<T>())>;
    bool has_value(const T &t) const noexcept { return t.has_value(); }
};
class SqlCellView;
using SqlParserResult = IoResult<std::any>;
using SqlParserFunc   = std::function<SqlParserResult(const SqlCellView &)>;
using SqlBinderResult = IoResult<std::unique_ptr<void, void (*)(void *)>>;
inline auto make_null_sql_binder_result() {
    return std::unique_ptr<void, void (*)(void *)> {nullptr, [](void *) {}};
}
template <typename T>
inline auto make_sql_binder_result_for_store(T &&t) {
    return std::unique_ptr<void, void (*)(void *)> {new T(std::forward<T>(t)),
                                                    [](void *ptr) { delete static_cast<T *>(ptr); }};
}
using SqlBindFunc = std::function<SqlBinderResult(const SqlCellView &, const std::any &)>;
class ILIAS_SQL_API SqlValueConverterContext {
public:
    virtual ~SqlValueConverterContext() = default;
    virtual auto findTypeParser(std::type_index type) const -> SqlParserFunc {
        // ILIAS_TRACE("ilias-sql", "find parser for type {}", type.name());
        auto item = mParsers.find(type);
        if (item != mParsers.end()) {
            return item->second;
        }
        return nullptr;
    }
    virtual auto findTypeBinder(std::type_index type) const -> SqlBindFunc {
        // ILIAS_TRACE("ilias-sql", "find binder for type {}", type.name());
        auto item = mBinders.find(type);
        if (item != mBinders.end()) {
            return item->second;
        }
        return nullptr;
    }
    virtual auto parserTypes() const -> const std::map<std::type_index, SqlParserFunc> & { return mParsers; }
    virtual auto binderTypes() const -> const std::map<std::type_index, SqlBindFunc> & { return mBinders; }

    template <typename T>
    auto registerType(SqlParserFunc func) -> void {
        registerType(std::type_index(typeid(T)), func);
    }
    template <typename T>
    auto registerType(SqlBindFunc func) -> void {
        using value_type = std::remove_const_t<T>;
        registerType(std::type_index(typeid(const value_type)), func);
    }

protected:
    auto registerType(std::type_index type, SqlParserFunc func) -> void;
    auto registerType(std::type_index type, SqlBindFunc func) -> void;
    auto parserTypes() -> std::map<std::type_index, SqlParserFunc> &;
    auto binderTypes() -> std::map<std::type_index, SqlBindFunc> &;

private:
    std::map<std::type_index, SqlParserFunc> mParsers;
    std::map<std::type_index, SqlBindFunc>   mBinders;
};

/**
 * @brief 数据库单元格视图
 * 该数据视图在Result::next()和Result释放数据前有效
 */
class SqlCellView {
public:
    using ValueString  = std::tuple<std::string_view, int>;
    using NativeValue  = std::any;
    using ValuePointer = std::tuple<const void *, int, std::type_index>;
    enum class DataFormat { kUnknown = 0, kString, kNativeValue, kValuePointer };

    SqlCellView()                               = default;
    SqlCellView(const SqlCellView &)            = default;
    SqlCellView(SqlCellView &&)                 = default;
    SqlCellView &operator=(const SqlCellView &) = default;
    SqlCellView &operator=(SqlCellView &&)      = default;
    ~SqlCellView()                              = default;
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt) : mContext(ctxt) {}
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt, std::string_view data, uint32_t native_type, int index)
        : mContext(ctxt), mSqlValue(ValueString(data, native_type)), mIndex(index) {}
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt, NativeValue data, int index)
        : mContext(ctxt), mSqlValue(data), mIndex(index) {}
    SqlCellView(std::shared_ptr<SqlValueConverterContext> ctxt, const void *data, int size, std::type_index type,
                int index)
        : mContext(ctxt), mSqlValue(ValuePointer(data, size, type)), mIndex(index) {}
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
    auto sql_value() const -> const std::any & {
        if (std::holds_alternative<NativeValue>(mSqlValue)) {
            return std::get<NativeValue>(mSqlValue);
        }
        return mNullValue;
    }
    /**
     * @brief 获取原始数据指针
     *
     * @return const void*
     */
    auto raw_value() const -> const void * {
        if (std::holds_alternative<ValuePointer>(mSqlValue)) {
            return std::get<0>(std::get<ValuePointer>(mSqlValue));
        }
        return nullptr;
    }
    auto raw_value_size() const -> size_t {
        if (std::holds_alternative<ValuePointer>(mSqlValue)) {
            return std::get<1>(std::get<ValuePointer>(mSqlValue));
        }
        return 0;
    }
    /**
     * @brief 获取原始数据类型
     *
     * @return std::type_index
     */
    auto raw_type() const -> std::type_index {
        if (std::holds_alternative<ValuePointer>(mSqlValue)) {
            return std::get<2>(std::get<ValuePointer>(mSqlValue));
        }
        return std::type_index {typeid(void)};
    }
    /**
     * @brief 获取格式化后的数据
     *
     * @return std::string
     */
    auto formatted_value() const -> std::string_view {
        if (std::holds_alternative<ValueString>(mSqlValue)) {
            return std::get<0>(std::get<ValueString>(mSqlValue));
        }
        return std::string_view {};
    }
    /**
     * @brief   获取格式化后的数据类型
     *
     * @return  int
     */
    auto formatted_type() const -> int {
        if (std::holds_alternative<ValueString>(mSqlValue)) {
            return std::get<1>(std::get<ValueString>(mSqlValue));
        }
        return -1;
    }
    auto context() const -> SqlValueConverterContext * { return mContext.get(); }

private:
    std::shared_ptr<SqlValueConverterContext>                            mContext;
    std::variant<std::monostate, ValueString, NativeValue, ValuePointer> mSqlValue;
    int                                                                  mIndex = 0;
    const static std::any                                                mNullValue;
};

inline const std::any SqlCellView::mNullValue = std::any {};

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
using std::type_index;
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
ILIAS_FORMATTER(ILIAS_SQL_NAMESPACE::type_index) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(const std::type_index & value, Context &ctx) const {
#ifdef __GNUC__
        static char buffer[256];
        static size_t size = sizeof(buffer);
        memset(buffer, 0, sizeof(buffer));
        auto name = abi::__cxa_demangle(value.name(), buffer, &size, nullptr);
        return format_to(ctx.out(), "{}", name);
#else
        return format_to(ctx.out(), "{}", value.name());
#endif
    }
};

#if NEKO_CPP_PLUS < 23
ILIAS_FORMATTER_T_RAW(,std::span<const char>) {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename Context> 
    auto format(std::span<const char> value, Context &ctx) const {
        return format_to(ctx.out(), "{}", std::string_view{value.data(), value.size()});
    }
};
#endif
// clang-format on

ILIAS_SQL_NS_BEGIN
template <typename T>
auto SqlCellView::as() const -> IoResult<T> {
    constexpr bool is_optional = NEKO_NAMESPACE::detail::is_optional<T>::value;
    constexpr bool is_pointer  = std::is_pointer_v<T>;
    constexpr bool is_nullable = is_optional || is_pointer || std::is_same_v<T, SqlDate>;
    auto           type_index  = std::type_index(typeid(std::remove_cvref_t<T>));
    if constexpr (is_optional) {
        type_index = std::type_index(typeid(typename T::value_type));
    }
    if (!mContext) {
        ILIAS_ERROR("ilias-sql", "SqlCellView::as() called without context");
        return Unexpected(SqlError::Code::NoContext);
    }
    auto parser_func = mContext->findTypeParser(type_index);
    if (!parser_func) {
        ILIAS_ERROR("ilias-sql", "Unsupport convert from sql type: {}", type_index);
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
    ILIAS_ERROR("ilias-sql", "Parser for type {} returned a mismatched type.", std::type_index(typeid(T)));
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}
ILIAS_SQL_NS_END
