#pragma once

#include "ilias/sql/global/global.hpp"

ILIAS_SQL_NS_BEGIN

// 标签定义
struct SqlTags {
    bool unique         = false;
    bool not_null       = false;
    bool primary_key    = false;
    bool auto_increment = false;
    bool default_value  = false;
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
} // namespace detail

ILIAS_SQL_NS_END