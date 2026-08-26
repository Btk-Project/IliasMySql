/**
 * @file test_connection_management.cpp
 * @brief Unit tests for PostgreSQL connection management
 * 
 * Tests connection establishment, disconnection, and ping functionality.
 * Validates: Requirements 3.1, 3.2, 3.4
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

/**
 * Test: Connection Establishment
 * 
 * Validates that a connection can be established to PostgreSQL using
 * non-blocking mode.
 * 
 * **Validates: Requirements 3.1**
 */
static auto test_connection_establishment() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Verify connection is working by executing a simple query
    auto queryRet = co_await db.query("SELECT 1");
    CO_ASSERT_VAL(queryRet);
    
    ILIAS_INFO("pgsql-test", "Connection establishment test passed");
    co_return {};
}

/**
 * Test: Connection Disconnection
 * 
 * Validates that a connection can be properly disconnected and resources
 * are cleaned up.
 * 
 * **Validates: Requirements 3.2, 8.4**
 */
static auto test_connection_disconnection() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    // Open connection
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Execute a query to ensure connection is active
    auto queryRet = co_await db.query("SELECT 1");
    CO_ASSERT_VAL(queryRet);
    
    // Close the connection (SqlDatabase uses close() instead of disconnect())
    auto closeRet = co_await db.close();
    CO_ASSERT_VAL(closeRet);
    
    ILIAS_INFO("pgsql-test", "Connection disconnection test passed");
    co_return {};
}

/**
 * Test: Ping Functionality
 * 
 * Validates that ping() correctly tests the connection by sending a query.
 * 
 * **Validates: Requirements 3.4**
 */
static auto test_ping_functionality() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Get the underlying connection to test ping
    auto connRet = db.connection();
    CO_ASSERT_VAL(connRet);
    auto* conn = connRet.value();
    
    // Test ping on active connection
    auto pingRet = co_await conn->ping();
    CO_ASSERT_VAL(pingRet);
    EXPECT_TRUE(pingRet.value()) << "Ping should return true for active connection";
    
    ILIAS_INFO("pgsql-test", "Ping functionality test passed");
    co_return {};
}

/**
 * Test: Multiple Sequential Connections
 * 
 * Validates that multiple connections can be established and disconnected
 * sequentially without resource leaks.
 * 
 * **Validates: Requirements 3.1, 3.2**
 */
static auto test_multiple_sequential_connections() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    for (int i = 0; i < 5; ++i) {
        auto ret = co_await SqlDatabase::open("postgres", options);
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());
        
        // Execute a query
        auto queryRet = co_await db.query("SELECT " + std::to_string(i));
        CO_ASSERT_VAL(queryRet);
        
        // Close the connection
        auto closeRet = co_await db.close();
        CO_ASSERT_VAL(closeRet);
        
        ILIAS_INFO("pgsql-test", "Sequential connection {} completed", i + 1);
    }
    
    ILIAS_INFO("pgsql-test", "Multiple sequential connections test passed");
    co_return {};
}

/**
 * Test: Connection with Pending Results Cleanup
 * 
 * Validates that disconnecting while there are pending results properly
 * drains and cleans up resources.
 * 
 * **Validates: Requirements 3.2, 8.4**
 */
static auto test_disconnect_with_pending_results() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Start a query but don't fully consume the results
    auto queryRet = co_await db.query("SELECT id FROM test_streaming LIMIT 10");
    CO_ASSERT_VAL(queryRet);
    
    // Don't iterate through all results - just close
    // The close should properly drain pending results
    auto closeRet = co_await db.close();
    CO_ASSERT_VAL(closeRet);
    
    ILIAS_INFO("pgsql-test", "Disconnect with pending results test passed");
    co_return {};
}

TEST(PostgresConnectionTest, ConnectionEstablishment) {
    // **Validates: Requirements 3.1**
    test_connection_establishment().wait();
}

TEST(PostgresConnectionTest, ConnectionDisconnection) {
    // **Validates: Requirements 3.2, 8.4**
    test_connection_disconnection().wait();
}

TEST(PostgresConnectionTest, PingFunctionality) {
    // **Validates: Requirements 3.4**
    test_ping_functionality().wait();
}

TEST(PostgresConnectionTest, MultipleSequentialConnections) {
    // **Validates: Requirements 3.1, 3.2**
    test_multiple_sequential_connections().wait();
}

TEST(PostgresConnectionTest, DisconnectWithPendingResults) {
    // **Validates: Requirements 3.2, 8.4**
    test_disconnect_with_pending_results().wait();
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
