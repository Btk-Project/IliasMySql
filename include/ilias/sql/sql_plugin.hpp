#pragma once

#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/global/global.hpp"
#include <memory>
#ifndef BUILD_AS_DYNAMIC_PLUGIN
#include "ilias/sql/driver_registry.hpp"
#endif

extern "C" {
struct ConnectOptions_C {
    const char *host;
    uint16_t    port;
    const char *user;
    const char *password;
    const char *database;
    const char *filename;
    int         api_version;

    // 对于 map，最简单的方式是使用两个平行的数组
    const char **extra_keys;
    const char **extra_values;
    int          extra_count;
};

typedef struct IConnection *IConnectionPtr;

// 工厂函数，用于创建IConnection实例
ILIAS_SQL_PLUGIN IConnectionPtr ilias_sql_plugin_create_driver(const ConnectOptions_C *options);

// 获取插件名字的函数
ILIAS_SQL_PLUGIN const char *ilias_sql_plugin_get_plugin_name();

// 获取插件API版本的函数
ILIAS_SQL_PLUGIN int ilias_sql_plugin_get_plugin_api_version();
}
#ifdef BUILD_AS_DYNAMIC_PLUGIN
#define ILIAS_SQL_REGISTER_PLUGIN(name)                                                                                \
    ILIAS_SQL_COMPLETE_NAMESPACE::IConnection *create_connection_##name(                                               \
        const ILIAS_SQL_COMPLETE_NAMESPACE::ConnectOptions &options);                                                  \
    IConnectionPtr ilias_sql_plugin_create_driver(const ConnectOptions_C *options) {                                   \
        ILIAS_SQL_COMPLETE_NAMESPACE::ConnectOptions opts;                                                             \
        opts.host     = options->host;                                                                                 \
        opts.port     = options->port;                                                                                 \
        opts.user     = options->user;                                                                                 \
        opts.password = options->password;                                                                             \
        opts.database = options->database;                                                                             \
        opts.filename = options->filename;                                                                             \
        for (int i = 0; i < options->extra_count; ++i) {                                                               \
            opts.extra.insert(std::make_pair(options->extra_keys[i], options->extra_values[i]));                       \
        }                                                                                                              \
        return reinterpret_cast<IConnectionPtr>(create_connection_##name(opts));                                       \
    }                                                                                                                  \
    const char *ilias_sql_plugin_get_plugin_name() {                                                                   \
        return #name;                                                                                                  \
    }                                                                                                                  \
    int ilias_sql_plugin_get_plugin_api_version() {                                                                    \
        return ILIAS_SQL_API_VERSION;                                                                                  \
    }                                                                                                                  \
    ILIAS_SQL_COMPLETE_NAMESPACE::IConnection *create_connection_##name(                                               \
        const ILIAS_SQL_COMPLETE_NAMESPACE::ConnectOptions &options)
#else
#define ILIAS_SQL_REGISTER_PLUGIN(name)                                                                                \
    ILIAS_SQL_COMPLETE_NAMESPACE::IConnection *create_connection_##name(                                               \
        const ILIAS_SQL_COMPLETE_NAMESPACE::ConnectOptions &options);                                                  \
    static const bool _ilias_sql_register_executor_##name = []() {                                                     \
        auto &instance = ILIAS_SQL_COMPLETE_NAMESPACE::DriverManager::instance();                                      \
        instance.registerDriver(#name,                                                                                 \
                                [](const ILIAS_SQL_COMPLETE_NAMESPACE::ConnectOptions &options)                        \
                                    -> std::unique_ptr<ILIAS_SQL_COMPLETE_NAMESPACE::IConnection> {                    \
                                    return std::unique_ptr<ILIAS_SQL_COMPLETE_NAMESPACE::IConnection>(                 \
                                        create_connection_##name(options));                                            \
                                });                                                                                    \
        return true;                                                                                                   \
    }();                                                                                                               \
    bool _ilias_sql_register_plugin_##name() {                                                                         \
        return _ilias_sql_register_executor_##name;                                                                    \
    };                                                                                                                 \
    ILIAS_SQL_COMPLETE_NAMESPACE::IConnection *create_connection_##name(                                               \
        const ILIAS_SQL_COMPLETE_NAMESPACE::ConnectOptions &options)
#endif
