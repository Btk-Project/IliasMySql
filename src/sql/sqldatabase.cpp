#include "ilias/sql/sqldatabase.hpp"

ILIAS_SQL_NS_BEGIN

auto SqlDatabase::open(std::string_view name, ConnectOptions options) -> IoTask<SqlDatabase> {
    auto connect = DriverManager::instance().createConnection(name, options);
    if (!connect) {
        co_return Unexpected(connect.error());
    }
    auto conn = std::move(connect.value());
    auto ret  = co_await conn->connect();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    ILIAS_TRACE("ilias-sql", "open {} with version: {}", conn->sqlname(), conn->sqlinfo());
    co_return SqlDatabase(std::move(conn));
}

#ifdef ENABLE_SQLITE_PLUGINS
auto SqlDatabase::open_in_memory() -> IoTask<SqlDatabase> {
    ConnectOptions options;
    options.filename = ":memory:";
    return open("sqlite", options);
}
#endif

SqlDatabase::~SqlDatabase() {
}

auto SqlDatabase::close() -> IoTask<void> {
    if (mInTransaction) {
        co_return Unexpected(std::make_error_code(std::errc::device_or_resource_busy));
    }
    auto ret = co_await mConnection->disconnect();
    mConnection.reset();
    co_return ret;
}

auto SqlDatabase::transaction() -> IoTask<SqlTransaction> {
    if (mInTransaction) {
        co_return Unexpected(std::make_error_code(std::errc::device_or_resource_busy));
    }
    auto ret = SqlTransaction(*this);
    auto rc  = co_await ret.begin();
    if (!rc) {
        co_return Unexpected(rc.error());
    }
    co_return ret;
}

SqlTransaction::~SqlTransaction() {
    if (mState == State::kBeginned) {
        auto ret = connection();
        if (ret) {
            (*ret)->syncRollback();
        }
    }
    mDatabase.releaseTransaction();
}

auto SqlTransaction::commit() -> IoTask<void> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    auto connect_ret = connection();
    if (!connect_ret) {
        co_return Unexpected(connect_ret.error());
    }
    auto ret = co_await (*connect_ret)->commit();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mState = State::kCommitted;
    mDatabase.releaseTransaction();
    co_return {};
}

auto SqlTransaction::rollback() -> IoTask<void> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    auto connect_ret = connection();
    if (!connect_ret) {
        co_return Unexpected(connect_ret.error());
    }
    auto ret = co_await (*connect_ret)->rollback();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mState = State::kRolledBack;
    mDatabase.releaseTransaction();
    co_return {};
}

auto SqlTransaction::rebegin() -> IoTask<void> {
    return begin();
}

auto SqlTransaction::begin() -> IoTask<void> {
    if (mState != State::kUnused && mState != State::kRolledBack && mState != State::kCommitted) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    if (!mConnection) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    auto ret = co_await mConnection->beginTransaction();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mDatabase.mInTransaction = true; // 锁定数据库
    mState                   = State::kBeginned;
    co_return {};
}

ILIAS_SQL_NS_END