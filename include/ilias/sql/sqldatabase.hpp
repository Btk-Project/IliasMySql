#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>
#include <string_view>

#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/detail/sql_base_api.hpp"

ILIAS_SQL_NS_BEGIN

// 前向声明
class SqlDatabase;
class SqlTransaction;
class ILIAS_SQL_API SqlDatabase : public detail::SqlApiMixin<SqlDatabase> {
public:
    static auto open(std::string_view name, ConnectOptions options) -> IoTask<SqlDatabase>;
#ifdef ENABLE_SQLITE_PLUGINS
    static auto open_in_memory() -> IoTask<SqlDatabase>;
#endif
    SqlDatabase(std::unique_ptr<IConnection> connection) : mConnection(std::move(connection)) {}
    SqlDatabase(SqlDatabase &&)                 = default;
    SqlDatabase &operator=(SqlDatabase &&)      = default;
    SqlDatabase(const SqlDatabase &)            = delete;
    SqlDatabase &operator=(const SqlDatabase &) = delete;
    ~SqlDatabase();

    auto connection() -> IoResult<IConnection *> {
        if (mInTransaction) {
            return Unexpected(std::make_error_code(std::errc::device_or_resource_busy));
        }
        if (mConnection) {
            return mConnection.get();
        }
        return Unexpected(SqlError::Code::NotConnected);
    }

    auto close() -> IoTask<void>;

    // --- 派生类特有 API ---
    auto transaction() -> IoTask<SqlTransaction>;

private:
    void releaseTransaction() { mInTransaction = false; }

    std::unique_ptr<IConnection> mConnection;
    bool                         mInTransaction = false;

    friend class SqlTransaction;
};

class ILIAS_SQL_API SqlTransaction : public detail::SqlApiMixin<SqlTransaction> {
    enum class State {
        kUnused = 0,
        kBeginned,
        kCommitted,
        kRolledBack,
    };

public:
    SqlTransaction(SqlDatabase &db) : mDatabase(db) {
        auto ret = mDatabase.connection();
        if (!ret) {
            ILIAS_ERROR("ilias-sql", "SqlTransaction<{}> connection error: {}", (void *)this, ret.error().message());
        } else {
            ILIAS_TRACE("ilias-sql", "SqlTransaction<{}> connection success", (void *)this);
        }
        mConnection = ret.value_or(nullptr);
    }
    SqlTransaction(const SqlTransaction &)            = delete;
    SqlTransaction &operator=(const SqlTransaction &) = delete;
    SqlTransaction(SqlTransaction &&other)
        : mDatabase(other.mDatabase), mConnection(other.mConnection), mState(other.mState) {
        ILIAS_TRACE("ilias-sql", "SqlTransaction<{}> move constructor", (void *)this);
        other.mState      = State::kUnused;
        other.mConnection = nullptr;
    }
    SqlTransaction &operator=(SqlTransaction &&) = delete;
    ~SqlTransaction();

    auto connection() -> IoResult<IConnection *> {
        if (mState != State::kBeginned) {
            return Unexpected(std::make_error_code(std::errc::operation_not_permitted));
        }
        if (!mConnection) {
            return Unexpected(SqlError::Code::NotConnected);
        }
        return mConnection;
    }

    // --- 派生类特有 API ---
    auto rebegin() -> IoTask<void>;
    auto commit() -> IoTask<void>;
    auto rollback() -> IoTask<void>;

protected:
    auto begin() -> IoTask<void>;

    friend class SqlDatabase;

private:
    SqlDatabase &mDatabase;
    IConnection *mConnection = nullptr;
    State        mState      = State::kUnused;
};

ILIAS_SQL_NS_END