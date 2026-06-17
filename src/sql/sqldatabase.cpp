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
    ILIAS_CO_TRYV(co_await ret.begin());
    co_return ret;
}

SqlTransaction::~SqlTransaction() {
    ILIAS_TRACE("ilias-sql", "SqlTransaction<{}> destroyed", (void *)this);
    if (mOwnsTransaction && mState == State::kBeginned) {
        auto ret = connection();
        if (ret) {
            (*ret)->syncRollback();
        }
    }
    if (mOwnsTransaction) {
        mDatabase.releaseTransaction();
    }
}

auto SqlTransaction::commit() -> IoTask<void> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    ILIAS_CO_TRY(auto connect_ret, connection());
    ILIAS_CO_TRYV(co_await connect_ret->commit());
    mState = State::kCommitted;
    if (mOwnsTransaction) {
        mDatabase.releaseTransaction();
        mOwnsTransaction = false;
    }
    co_return {};
}

auto SqlTransaction::rollback() -> IoTask<void> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    ILIAS_CO_TRY(auto connect_ret, connection());
    ILIAS_CO_TRYV(co_await connect_ret->rollback());
    mState = State::kRolledBack;
    if (mOwnsTransaction) {
        mDatabase.releaseTransaction();
        mOwnsTransaction = false;
    }
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
    ILIAS_CO_TRYV(co_await mConnection->beginTransaction());
    mDatabase.mInTransaction = true; // 锁定数据库
    mOwnsTransaction         = true;
    mState                   = State::kBeginned;
    co_return {};
}

ILIAS_SQL_NS_END
