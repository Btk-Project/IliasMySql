#pragma once

#include "interfaces.hpp"

#include <functional>
#include <map>
#include <string>

#include "sqlerror.hpp"

ILIAS_SQL_NS_BEGIN

// 驱动创建函数原型
using DriverFactoryFn = std::function<std::unique_ptr<IConnection>(const ConnectOptions &)>;

class ILIAS_SQL_API DriverManager {
public:
    static auto instance() -> DriverManager &;
    auto        registerDriver(std::string_view name, DriverFactoryFn factory) -> IoResult<void>;
    auto        createConnection(std::string_view driverName, const ConnectOptions &opts)
        -> IoResult<std::unique_ptr<IConnection>>;
    auto loadPlugin(std::string_view path) -> IoResult<void>;
    auto pluginNames() const -> std::vector<std::string>;

private:
    DriverManager();
    ~DriverManager();
    DriverManager(const DriverManager &)            = delete;
    DriverManager &operator=(const DriverManager &) = delete;
    DriverManager(DriverManager &&)                 = delete;
    DriverManager &operator=(DriverManager &&)      = delete;

private:
    std::map<std::string, DriverFactoryFn> drivers_;
    std::vector<void *>                    plugins_;
};

ILIAS_SQL_NS_END