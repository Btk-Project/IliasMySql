/**
 * @file test_transaction_support.cpp
 * @brief Unit tests for PostgreSQL transaction support
 *
 * Tests transaction isolation and rollback behavior.
 * Validates: Requirements 5.1, 5.2, 5.3, 5.4
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include "ilias/sql/sqldatabase.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ilias;

// Helper macro for async assertions
#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value()) {                                                                                        \
            ILIAS_ERROR("pgsql-test", "assert failed: {}", ret.error().message());                                     \
            EXPECT_TRUE(false) << ret.error().message();                                                               \
            co_return {};                                                                                              \
        }                                                                                                              \
    } while (0)

static auto get_options() -> ConnectOptions {
    ConnectOptions options;
    auto           get_env = [](const char *name, const char *default_val) -> std::string {
        const char *val = std::getenv(name);
        return val ? std::string(val) : std::string(default_val);
    };
    auto get_env_int = [](const char *name, int default_val) -> int {
        const char *val = std::getenv(name);
        return val ? std::atoi(val) : default_val;
    };
    options.host     = get_env("PG_HOST", "localhost");
    options.port     = get_env_int("PG_PORT", 5432);
    options.user     = get_env("PG_USER", "test");
    options.password = get_env("PG_PASS", "test");
    options.database = get_env("PG_NAME", "testdb");
    return options;
}
/**
 * Test: Begin Transaction
 *
 * Validates that beginTransaction() correctly executes BEGIN.
 *
 * **Validates: Requirements 5.1**
 */
static auto test_begin_transaction() -> IoTask<void> {
    ConnectOptions options = get_options();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    auto connRet = db.connection();
    CO_ASSERT_VAL(connRet);
    auto *conn = connRet.value();

    // Begin transaction
    auto beginRet = co_await conn->beginTransaction();
    CO_ASSERT_VAL(beginRet);
    EXPECT_TRUE(beginRet.value()) << "beginTransaction should return true";

    // Rollback to clean up
    auto rollbackRet = co_await conn->rollback();
    CO_ASSERT_VAL(rollbackRet);

    ILIAS_INFO("pgsql-test", "Begin transaction test passed");
    co_return {};
}

/**
 * Test: Commit Transaction
 *
 * Validates that commit() correctly executes COMMIT and persists changes.
 *
 * **Validates: Requirements 5.2**
 */
static auto test_commit_transaction() -> IoTask<void> {
    ConnectOptions options = get_options();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    auto connRet = db.connection();
    CO_ASSERT_VAL(connRet);
    auto *conn = connRet.value();

    // Begin transaction
    auto beginRet = co_await conn->beginTransaction();
    CO_ASSERT_VAL(beginRet);
    EXPECT_TRUE(beginRet.value());

    // Insert a test row
    auto insertRet = co_await db.execute("INSERT INTO test_transactions (balance) VALUES (999.99)");
    CO_ASSERT_VAL(insertRet);

    // Commit transaction
    auto commitRet = co_await conn->commit();
    CO_ASSERT_VAL(commitRet);
    EXPECT_TRUE(commitRet.value()) << "commit should return true";

    // Verify the row was persisted
    auto queryRet = co_await db.query("SELECT balance FROM test_transactions WHERE balance = 999.99");
    CO_ASSERT_VAL(queryRet);

    auto        result = std::move(queryRet.value());
    std::string balance;
    int         count = 0;
    ilias_for_await(auto rc, result.range(balance)) {
        EXPECT_TRUE(rc.has_value());
        count++;
    }
    EXPECT_EQ(count, 1) << "Should find the committed row";

    // Clean up - delete the test row
    auto deleteRet = co_await db.execute("DELETE FROM test_transactions WHERE balance = 999.99");
    CO_ASSERT_VAL(deleteRet);

    ILIAS_INFO("pgsql-test", "Commit transaction test passed");
    co_return {};
}

/**
 * Test: Rollback Transaction
 *
 * Validates that rollback() correctly executes ROLLBACK and discards changes.
 *
 * **Validates: Requirements 5.3**
 */
