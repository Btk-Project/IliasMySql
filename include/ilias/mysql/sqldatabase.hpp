/**
 * @file database.hpp
 * @author llhsdmd (llhsdmd@gmail.com)
 * @brief mysql I/O
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
#include <mariadb/mysql.h>
#include <mariadb/mysqld_error.h>

#include "detail/global.hpp"
#include "detail/mysql.hpp"
#include "detail/sqlopt.hpp"

ILIAS_MYSQL_NS_BEGIN

class SqlDatabase {
public:
    SqlDatabase();
    SqlDatabase(const SqlDatabase &other);
    ~SqlDatabase();

    SqlDatabase &operator=(const SqlDatabase &other);

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

private:
    auto mysql() -> std::shared_ptr<detail::MySql>;
    auto parserOptions() -> void;

    friend class SqlQuery;

private:
    std::string                    mUserName       = "";
    std::string                    mPassword       = "";
    std::string                    mHost           = "";
    unsigned short                 mPort           = 0;
    std::string                    mDatabase       = "";
    std::string                    mUnixSocket     = "";
    unsigned long                  mClientFlag     = 0;
    std::string                    mConnectOptions = "";
    std::shared_ptr<detail::MySql> mMySql          = nullptr;
};

inline SqlDatabase::SqlDatabase() {
    mMySql = std::make_shared<detail::MySql>();
}

inline SqlDatabase::SqlDatabase(const SqlDatabase &other)
    : mUserName(other.mUserName), mPassword(other.mPassword), mHost(other.mHost), mPort(other.mPort),
      mDatabase(other.mDatabase), mMySql(other.mMySql) {
}

inline SqlDatabase::~SqlDatabase() {
    if (mMySql.use_count() == 1) {
        mMySql->close();
    }
    mMySql.reset();
}

inline SqlDatabase &SqlDatabase::operator=(const SqlDatabase &other) {
    mUserName = other.mUserName;
    mPassword = other.mPassword;
    mHost     = other.mHost;
    mPort     = other.mPort;
    mDatabase = other.mDatabase;
    mMySql    = other.mMySql;
    return *this;
}

inline auto SqlDatabase::parserOptions() -> void {
    if (mConnectOptions.empty()) {
        return;
    }
}

inline auto SqlDatabase::open() -> IoTask<void> {
    // mysql need a valid username and password
    if (mUserName.empty() || mPassword.empty()) {
        co_return Unexpected(SqlError::INVALID_PARAMETER);
    }
    co_return co_await open(mUserName, mPassword);
}

inline auto SqlDatabase::open(std::string_view username, std::string_view password) -> IoTask<void> {
    if (mMySql.use_count() != 1) {
        mMySql = std::make_shared<detail::MySql>();
    }
    mUserName = std::string(username);
    mPassword = std::string(password);

    co_return co_await mMySql->connect(mHost, mUserName, mPassword, mDatabase, mPort);
}

inline auto SqlDatabase::selectDb(std::string_view db) -> IoTask<void> {
    auto ret = co_await mMySql->selectDb(db);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mDatabase = db;
    co_return {};
}

inline auto SqlDatabase::setUserName(std::string_view username) -> void {
    mUserName = std::string(username);
}

inline auto SqlDatabase::setPassword(std::string_view password) -> void {
    mPassword = std::string(password);
}

inline auto SqlDatabase::setHost(std::string_view host) -> void {
    mHost = std::string(host);
}

inline auto SqlDatabase::setPort(unsigned short port) -> void {
    mPort = port;
}

inline auto SqlDatabase::setDatabase(std::string_view database) -> void {
    mDatabase = std::string(database);
}

inline auto SqlDatabase::close() -> IoTask<void> {
    co_return co_await mMySql->disconnect();
}

inline auto SqlDatabase::isOpen() const -> bool {
    return mMySql != nullptr;
}

inline auto SqlDatabase::mysql() -> std::shared_ptr<detail::MySql> {
    return mMySql;
}

template <typename T>
    requires std::is_base_of_v<sqlopt::OptionBase, T>
auto SqlDatabase::setOption(const T &option) -> SqlError {
    ILIAS_ASSERT_MSG(mMySql != nullptr, "sql ptr is empty");
    auto ret = mMySql->setOpt(option);
    if (ret != 0) {
        ILIAS_ERROR("sql", "set option error {}", ret);
    }
    return (SqlError::Code)ret;
}

template <typename T>
    requires std::is_base_of_v<sqlopt::OptionBase, T>
auto SqlDatabase::getOption(T &option) -> SqlError {
    ILIAS_ASSERT_MSG(mMySql != nullptr, "sql ptr is empty");
    auto ret = mMySql->getOpt(option);
    if (ret != 0) {
        ILIAS_ERROR("sql", "get option error {}", ret);
    }
    return (SqlError::Code)ret;
}

ILIAS_MYSQL_NS_END
