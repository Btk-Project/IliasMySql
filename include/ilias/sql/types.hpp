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

// 统一的时间类型，不依赖 MYSQL_TIME
struct ILIAS_SQL_API SqlDate {
    enum TimeType {
        kErrorTime = 0, // 错误时间
        kDate,          // 2022-01-01
        kDateTime,      // 2022-01-01 00:00:00.000000
        kTime,          // 00:00:00.000000
    };
    inline SqlDate(int year = 0, int month = 0, int day = 0, int hour = 0, int minute = 0, int second = 0) {
        setTime(year, month, day, hour, minute, second);
    }
    inline SqlDate(struct tm *timeinfo) { setTime(timeinfo); }
    inline SqlDate(std::chrono::system_clock::time_point tp) { setTime(tp); }
    inline SqlDate(std::chrono::milliseconds timestamp) { setTime(timestamp); }
    inline SqlDate(std::string_view str, std::string_view fmt = "%Y-%m-%d %H:%M:%S") { setTime(str, fmt); }

    auto setTime(std::chrono::system_clock::time_point tp) -> void;
    auto setTime(std::chrono::milliseconds timestamp) -> void;
    auto setTime(int year, int month, int day, int hour, int minute, int second, int microsecond = 0) -> void;
    auto setDate(int year, int month, int day) -> void;
    auto setTime(int hour, int minute, int second) -> void;
    auto setTime(struct tm *timeinfo) -> void;
    auto setTime(std::string_view str, std::string_view fmt = "%Y-%m-%d %H:%M:%S") -> void;
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

// 二进制视图
using SqlBlobView = std::span<const std::byte>;
// 二进制拥有权对象
using SqlBlob = std::vector<std::byte>;

// 数据库值的通用变体
using SqlValue = std::variant<SqlNull,     // NULL
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
enum class SqlValueType { kNull = 0, kChar, kInt, kBigInt, kFloat, kDouble, kText, kBlob, kDate, kMax };

// 用于参数绑定的轻量级变体 (避免拷贝 string/blob)
using SqlValueView =
    std::variant<SqlNull, char, int32_t, int64_t, float, double, std::string_view, SqlBlobView, SqlDate>;

ILIAS_SQL_NS_END