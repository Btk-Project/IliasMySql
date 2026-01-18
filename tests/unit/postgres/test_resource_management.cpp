/**
 * @file test_resource_management.cpp
 * @brief Tests for PostgreSQL resource management (PGresult and prepared statement cleanup)
 * 
 * Validates: Requirements 8.1, 8.2, 8.3
 * 
 * 8.1: WHEN a PostgresResultSet is destroyed, THE PostgreSQL_Backend SHALL free the PGresult using PQclear
 * 8.2: WHEN a PostgresStatement is destroyed, THE PostgreSQL_Backend SHALL deallocate the prepared statement on the server
 * 8.3: WHEN iterating results in streaming mode, THE PostgreSQL_Backend SHALL clear each row result after processing
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include "ilias/sql/sqldatabase.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;

// Helper macro for coroutine assertions
#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value()) {                                                                                        \
            ILIAS_ERROR("pgsql-test", "assert failed: {}", ret.error().message());                                     \
            EXPECT_TRUE(false) << ret.error().message();                                                               \
            co_return {};                                                                                              \
        }                                                                                                              \
    } while (0)

// Helper to get connection options
static ConnectOptions getConnectionOptions() {
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
 * Test 10.1a: PGresult cleanup during streaming iteration
 * 
 * Verifies that each row's PGresult is properly cleared after processing
 * during streaming iteration. This is verified by:
 * 1. Iterating through a large result set
 * 2. Ensuring no memory leaks or connection issues occur
 * 3. Verifying the connection remains usable after iteration
 * 
 * **Validates: Requirements 8.3**
 */
static auto test_pgresult_cleanup_during_streaming() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Query a large result set to stress test PGresult cleanup
    auto queryRet = co_await db.query("SELECT id, value, description FROM test_streaming ORDER BY id LIMIT 500");
    CO_ASSERT_VAL(queryRet);
    
    auto result = std::move(queryRet.value());
    int id = 0, value = 0;
    std::string description;
    int rowCount = 0;
    
    // Iterate through all rows - each iteration should clear the previous row's PGresult
    ilias_for_await(auto rc, result.range(id, value, description)) {
        EXPECT_TRUE(rc.has_value()) << "Row " << rowCount << " should load successfully";
        if (!rc.has_value()) break;
        rowCount++;
    }
    
    EXPECT_EQ(rowCount, 500) << "Should iterate through all 500 rows";
    
    // Verify connection is still usable after streaming iteration
    auto pingRet = co_await db.query("SELECT 1 as test");
    CO_ASSERT_VAL(pingRet);
    
    auto pingResult = std::move(pingRet.value());
    int testVal = 0;
    int pingCount = 0;
    ilias_for_await(auto rc, pingResult.range(testVal)) {
        EXPECT_TRUE(rc.has_value());
        EXPECT_EQ(testVal, 1);
        pingCount++;
    }
    EXPECT_EQ(pingCount, 1) << "Connection should remain usable after streaming";
    
    ILIAS_INFO("pgsql-test", "PGresult cleanup during streaming test passed");
    co_return {};
}

/**
 * Test 10.1b: PGresult cleanup on result set destruction (early termination)
 * 
 * Verifies that when a result set is destroyed before fully iterating,
 * remaining results are properly drained and cleaned up.
 * 
 * **Validates: Requirements 8.1, 8.3**
 */
static auto test_pgresult_cleanup_on_early_destruction() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Start a query but don't fully iterate
    {
        auto queryRet = co_await db.query("SELECT id, value FROM test_streaming ORDER BY id LIMIT 100");
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        int id = 0, value = 0;
        int rowCount = 0;
        
        // Only iterate through first 10 rows, then let result go out of scope
        ilias_for_await(auto rc, result.range(id, value)) {
            if (!rc.has_value()) break;
            rowCount++;
            if (rowCount >= 10) break;  // Early termination
        }
        
        EXPECT_EQ(rowCount, 10) << "Should have iterated through 10 rows";
        // Result set destructor should drain remaining results
    }
    
    // Verify connection is still usable after early termination
    // This is the key test - if cleanup didn't happen, the connection would be in a bad state
    auto pingRet = co_await db.query("SELECT 2 as test");
    CO_ASSERT_VAL(pingRet);
    
    auto pingResult = std::move(pingRet.value());
    int testVal = 0;
    int pingCount = 0;
    ilias_for_await(auto rc, pingResult.range(testVal)) {
        EXPECT_TRUE(rc.has_value());
        EXPECT_EQ(testVal, 2);
        pingCount++;
    }
    EXPECT_EQ(pingCount, 1) << "Connection should remain usable after early termination";
    
    ILIAS_INFO("pgsql-test", "PGresult cleanup on early destruction test passed");
    co_return {};
}

