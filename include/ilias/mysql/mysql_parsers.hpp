#pragma once

#include "ilias/mysql/global.hpp"
#include "ilias/sql/types.hpp"

ILIAS_MYSQL_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

/**
 * @brief 注册所有MySQL类型解析器和绑定器
 *
 * @param context 通用值转换器上下文
 */
void registerMysqlTypeParsers(SqlValueConverterContext& context);

ILIAS_MYSQL_NS_END