static auto test_rollback_transaction() -> IoTask<void> {
    ConnectOptions options = get_options();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    auto connRet = db.connection();
    CO_ASSERT_VAL(connRet);
    auto *conn = connRet.value();

    // Begin transaction
    auto beginRet = co_await conn->beginTransaction();
    CO_ASSERT_VAL(beginRet);
    EXPECT_TRUE(beginRet.value());

    // Insert a test row
    auto insertRet = co_await db.execute("INSERT INTO test_transactions (balance) VALUES (888.88)");
    CO_ASSERT_VAL(insertRet);

    // Rollback transaction
    auto rollbackRet = co_await conn->rollback();
    CO_ASSERT_VAL(rollbackRet);
    EXPECT_TRUE(rollbackRet.value()) << "rollback should return true";

    // Verify the row was NOT persisted
    auto queryRet = co_await db.query("SELECT balance FROM test_transactions WHERE balance = 888.88");
    CO_ASSERT_VAL(queryRet);

    auto        result = std::move(queryRet.value());
    std::string balance;
    int         count = 0;
    ilias_for_await(auto rc, result.range(balance)) {
        EXPECT_FALSE(rc.has_value());
        count++;
    }
    EXPECT_EQ(count, 0) << "Should NOT find the rolled back row";

    ILIAS_INFO("pgsql-test", "Rollback transaction test passed");
    co_return {};
}

/**
 * Test: Transaction Isolation
 *
 * Validates that changes made within a transaction are isolated until committed.
 *
 * **Validates: Requirements 5.1, 5.2, 5.3**
 */
static auto test_transaction_isolation() -> IoTask<void> {
    ConnectOptions options = get_options();

    // Open first connection
    auto ret1 = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret1);
    auto db1 = std::move(ret1.value());

    auto connRet1 = db1.connection();
    CO_ASSERT_VAL(connRet1);
    auto *conn1 = connRet1.value();

    // Open second connection
    auto ret2 = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret2);
    auto db2 = std::move(ret2.value());

    // Begin transaction on first connection
    auto beginRet = co_await conn1->beginTransaction();
    CO_ASSERT_VAL(beginRet);
    EXPECT_TRUE(beginRet.value());

    // Insert a row in the transaction
    auto insertRet = co_await db1.execute("INSERT INTO test_transactions (balance) VALUES (777.77)");
    CO_ASSERT_VAL(insertRet);

    // From second connection, the row should NOT be visible (default isolation)
    auto queryRet2 = co_await db2.query("SELECT balance FROM test_transactions WHERE balance = 777.77");
    CO_ASSERT_VAL(queryRet2);

    auto        result2 = std::move(queryRet2.value());
    std::string balance2;
    int         count2 = 0;
    ilias_for_await(auto rc, result2.range(balance2)) {
        EXPECT_TRUE(rc.has_value());
        count2++;
    }
    EXPECT_EQ(count2, 0) << "Uncommitted row should not be visible to other connections";

    // Commit the transaction
    auto commitRet = co_await conn1->commit();
    CO_ASSERT_VAL(commitRet);

    // Now the row should be visible from second connection
    auto queryRet3 = co_await db2.query("SELECT balance FROM test_transactions WHERE balance = 777.77");
    CO_ASSERT_VAL(queryRet3);

    auto        result3 = std::move(queryRet3.value());
    std::string balance3;
    int         count3 = 0;
    ilias_for_await(auto rc, result3.range(balance3)) {
        EXPECT_TRUE(rc.has_value());
        count3++;
    }
    EXPECT_EQ(count3, 1) << "Committed row should be visible to other connections";

    // Clean up
    auto deleteRet = co_await db1.execute("DELETE FROM test_transactions WHERE balance = 777.77");
    CO_ASSERT_VAL(deleteRet);

    ILIAS_INFO("pgsql-test", "Transaction isolation test passed");
    co_return {};
}

/**
 * Test: Sync Rollback
 *
 * Validates that syncRollback() correctly executes ROLLBACK synchronously.
 *
 * **Validates: Requirements 5.4**
 */
static auto test_sync_rollback() -> IoTask<void> {
    ConnectOptions options = get_options();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    auto connRet = db.connection();
    CO_ASSERT_VAL(connRet);
    auto *conn = connRet.value();

    // Begin transaction
    auto beginRet = co_await conn->beginTransaction();
    CO_ASSERT_VAL(beginRet);
    EXPECT_TRUE(beginRet.value());

    // Insert a test row
    auto insertRet = co_await db.execute("INSERT INTO test_transactions (balance) VALUES (666.66)");
    CO_ASSERT_VAL(insertRet);

    // Use syncRollback (synchronous rollback for cleanup scenarios)
    bool syncResult = conn->syncRollback();
    EXPECT_TRUE(syncResult) << "syncRollback should return true";

    // Verify the row was NOT persisted
    auto queryRet = co_await db.query("SELECT balance FROM test_transactions WHERE balance = 666.66");
    CO_ASSERT_VAL(queryRet);

    auto        result = std::move(queryRet.value());
    std::string balance;
    int         count = 0;
    ilias_for_await(auto rc, result.range(balance)) {
        EXPECT_TRUE(rc.has_value());
        count++;
    }
    EXPECT_EQ(count, 0) << "Should NOT find the rolled back row";

    ILIAS_INFO("pgsql-test", "Sync rollback test passed");
    co_return {};
}

