/**
 * @file sqldatabase.hpp
 * @brief Database connection and transaction management
 *
 * This file provides the SqlDatabase and SqlTransaction classes for managing
 * database connections and transactions in an async coroutine-friendly manner.
 */

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

/**
 * @class SqlDatabase
 * @brief Database connection wrapper with coroutine support
 *
 * SqlDatabase provides a high-level interface for database connections,
 * supporting async operations through c++23 coroutines. It manages
 * the underlying IConnection and provides transaction support.
 *
 * @note This class is move-only and cannot be copied.
 */
class ILIAS_SQL_API SqlDatabase : public detail::SqlApiMixin<SqlDatabase> {
public:
    /**
     * @brief Open a database connection
     *
     * @param name Database name or connection string
     * @param options Connection options (host, port, user, password, etc.)
     * @return IoTask<SqlDatabase> Async task that resolves to a SqlDatabase instance
     */
    static auto open(std::string_view name, ConnectOptions options) -> IoTask<SqlDatabase>;
    
#ifdef ENABLE_SQLITE_PLUGINS
    /**
     * @brief Open an in-memory SQLite database
     *
     * @return IoTask<SqlDatabase> Async task that resolves to a SqlDatabase instance
     */
    static auto open_in_memory() -> IoTask<SqlDatabase>;
#endif
    /**
     * @brief Construct from an existing IConnection
     * @param connection The underlying connection implementation
     */
    SqlDatabase(std::unique_ptr<IConnection> connection) : mConnection(std::move(connection)) {}
    
    /**
     * @brief Move constructor
     */
    SqlDatabase(SqlDatabase &&)                 = default;
    
    /**
     * @brief Move assignment operator
     */
    SqlDatabase &operator=(SqlDatabase &&)      = default;
    
    /**
     * @brief Copy constructor (deleted)
     */
    SqlDatabase(const SqlDatabase &)            = delete;
    
    /**
     * @brief Copy assignment operator (deleted)
     */
    SqlDatabase &operator=(const SqlDatabase &) = delete;
    
    /**
     * @brief Destructor
     */
    ~SqlDatabase();

    /**
     * @brief Get the underlying connection
     *
     * @return IoResult<IConnection*> The connection or an error if in transaction
     * @note Returns error if a transaction is currently active
     */
    auto connection() -> IoResult<IConnection *> {
        if (mInTransaction) {
            return Err(std::make_error_code(std::errc::device_or_resource_busy));
        }
        if (mConnection) {
            return mConnection.get();
        }
        return Err(SqlError::Code::NotConnected);
    }

    /**
     * @brief Close the database connection
     * @return IoTask<void> Async task that completes when the connection is closed
     */
    auto close() -> IoTask<void>;

    /**
     * @brief Begin a new transaction
     * @return IoTask<SqlTransaction> Async task that resolves to a SqlTransaction instance
     */
    auto transaction() -> IoTask<SqlTransaction>;

    auto lastNativeError() const -> std::optional<NativeSqlError> {
        if (!mConnection) {
            return std::nullopt;
        }
        return mConnection->lastNativeError();
    }

private:
    void releaseTransaction() { mInTransaction = false; }

    std::unique_ptr<IConnection> mConnection;
    bool                         mInTransaction = false;

    friend class SqlTransaction;
};

/**
 * @class SqlTransaction
 * @brief RAII transaction manager with automatic rollback
 *
 * SqlTransaction provides automatic transaction management with RAII semantics.
 * If a transaction is not explicitly committed, it will be rolled back
 * automatically when the object is destroyed.
 *
 * @note This class is move-only and cannot be copied.
 */
class ILIAS_SQL_API SqlTransaction : public detail::SqlApiMixin<SqlTransaction> {
    /**
     * @brief Transaction state enumeration
     */
    enum class State {
        kUnused = 0,     ///< Transaction not yet started
        kBeginned,       ///< Transaction active
        kCommitted,      ///< Transaction committed
        kRolledBack,     ///< Transaction rolled back
    };

public:
    /**
     * @brief Construct a transaction for the given database
     * @param db The database to create a transaction for
     */
    SqlTransaction(SqlDatabase &db) : mDatabase(db) {
        auto ret = mDatabase.connection();
        if (!ret) {
            ILIAS_ERROR("ilias-sql", "SqlTransaction<{}> connection error: {}", (void *)this, ret.error().message());
        } else {
            ILIAS_TRACE("ilias-sql", "SqlTransaction<{}> connection success", (void *)this);
        }
        mConnection = ret.value_or(nullptr);
    }
    /**
     * @brief Copy constructor (deleted)
     */
    SqlTransaction(const SqlTransaction &)            = delete;
    
    /**
     * @brief Copy assignment operator (deleted)
     */
    SqlTransaction &operator=(const SqlTransaction &) = delete;
    
    /**
     * @brief Move constructor
     * @param other The transaction to move from
     */
    SqlTransaction(SqlTransaction &&other)
        : mDatabase(other.mDatabase), mConnection(other.mConnection), mState(other.mState),
          mOwnsTransaction(other.mOwnsTransaction) {
        ILIAS_TRACE("ilias-sql", "SqlTransaction<{}> move constructor", (void *)this);
        other.mState           = State::kUnused;
        other.mConnection      = nullptr;
        other.mOwnsTransaction = false;
    }
    
    /**
     * @brief Move assignment operator (deleted)
     */
    SqlTransaction &operator=(SqlTransaction &&) = delete;
    
    /**
     * @brief Destructor - automatically rolls back if not committed
     */
    ~SqlTransaction();

    /**
     * @brief Get the underlying connection
     * @return IoResult<IConnection*> The connection or an error
     * @note Returns error if transaction is not in the active state
     */
    auto connection() -> IoResult<IConnection *> {
        if (mState != State::kBeginned) {
            return Err(std::make_error_code(std::errc::operation_not_permitted));
        }
        if (!mConnection) {
            return Err(SqlError::Code::NotConnected);
        }
        return mConnection;
    }

    /**
     * @brief Re-begin the transaction (start a new transaction after commit/rollback)
     * @return IoTask<void> Async task that completes when the transaction is restarted
     */
    auto rebegin() -> IoTask<void>;
    
    /**
     * @brief Commit the transaction
     * @return IoTask<void> Async task that completes when the transaction is committed
     */
    auto commit() -> IoTask<void>;
    
    /**
     * @brief Rollback the transaction
     * @return IoTask<void> Async task that completes when the transaction is rolled back
     */
    auto rollback() -> IoTask<void>;

    auto lastNativeError() const -> std::optional<NativeSqlError> {
        if (!mConnection) {
            return std::nullopt;
        }
        return mConnection->lastNativeError();
    }

protected:
    /**
     * @brief Begin the transaction (internal)
     * @return IoTask<void> Async task that completes when the transaction is started
     */
    auto begin() -> IoTask<void>;

    friend class SqlDatabase;

private:
    SqlDatabase &mDatabase;
    IConnection *mConnection = nullptr;
    State        mState      = State::kUnused;
    bool         mOwnsTransaction = false;
};

ILIAS_SQL_NS_END
