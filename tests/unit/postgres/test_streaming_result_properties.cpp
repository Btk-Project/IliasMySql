/**
 * @file test_streaming_result_properties.cpp
 * @brief Property-based tests for PostgreSQL streaming result set
 * 
 * Feature: postgres-backend, Property 1: Streaming Result Iteration
 * Validates: Requirements 1.1, 1.2, 1.3, 1.5
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <random>
#include "ilias/sql/sqldatabase.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;

// 辅助宏
#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value()) {                                                                                        \
            ILIAS_ERROR("pgsql-test", "assert failed: {}", ret.error().message());                                     \
            EXPECT_TRUE(false) << ret.error().message();                                                               \
            co_return {};                                                                                              \
        }                                                                                                              \
    } while (0)

// 基础连接测试
static auto test_basic_connection() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // 简单查询测试
    auto queryRet = co_await db.query("SELECT 1 as test_col");
    CO_ASSERT_VAL(queryRet);
    
    auto result = std::move(queryRet.value());
    int val = 0;
    int count = 0;
    ilias_for_await(auto rc, result.range(val)) {
        EXPECT_TRUE(rc.has_value());
        EXPECT_EQ(val, 1);
        count++;
    }
    EXPECT_EQ(count, 1);
    
    co_return {};
}

/**
 * Property 1: Streaming Result Iteration
 * 
 * *For any* query that returns N rows, iterating through the result should
 * yield exactly N rows with correct values.
 * 
 * **Validates: Requirements 1.1, 1.2, 1.3, 1.5**
 */
static auto test_streaming_iteration_property() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // 使用 test_streaming 表（已在 init.sql 中创建，包含 1000 行）
    auto queryRet = co_await db.query("SELECT id, value, description FROM test_streaming ORDER BY id LIMIT 100");
    CO_ASSERT_VAL(queryRet);
    
    auto result = std::move(queryRet.value());
    
    int id = 0, value = 0;
    std::string description;
    int rowCount = 0;
    
    ilias_for_await(auto rc, result.range(id, value, description)) {
        EXPECT_TRUE(rc.has_value()) << "Row " << rowCount << " should load successfully";
        if (!rc.has_value()) {
            ILIAS_ERROR("pgsql-test", "Failed to load row {}: {}", rowCount, rc.error().message());
            break;
        }
        
        rowCount++;
        
        // Property: id should match row number
        EXPECT_EQ(id, rowCount) << "ID should be sequential";
        
        // Property: value should equal id (as per init.sql)
        EXPECT_EQ(value, rowCount) << "Value should equal ID";
    }
    
    // Property: exactly 100 rows should be returned
    EXPECT_EQ(rowCount, 100) << "Should iterate through exactly 100 rows";
    
    ILIAS_INFO("pgsql-test", "Streaming iteration test passed: {} rows", rowCount);
    co_return {};
}

/**
 * Property 1b: Empty Result Set Handling
 * 
 * *For any* query that returns 0 rows, iteration should yield no rows.
 * 
 * **Validates: Requirements 1.3**
 */
static auto test_empty_result_property() -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Query that returns no rows
    auto queryRet = co_await db.query("SELECT id FROM test_streaming WHERE id < 0");
    CO_ASSERT_VAL(queryRet);
    
    auto result = std::move(queryRet.value());
    
    int id = 0;
    int rowCount = 0;
    
    ilias_for_await(auto rc, result.range(id)) {
        rowCount++;
    }
    
    // Property: no rows should be returned
    EXPECT_EQ(rowCount, 0) << "Empty result should yield no rows";
    
    ILIAS_INFO("pgsql-test", "Empty result test passed");
    co_return {};
}

/**
 * Property 1c: Random Row Count Property Test
 * 
 * *For any* N in [1, 100], querying N rows should return exactly N rows.
 * 
 * **Validates: Requirements 1.1, 1.2, 1.3**
 */
static auto test_random_row_count_property(int expectedRows) -> IoTask<void> {
    ConnectOptions options;
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    
    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Query with LIMIT
    std::string sql = "SELECT id, value FROM test_streaming ORDER BY id LIMIT " + std::to_string(expectedRows);
    auto queryRet = co_await db.query(sql);
    CO_ASSERT_VAL(queryRet);
    
    auto result = std::move(queryRet.value());
    
    int id = 0, value = 0;
    int actualRows = 0;
    
    ilias_for_await(auto rc, result.range(id, value)) {
        if (!rc.has_value()) break;
        actualRows++;
        
        // Property: id should be sequential
        EXPECT_EQ(id, actualRows);
    }
    
    // Property: row count should match expected
    EXPECT_EQ(actualRows, expectedRows) << "Expected " << expectedRows << " rows, got " << actualRows;
    
    co_return {};
}

TEST(PostgresStreamingTest, BasicConnection) {
    test_basic_connection().wait();
}

TEST(PostgresStreamingTest, Property1_StreamingIteration) {
    // Feature: postgres-backend, Property 1: Streaming Result Iteration
    // **Validates: Requirements 1.1, 1.2, 1.3, 1.5**
    test_streaming_iteration_property().wait();
}

TEST(PostgresStreamingTest, Property1b_EmptyResultSet) {
    // Feature: postgres-backend, Property 1b: Empty Result Set Handling
    // **Validates: Requirements 1.3**
    test_empty_result_property().wait();
}

TEST(PostgresStreamingTest, Property1c_RandomRowCount) {
    // Feature: postgres-backend, Property 1c: Random Row Count
    // **Validates: Requirements 1.1, 1.2, 1.3**
    
    // Run property test with random row counts
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(1, 100);
    
    for (int i = 0; i < 10; ++i) {
        int rowCount = dist(gen);
        ILIAS_INFO("pgsql-test", "Testing with {} rows (iteration {})", rowCount, i + 1);
        test_random_row_count_property(rowCount).wait();
    }
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