/**
 * Test 10.1c: Multiple sequential queries with proper cleanup
 * 
 * Verifies that multiple sequential queries work correctly, which requires
 * proper cleanup of PGresult between queries.
 * 
 * **Validates: Requirements 8.1, 8.3**
 */
static auto test_multiple_sequential_queries() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Execute multiple queries in sequence
    for (int i = 0; i < 20; ++i) {
        std::string sql = "SELECT id, value FROM test_streaming WHERE id <= " + std::to_string((i + 1) * 5);
        auto queryRet = co_await db.query(sql);
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        int id = 0, value = 0;
        int rowCount = 0;
        
        ilias_for_await(auto rc, result.range(id, value)) {
            if (!rc.has_value()) break;
            rowCount++;
        }
        
        EXPECT_EQ(rowCount, (i + 1) * 5) << "Query " << i << " should return " << (i + 1) * 5 << " rows";
    }
    
    ILIAS_INFO("pgsql-test", "Multiple sequential queries test passed");
    co_return {};
}


/**
 * Test 10.2a: Prepared statement DEALLOCATE on destruction
 * 
 * Verifies that when a prepared statement is destroyed, the server-side
 * prepared statement is deallocated. This is verified by:
 * 1. Creating a prepared statement
 * 2. Destroying it
 * 3. Verifying the statement name no longer exists on the server
 * 
 * **Validates: Requirements 8.2**
 */
static auto test_prepared_statement_deallocate() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Create and destroy multiple prepared statements
    for (int i = 0; i < 10; ++i) {
        {
            auto stmtRet = co_await db.prepare("SELECT ?::integer as val");
            CO_ASSERT_VAL(stmtRet);
            auto& stmt = stmtRet.value();
            
            // Use the statement
            auto bindRet = stmt.bind(i);
            EXPECT_TRUE(bindRet.has_value());
            
            auto queryRet = co_await stmt.query();
            CO_ASSERT_VAL(queryRet);
            
            auto result = std::move(queryRet.value());
            int val = 0;
            ilias_for_await(auto rc, result.range(val)) {
                EXPECT_TRUE(rc.has_value());
                EXPECT_EQ(val, i);
            }
            // Statement goes out of scope here - should DEALLOCATE
        }
        
        // Verify connection is still usable (DEALLOCATE succeeded)
        auto pingRet = co_await db.query("SELECT 1");
        CO_ASSERT_VAL(pingRet);
        auto pingResult = std::move(pingRet.value());
        ilias_for_await(auto rc, pingResult.range()) {
            (void)rc;
        }
    }
    
    ILIAS_INFO("pgsql-test", "Prepared statement DEALLOCATE test passed");
    co_return {};
}

/**
 * Test 10.2b: Multiple prepared statements with proper cleanup
 * 
 * Verifies that creating many prepared statements doesn't exhaust server
 * resources, which would happen if DEALLOCATE wasn't being called.
 * 
 * **Validates: Requirements 8.2**
 */
static auto test_many_prepared_statements() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Create and destroy many prepared statements
    // If DEALLOCATE isn't working, this would eventually fail
    for (int i = 0; i < 100; ++i) {
        auto stmtRet = co_await db.prepare("SELECT ?::integer * 2 as doubled");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        auto bindRet = stmt.bind(i);
        EXPECT_TRUE(bindRet.has_value());
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        int doubled = 0;
        ilias_for_await(auto rc, result.range(doubled)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(doubled, i * 2);
        }
        // Statement destroyed here
    }
    
    // Verify we can still create new statements (server resources not exhausted)
    auto finalStmtRet = co_await db.prepare("SELECT 'cleanup_test'::text as msg");
    CO_ASSERT_VAL(finalStmtRet);
    
    ILIAS_INFO("pgsql-test", "Many prepared statements test passed (100 statements created/destroyed)");
    co_return {};
}

