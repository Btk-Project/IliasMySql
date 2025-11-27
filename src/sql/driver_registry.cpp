#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/global/config.h"

#ifdef ENABLE_MYSQL_PLUGINS
#include "ilias/mysql/mysql.hpp"
#endif

#ifdef ENABLE_SQLITE_PLUGINS
#include "ilias/sqlite/sqlite.hpp"
#endif

ILIAS_SQL_NS_BEGIN

DriverManager::DriverManager() {
#ifdef ENABLE_MYSQL_PLUGINS
    mysql::ilias_register_sql_plugin(this);
#endif

#ifdef ENABLE_SQLITE_PLUGINS
    sqlite::ilias_register_sql_plugin(this);
#endif
}

auto DriverManager::instance() -> DriverManager & {
    static DriverManager inst;
    return inst;
}

void DriverManager::registerDriver(std::string_view name, DriverFactoryFn factory) {
    drivers_[std::string(name)] = std::move(factory);
}

// 创建连接
auto DriverManager::createConnection(std::string_view driverName, const ConnectOptions &opts)
    -> IoResult<std::unique_ptr<IConnection>> {
    if (auto it = drivers_.find(std::string(driverName)); it != drivers_.end()) {
        return it->second(opts);
    }
    return Unexpected(SqlError::Code::DriverNotFound);
}
ILIAS_SQL_NS_END