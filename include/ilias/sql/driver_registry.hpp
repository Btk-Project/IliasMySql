#pragma once

#include "interfaces.hpp"

#include <functional>
#include <map>
#include <string>

#include "sqlerror.hpp"

ILIAS_SQL_NS_BEGIN

struct ConnectOptions {
    std::string                        host;
    uint16_t                           port = 0;
    std::string                        user;
    std::string                        password;
    std::string                        database;
    std::map<std::string, std::string> extra;
};

// 驱动创建函数原型
using DriverFactoryFn = std::function<std::unique_ptr<IConnection>(const ConnectOptions &)>;

class DriverManager {
public:
    static auto instance() -> DriverManager &;
    void        registerDriver(std::string_view name, DriverFactoryFn factory);
    auto        createConnection(std::string_view driverName, const ConnectOptions &opts)
        -> IoResult<std::unique_ptr<IConnection>>;

private:
    DriverManager();
    ~DriverManager()                                = default;
    DriverManager(const DriverManager &)            = delete;
    DriverManager &operator=(const DriverManager &) = delete;
    DriverManager(DriverManager &&)                 = delete;
    DriverManager &operator=(DriverManager &&)      = delete;

private:
    std::map<std::string, DriverFactoryFn> drivers_;
};

ILIAS_SQL_NS_END