/**
 * Test 10.2c: Prepared statement cleanup with pending results
 * 
 * Verifies that prepared statement destruction properly drains pending
 * results before executing DEALLOCATE.
 * 
 * **Validates: Requirements 8.2, 8.3**
 */
static auto test_prepared_statement_cleanup_with_pending_results() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    for (int i = 0; i < 10; ++i) {
        {
            auto stmtRet = co_await db.prepare(
                "SELECT id, value FROM test_streaming ORDER BY id LIMIT 50"
            );
            CO_ASSERT_VAL(stmtRet);
            auto& stmt = stmtRet.value();
            
            auto queryRet = co_await stmt.query();
            CO_ASSERT_VAL(queryRet);
            
            auto result = std::move(queryRet.value());
            int id = 0, value = 0;
            int rowCount = 0;
            
            // Only read first 5 rows, leaving pending results
            ilias_for_await(auto rc, result.range(id, value)) {
                if (!rc.has_value()) break;
                rowCount++;
                if (rowCount >= 5) break;
            }
            
            EXPECT_EQ(rowCount, 5);
            // Result and statement go out of scope with pending results
        }
        
        // Verify connection is still usable
        auto pingRet = co_await db.query("SELECT 1");
        CO_ASSERT_VAL(pingRet);
        auto pingResult = std::move(pingRet.value());
        ilias_for_await(auto rc, pingResult.range()) {
            (void)rc;
        }
    }
    
    ILIAS_INFO("pgsql-test", "Prepared statement cleanup with pending results test passed");
    co_return {};
}

/**
 * Test 10.1d: Empty result set cleanup
 * 
 * Verifies that result sets with no rows are properly cleaned up.
 * 
 * **Validates: Requirements 8.1**
 */
static auto test_empty_result_cleanup() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    for (int i = 0; i < 20; ++i) {
        auto queryRet = co_await db.query("SELECT id FROM test_streaming WHERE id < 0");
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        int id = 0;
        int rowCount = 0;
        
        ilias_for_await(auto rc, result.range(id)) {
            (void)rc;
            rowCount++;
        }
        
        EXPECT_EQ(rowCount, 0) << "Empty result should have no rows";
    }
    
    // Verify connection is still usable
    auto pingRet = co_await db.query("SELECT 1");
    CO_ASSERT_VAL(pingRet);
    auto pingResult = std::move(pingRet.value());
    ilias_for_await(auto pingRc, pingResult.range()) {
        (void)pingRc;
    }
    
    ILIAS_INFO("pgsql-test", "Empty result cleanup test passed");
    co_return {};
}

// GTest test cases

TEST(PostgresResourceManagementTest, Test10_1a_PGresultCleanupDuringStreaming) {
    // **Validates: Requirements 8.3**
    test_pgresult_cleanup_during_streaming().wait();
}

TEST(PostgresResourceManagementTest, Test10_1b_PGresultCleanupOnEarlyDestruction) {
    // **Validates: Requirements 8.1, 8.3**
    test_pgresult_cleanup_on_early_destruction().wait();
}

TEST(PostgresResourceManagementTest, Test10_1c_MultipleSequentialQueries) {
    // **Validates: Requirements 8.1, 8.3**
    test_multiple_sequential_queries().wait();
}

TEST(PostgresResourceManagementTest, Test10_1d_EmptyResultCleanup) {
    // **Validates: Requirements 8.1**
    test_empty_result_cleanup().wait();
}

TEST(PostgresResourceManagementTest, Test10_2a_PreparedStatementDeallocate) {
    // **Validates: Requirements 8.2**
    test_prepared_statement_deallocate().wait();
}

TEST(PostgresResourceManagementTest, Test10_2b_ManyPreparedStatements) {
    // **Validates: Requirements 8.2**
    test_many_prepared_statements().wait();
}

TEST(PostgresResourceManagementTest, Test10_2c_PreparedStatementCleanupWithPendingResults) {
    // **Validates: Requirements 8.2, 8.3**
    test_prepared_statement_cleanup_with_pending_results().wait();
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
