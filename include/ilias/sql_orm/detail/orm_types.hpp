#pragma once

#include "ilias/sql/global/global.hpp"

ILIAS_SQL_NS_BEGIN

// 标签定义
struct SqlTags {
     // 核心约束
    bool primary_key    = false; // 主键约束
    bool not_null       = false; // 非空约束
    bool unique         = false; // 唯一约束
    bool auto_increment = false; // 自增约束
    bool index          = false; // 声明该列需要被索引

    // 数据类型修饰符
    bool unsigned_type = false; // 无符号类型 (适用于整数)
    int  length        = 0;     // 字符串长度

    // 常用逻辑标记 (用于自动化生成时间戳)
    bool created_at = false; // 标记为创建时间字段
    bool updated_at = false; // 标记为更新时间字段

    // 验证方法
    ILIAS_SQL_API bool isValid() const;
    ILIAS_SQL_API std::vector<std::string> getValidationErrors() const;
    
    // 约束组合辅助方法
    ILIAS_SQL_API bool hasTimestampBehavior() const;
    ILIAS_SQL_API bool requiresIndex() const;
};

// 前置声明
class SqlDatabase;
template <typename T>
class SqlResult;
template <typename T>
class SqlStatement;

namespace detail {
class SqlCondition;
class SqlStatementBinder;
class SelectBuilder;
template <typename... ResultTypes>
class ProjectedSelectBuilder;
template <typename T>
class TypedColumn;
// 工具函数声明
ILIAS_SQL_API std::string join_strs(const std::vector<std::string> &vec, const std::string &sep,
                                    const std::string &prefix = "", const std::string &suffix = "");
} // namespace detail

ILIAS_SQL_NS_END