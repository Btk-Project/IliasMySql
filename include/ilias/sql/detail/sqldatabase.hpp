/**
 * @file database.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief sql I/O
 * @version 0.1
 * @date 2025-1-9
 *
 * @copyright Copyright (c) 2024
 *
 */
#pragma once

#include <ilias/io/context.hpp>
#include <ilias/io/fd_utils.hpp>
#include <ilias/io/method.hpp>
#include <ilias/io/system_error.hpp>
#include <ilias/net/poller.hpp>
#include <ilias/net/sockfd.hpp>
#include <ilias/task/when_any.hpp>

#include "detail/global.hpp"
#include "sqlopt.hpp"
#include "sqlerror.hpp"

ILIAS_SQL_NS_BEGIN

class ISqlDatabase {
public:
    ISqlDatabase();
    ISqlDatabase(const ISqlDatabase &other);
    ~ISqlDatabase();

    ISqlDatabase &operator=(const ISqlDatabase &other);

    auto open() -> IoTask<void>;
    auto open(std::string_view username, std::string_view password) -> IoTask<void>;
    auto close() -> IoTask<void>;
    auto setUserName(std::string_view username) -> void;
    auto setPassword(std::string_view password) -> void;
    auto setHost(std::string_view host) -> void;
    auto setPort(unsigned short port) -> void;
    auto setDatabase(std::string_view database) -> void;
    auto setConnectOptions(std::string_view options = "") -> void;
    auto getConnectOptions() -> std::string;
    auto isOpen() const -> bool;
    auto selectDb(std::string_view db) -> IoTask<void>;
    template <typename T>
        requires std::is_base_of_v<sqlopt::OptionBase, T>
    auto setOption(const T &option) -> SqlError;
    template <typename T>
        requires std::is_base_of_v<sqlopt::OptionBase, T>
    auto getOption(T &option) -> SqlError;
};

ILIAS_SQL_NS_END
