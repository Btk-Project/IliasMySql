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
    ILIAS_INFO("ilias-sql", "open {} with version: {}", conn->sqlname(), conn->sqlinfo());
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
    return mConnection->disconnect();
}

auto SqlDatabase::execute(std::string_view query) -> IoTask<size_t> {
    return mConnection->execute(query);
}

auto SqlDatabase::transaction() -> IoTask<SqlTransaction> {
    auto ret = SqlTransaction(*this);
    auto rc  = co_await ret.begin();
    if (!rc) {
        co_return Unexpected(rc.error());
    }
    co_return ret;
}

SqlTransaction::~SqlTransaction() {
    if (mState == State::kBeginned) {
        mDatabase->syncRollback();
    }
}

auto SqlTransaction::commit() -> IoTask<void> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    mState   = State::kCommitted;
    auto ret = co_await mDatabase->commit();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return {};
}

auto SqlTransaction::rollback() -> IoTask<void> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    mState   = State::kRolledBack;
    auto ret = co_await mDatabase->rollback();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return {};
}

auto SqlTransaction::execute(std::string_view query) -> IoTask<size_t> {
    if (mState != State::kBeginned) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    co_return co_await mDatabase.execute(query);
}

auto SqlTransaction::begin() -> IoTask<void> {
    if (mState != State::kUnused && mState != State::kRolledBack) {
        co_return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
    }
    auto ret = co_await mDatabase->beginTransaction();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mState = State::kBeginned;
    co_return {};
}

ILIAS_SQL_NS_END