/**
 * Test: Multiple Transactions
 *
 * Validates that multiple sequential transactions work correctly.
 *
 * **Validates: Requirements 5.1, 5.2, 5.3**
 */
static auto test_multiple_transactions() -> IoTask<void> {
    ConnectOptions options = get_options();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    auto connRet = db.connection();
    CO_ASSERT_VAL(connRet);
    auto *conn = connRet.value();

    // First transaction - commit
    {
        auto beginRet = co_await conn->beginTransaction();
        CO_ASSERT_VAL(beginRet);

        auto insertRet = co_await db.execute("INSERT INTO test_transactions (balance) VALUES (111.11)");
        CO_ASSERT_VAL(insertRet);

        auto commitRet = co_await conn->commit();
        CO_ASSERT_VAL(commitRet);
    }

    // Second transaction - rollback
    {
        auto beginRet = co_await conn->beginTransaction();
        CO_ASSERT_VAL(beginRet);

        auto insertRet = co_await db.execute("INSERT INTO test_transactions (balance) VALUES (222.22)");
        CO_ASSERT_VAL(insertRet);

        auto rollbackRet = co_await conn->rollback();
        CO_ASSERT_VAL(rollbackRet);
    }

    // Third transaction - commit
    {
        auto beginRet = co_await conn->beginTransaction();
        CO_ASSERT_VAL(beginRet);

        auto insertRet = co_await db.execute("INSERT INTO test_transactions (balance) VALUES (333.33)");
        CO_ASSERT_VAL(insertRet);

        auto commitRet = co_await conn->commit();
        CO_ASSERT_VAL(commitRet);
    }

    // Verify: 111.11 and 333.33 should exist, 222.22 should not
    auto query1 = co_await db.query("SELECT balance FROM test_transactions WHERE balance = 111.11");
    CO_ASSERT_VAL(query1);
    auto        result1 = std::move(query1.value());
    std::string bal1;
    int         count1 = 0;
    ilias_for_await(auto rc, result1.range(bal1)) {
        EXPECT_TRUE(rc.has_value());
        count1++;
    }
    EXPECT_EQ(count1, 1) << "First committed row should exist";

    auto query2 = co_await db.query("SELECT balance FROM test_transactions WHERE balance = 222.22");
    CO_ASSERT_VAL(query2);
    auto        result2 = std::move(query2.value());
    std::string bal2;
    int         count2 = 0;
    ilias_for_await(auto rc, result2.range(bal2)) {
        EXPECT_TRUE(rc.has_value());
        count2++;
    }
    EXPECT_EQ(count2, 0) << "Rolled back row should not exist";

    auto query3 = co_await db.query("SELECT balance FROM test_transactions WHERE balance = 333.33");
    CO_ASSERT_VAL(query3);
    auto        result3 = std::move(query3.value());
    std::string bal3;
    int         count3 = 0;
    ilias_for_await(auto rc, result3.range(bal3)) {
        EXPECT_TRUE(rc.has_value());
        count3++;
    }
    EXPECT_EQ(count3, 1) << "Third committed row should exist";

    // Clean up
    auto deleteRet = co_await db.execute("DELETE FROM test_transactions WHERE balance IN (111.11, 333.33)");
    CO_ASSERT_VAL(deleteRet);

    ILIAS_INFO("pgsql-test", "Multiple transactions test passed");
    co_return {};
}

TEST(PostgresTransactionTest, BeginTransaction) {
    // **Validates: Requirements 5.1**
    test_begin_transaction().wait();
}

TEST(PostgresTransactionTest, CommitTransaction) {
    // **Validates: Requirements 5.2**
    test_commit_transaction().wait();
}

TEST(PostgresTransactionTest, RollbackTransaction) {
    // **Validates: Requirements 5.3**
    test_rollback_transaction().wait();
}

TEST(PostgresTransactionTest, TransactionIsolation) {
    // **Validates: Requirements 5.1, 5.2, 5.3**
    test_transaction_isolation().wait();
}

TEST(PostgresTransactionTest, SyncRollback) {
    // **Validates: Requirements 5.4**
    test_sync_rollback().wait();
}

TEST(PostgresTransactionTest, MultipleTransactions) {
    // **Validates: Requirements 5.1, 5.2, 5.3**
    test_multiple_transactions().wait();
}

int main(int argc, char **argv) {
    cpptrace::init();
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ILIAS_LOG_ADD_WHITELIST("pgsql-test");
    ILIAS_LOG_ADD_WHITELIST("ilias-pgsql");

    ilias::PlatformContext ioContext;
    ioContext.install();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
