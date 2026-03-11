#pragma once

#include "ilias/sqlite/global.hpp"
#include "ilias/sql/types.hpp"

ILIAS_SQLITE_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

/**
 * @brief 注册所有Sqlite类型解析器和绑定器
 *
 * @param context 通用值转换器上下文
 */
void registerSqliteTypeParsers(SqlValueConverterContext& context);

ILIAS_SQLITE_NS_END