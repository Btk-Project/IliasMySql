#pragma once

#include "interfaces.hpp"

#include <functional>
#include <map>
#include <string>

#include "sqlerror.hpp"

ILIAS_SQL_NS_BEGIN

struct ConnectOptions {
    std::string host;
    uint16_t port = 0;
    std::string user;
    std::string password;
    std::string database;
    std::map<std::string, std::string> extra;
};

// 驱动创建函数原型
using DriverFactoryFn = std::function<std::unique_ptr<IConnection>(const ConnectOptions&)>;

class DriverManager {
public:
    static auto instance() -> DriverManager& {
        static DriverManager inst;
        return inst;
    }

    void registerDriver(std::string_view name, DriverFactoryFn factory) {
        drivers_[std::string(name)] = std::move(factory);
    }

    // 创建连接
    auto createConnection(std::string_view driverName, const ConnectOptions& opts) -> IoResult<std::unique_ptr<IConnection>> {
        if (auto it = drivers_.find(std::string(driverName)); it != drivers_.end()) {
            // 这里通常不是协程，因为只是创建对象，具体的 connect 动作可以在对象内部的 init 方法或者第一次操作时触发
            // 或者让 FactoryFn 返回 IoTask<unique_ptr>
            return it->second(opts);
        }
        return Unexpected(SqlError::Code::DriverNotFound);
    }

private:
    std::map<std::string, DriverFactoryFn> drivers_;
};

ILIAS_SQL_NS_END