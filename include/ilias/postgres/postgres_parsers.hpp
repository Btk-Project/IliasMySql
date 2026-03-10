#pragma once

#include "ilias/postgres/global.hpp"
#include "ilias/postgres/postgres_context.hpp"
#include "ilias/sql/types.hpp"

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

/**
 * @brief 注册所有PostgreSQL类型解析器和绑定器
 *
 * @param context PostgreSQL值转换器上下文
 */
void registerPostgresTypeParsers(PostgresValueConverterContext& context);

ILIAS_POSTGRES_NS_END
