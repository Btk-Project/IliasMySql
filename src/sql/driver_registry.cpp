#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/global/config.h"
#include "ilias/sql/sql_plugin.hpp"
#include <filesystem>

#ifdef ENABLE_MYSQL_PLUGINS
#include "ilias/mysql/mysql.hpp"
extern bool _ilias_sql_register_plugin_mysql();
#endif

#ifdef ENABLE_SQLITE_PLUGINS
#include "ilias/sqlite/sqlite.hpp"
extern bool _ilias_sql_register_plugin_sqlite();
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

ILIAS_SQL_NS_BEGIN

DriverManager::DriverManager() {
#ifdef ENABLE_MYSQL_PLUGINS
    _ilias_sql_register_plugin_mysql();
#endif

#ifdef ENABLE_SQLITE_PLUGINS
    _ilias_sql_register_plugin_sqlite();
#endif
}

DriverManager::~DriverManager() {
    for (auto plugin : plugins_) {
#ifdef _WIN32
        FreeLibrary((HMODULE)plugin);
#else
        dlclose(plugin);
#endif
    }
}

auto DriverManager::instance() -> DriverManager & {
    static DriverManager inst;
    return inst;
}

auto DriverManager::registerDriver(std::string_view name, DriverFactoryFn factory) -> IoResult<void> {
    if (drivers_.find(std::string(name)) != drivers_.end()) {
        ILIAS_WARN("ilias-sql", "{} has already registerd", name);
        return Unexpected(SqlError::Code::DriverAlreadyRegistered);
    }
    ILIAS_TRACE("ilias-sql", "Register driver: {}", name);
    drivers_[std::string(name)] = std::move(factory);
    return {};
}

// 创建连接
auto DriverManager::createConnection(std::string_view driverName, const ConnectOptions &opts)
    -> IoResult<std::unique_ptr<IConnection>> {
    if (auto it = drivers_.find(std::string(driverName)); it != drivers_.end()) {
        return it->second(opts);
    }
    return Unexpected(SqlError::Code::DriverNotFound);
}

auto DriverManager::loadPlugin(std::string_view path_) -> IoResult<void> {
#ifdef _WIN32
    std::filesystem::path path(path_);
    HMODULE               handle = LoadLibraryW(path.c_str());
#else
    std::string path   = std::string(path_);
    void       *handle = dlopen(path.c_str(), RTLD_LAZY);
#endif

    if (!handle) {
        // 最好有更详细的错误报告
        return Unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }

    // 定义注册函数的类型
    using createDriver        = IConnectionPtr (*)(const ConnectOptions_C *opts);
    using getPluginName       = const char *(*)();
    using getPluginApiVersion = int (*)();

#ifdef _WIN32
    createDriver        create_driver   = (createDriver)GetProcAddress(handle, "ilias_sql_plugin_create_driver");
    getPluginName       get_plugin_name = (getPluginName)GetProcAddress(handle, "ilias_sql_plugin_get_name");
    getPluginApiVersion get_plugin_api_version =
        (getPluginApiVersion)GetProcAddress(handle, "ilias_sql_plugin_get_api_version");
#else
    createDriver        create_driver          = (createDriver)dlsym(handle, "ilias_sql_plugin_create_driver");
    getPluginName       get_plugin_name        = (getPluginName)dlsym(handle, "ilias_sql_plugin_get_name");
    getPluginApiVersion get_plugin_api_version = (getPluginApiVersion)dlsym(handle, "ilias_sql_plugin_get_api_version");
#endif

    if (!create_driver || !get_plugin_name || !get_plugin_api_version) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    try {
        if (get_plugin_api_version() != ILIAS_SQL_API_VERSION) {
            return Unexpected(std::make_error_code(std::errc::operation_not_supported));
        }
        registerDriver(get_plugin_name(), [create_driver](const ConnectOptions &opts) -> std::unique_ptr<IConnection> {
            ConnectOptions_C opts_c;
            opts_c.host         = opts.host.c_str();
            opts_c.port         = opts.port;
            opts_c.user         = opts.user.c_str();
            opts_c.password     = opts.password.c_str();
            opts_c.database     = opts.database.c_str();
            opts_c.filename     = opts.filename.c_str();
            opts_c.extra_count  = opts.extra.size();
            opts_c.api_version  = ILIAS_SQL_API_VERSION;
            opts_c.extra_keys   = new const char *[opts.extra.size()];
            opts_c.extra_values = new const char *[opts.extra.size()];
            int i               = 0;
            for (auto &[key, value] : opts.extra) {
                opts_c.extra_keys[i]   = key.c_str();
                opts_c.extra_values[i] = value.c_str();
                i++;
            }
            return std::unique_ptr<IConnection>(reinterpret_cast<IConnection *>(create_driver(&opts_c)));
        });
    } catch (const std::exception &e) {
#ifdef _WIN32
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    plugins_.push_back(handle);
    return {};
}

auto DriverManager::pluginNames() const -> std::vector<std::string> {
    std::vector<std::string> plugins;
    for (auto &[name, _] : drivers_) {
        plugins.push_back(name);
    }
    return plugins;
}

ILIAS_SQL_NS_END