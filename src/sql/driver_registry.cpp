#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/global/config.h"
#include "ilias/sql/sql_plugin.hpp"
#include <filesystem>
#include <utility>
#include <vector>

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

namespace {
auto closePluginHandle(void *handle) -> void {
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

auto lookupPluginSymbol(void *handle, const char *name) -> void * {
#ifdef _WIN32
    return reinterpret_cast<void *>(GetProcAddress((HMODULE)handle, name));
#else
    return dlsym(handle, name);
#endif
}

class DynamicPluginConnection final : public IConnection {
public:
    using DestroyDriver = void (*)(IConnectionPtr);

    DynamicPluginConnection(IConnectionPtr connection, DestroyDriver destroyDriver)
        : mConnection(reinterpret_cast<IConnection *>(connection)), mDestroyDriver(destroyDriver) {
    }

    DynamicPluginConnection(const DynamicPluginConnection &)            = delete;
    DynamicPluginConnection &operator=(const DynamicPluginConnection &) = delete;

    ~DynamicPluginConnection() override {
        if (mConnection) {
            mDestroyDriver(static_cast<IConnectionPtr>(mConnection));
        }
    }

    auto sqlname() -> std::string override { return mConnection->sqlname(); }
    auto sqlinfo() -> std::string override { return mConnection->sqlinfo(); }
    auto connect() -> IoTask<void> override { return mConnection->connect(); }
    auto disconnect() -> IoTask<void> override { return mConnection->disconnect(); }
    auto selectDatabase(std::string_view name) -> IoTask<void> override { return mConnection->selectDatabase(name); }
    auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> override {
        return mConnection->prepare(sql);
    }
    auto execute(std::string_view sql) -> IoTask<size_t> override { return mConnection->execute(sql); }
    auto query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> override { return mConnection->query(sql); }
    auto beginTransaction() -> IoTask<bool> override { return mConnection->beginTransaction(); }
    auto commit() -> IoTask<bool> override { return mConnection->commit(); }
    auto rollback() -> IoTask<bool> override { return mConnection->rollback(); }
    auto syncRollback() -> bool override { return mConnection->syncRollback(); }
    auto lastInsertId() const -> int64_t override { return mConnection->lastInsertId(); }
    auto valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> override {
        return mConnection->valueConverterContext();
    }
    auto ping() -> IoTask<bool> override { return mConnection->ping(); }
    auto nativeHandle() const -> void * override { return mConnection->nativeHandle(); }

private:
    IConnection  *mConnection    = nullptr;
    DestroyDriver mDestroyDriver = nullptr;
};
} // namespace

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
        closePluginHandle(plugin);
    }
}

auto DriverManager::instance() -> DriverManager & {
    static DriverManager inst;
    return inst;
}

auto DriverManager::registerDriver(std::string_view name, DriverFactoryFn factory) -> IoResult<void> {
    if (drivers_.find(std::string(name)) != drivers_.end()) {
        ILIAS_WARN("ilias-sql", "{} has already registerd", name);
        return Err(SqlError::Code::DriverAlreadyRegistered);
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
    return Err(SqlError::Code::DriverNotFound);
}

auto DriverManager::loadPlugin(std::string_view path_) -> IoResult<void> {
#ifdef _WIN32
    std::filesystem::path path(path_);
    void                 *handle = LoadLibraryW(path.c_str());
#else
    std::string path   = std::string(path_);
    void       *handle = dlopen(path.c_str(), RTLD_LAZY);
#endif

    if (!handle) {
        // 最好有更详细的错误报告
        return Err(std::make_error_code(std::errc::no_such_file_or_directory));
    }

    // 定义注册函数的类型
    using createDriver        = IConnectionPtr (*)(const ConnectOptions_C *opts);
    using destroyDriver       = void (*)(IConnectionPtr connection);
    using getPluginName       = const char *(*)();
    using getPluginApiVersion = int (*)();

    auto create_driver = reinterpret_cast<createDriver>(lookupPluginSymbol(handle, "ilias_sql_plugin_create_driver"));
    auto destroy_driver =
        reinterpret_cast<destroyDriver>(lookupPluginSymbol(handle, "ilias_sql_plugin_destroy_driver"));
    auto get_plugin_name =
        reinterpret_cast<getPluginName>(lookupPluginSymbol(handle, "ilias_sql_plugin_get_plugin_name"));
    auto get_plugin_api_version =
        reinterpret_cast<getPluginApiVersion>(lookupPluginSymbol(handle, "ilias_sql_plugin_get_plugin_api_version"));

    if (!create_driver || !destroy_driver || !get_plugin_name || !get_plugin_api_version) {
        closePluginHandle(handle);
        return Err(std::make_error_code(std::errc::invalid_argument));
    }

    try {
        if (get_plugin_api_version() != ILIAS_SQL_API_VERSION) {
            closePluginHandle(handle);
            return Err(std::make_error_code(std::errc::operation_not_supported));
        }
        auto pluginName = get_plugin_name();
        if (!pluginName || pluginName[0] == '\0') {
            closePluginHandle(handle);
            return Err(std::make_error_code(std::errc::invalid_argument));
        }
        auto ret = registerDriver(pluginName, [create_driver, destroy_driver](const ConnectOptions &opts)
                                                  -> std::unique_ptr<IConnection> {
            ConnectOptions_C opts_c {};
            opts_c.host         = opts.host.c_str();
            opts_c.port         = opts.port;
            opts_c.user         = opts.user.c_str();
            opts_c.password     = opts.password.c_str();
            opts_c.database     = opts.database.c_str();
            opts_c.filename     = opts.filename.c_str();
            opts_c.extra_count  = static_cast<int>(opts.extra.size());
            opts_c.api_version  = ILIAS_SQL_API_VERSION;
            std::vector<const char *> extraKeys;
            std::vector<const char *> extraValues;
            extraKeys.reserve(opts.extra.size());
            extraValues.reserve(opts.extra.size());
            for (auto &[key, value] : opts.extra) {
                extraKeys.push_back(key.c_str());
                extraValues.push_back(value.c_str());
            }
            opts_c.extra_keys   = extraKeys.empty() ? nullptr : extraKeys.data();
            opts_c.extra_values = extraValues.empty() ? nullptr : extraValues.data();
            auto connection     = create_driver(&opts_c);
            if (!connection) {
                return nullptr;
            }
            return std::make_unique<DynamicPluginConnection>(connection, destroy_driver);
        });
        if (!ret) {
            closePluginHandle(handle);
            return Err(ret.error());
        }
    } catch (const std::exception &e) {
        closePluginHandle(handle);
        return Err(std::make_error_code(std::errc::invalid_argument));
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
