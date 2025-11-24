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
#include "detail/global.hpp"

ILIAS_SQL_NS_BEGIN

// 统一的时间类型，不依赖 MYSQL_TIME
using SqlTimePoint = std::chrono::system_clock::time_point;

// 空值类型
struct SqlNull {};

// 二进制视图
using SqlBlobView = std::span<const std::byte>;
// 二进制拥有权对象
using SqlBlob = std::vector<std::byte>;

// 数据库值的通用变体
using SqlValue = std::variant<
    SqlNull,
    bool,
    int32_t,
    int64_t,
    double,
    std::string,       // Text
    SqlBlob,           // Binary
    SqlTimePoint       // Timestamp/Date
>;

// 用于参数绑定的轻量级变体 (避免拷贝 string/blob)
using SqlValueView = std::variant<
    SqlNull,
    bool,
    int32_t,
    int64_t,
    double,
    std::string_view,
    SqlBlobView,
    SqlTimePoint
>;

ILIAS_SQL_NS_END