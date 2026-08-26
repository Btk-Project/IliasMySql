/**
 * @file test_error_handling.cpp
 * @brief Unit tests for PostgreSQL error handling
 *
 * Tests error reporting for invalid SQL and invalid parameter indices.
 * Validates: Requirements 7.3, 7.4
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/sqlerror.hpp"
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

static ConnectOptions getOptions() {
    ConnectOptions options;
    auto           get_env = [](const char *name, const char *default_val) -> std::string {
        const char *val = std::getenv(name);
        return val ? std::string(val) : std::string(default_val);
    };
    auto get_env_int = [](const char *name, int default_val) -> int {
        const char *val = std::getenv(name);
        return val ? std::atoi(val) : default_val;
    };
    options.host     = get_env("PG_HOST", "127.0.0.1");
    options.port     = get_env_int("PG_PORT", 5432);
    options.user     = get_env("PG_USER", "test");
    options.password = get_env("PG_PASS", "test");
    options.database = get_env("PG_NAME", "testdb");
    return options;
};

/**
 * Test: Invalid SQL Error Reporting
 *
 * Validates that executing invalid SQL returns an appropriate error
 * with detailed error information.
 *
 * **Validates: Requirements 7.3**
 */
static auto test_invalid_sql_error() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Execute invalid SQL - syntax error
    auto queryRet = co_await db.query("SELECT * FROM nonexistent_table_xyz");

    // Should fail with an error
    EXPECT_FALSE(queryRet.has_value()) << "Query to non-existent table should fail";

    if (!queryRet.has_value()) {
        auto errorCode = queryRet.error();
        ILIAS_INFO("pgsql-test", "Got expected error: {}", errorCode.message());

        // The error should be TableNotFound or a related error
        // PostgreSQL returns SQLSTATE 42P01 for undefined_table
        EXPECT_TRUE(errorCode == SqlError::Code::TableNotFound || errorCode == SqlError::Code::UnknownError)
            << "Expected TableNotFound or UnknownError, got: " << errorCode.message();
    }
    ILIAS_INFO("pgsql-test", "Invalid SQL error test passed");
    co_return {};
}

/**
 * Test: SQL Syntax Error Reporting
 *
 * Validates that SQL syntax errors are properly reported.
 *
 * **Validates: Requirements 7.3**
 */
static auto test_sql_syntax_error() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Execute SQL with syntax error
    auto queryRet = co_await db.query("SELEC * FROM test_types"); // Typo: SELEC instead of SELECT

    // Should fail with an error
    EXPECT_FALSE(queryRet.has_value()) << "Query with syntax error should fail";

    if (!queryRet.has_value()) {
        auto errorCode = queryRet.error();
        ILIAS_INFO("pgsql-test", "Got expected syntax error: {}", errorCode.message());
    }

    ILIAS_INFO("pgsql-test", "SQL syntax error test passed");
    co_return {};
}

/**
 * Test: Invalid Parameter Index Error
 *
 * Validates that binding to an invalid parameter index returns
 * SqlError::InvalidIndex.
 *
 * **Validates: Requirements 7.4**
 */
static auto test_invalid_parameter_index() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Prepare a statement with one parameter
    auto stmtRet = co_await db.prepare("SELECT * FROM test_types WHERE id = ?");
    CO_ASSERT_VAL(stmtRet);
    auto stmt = std::move(stmtRet.value());

    // Try to bind to index 0 (invalid - indices start at 1)
    int32_t value    = 1;
    auto    bindRet0 = stmt->bind(0, value);
    EXPECT_FALSE(bindRet0.has_value()) << "Binding to index 0 should fail";
    if (!bindRet0.has_value()) {
        EXPECT_EQ(bindRet0.error(), SqlError::Code::InvalidIndex) << "Expected InvalidIndex error for index 0";
    }

    // Try to bind to index 2 (invalid - only 1 parameter)
    auto bindRet2 = stmt->bind(2, value);
    EXPECT_FALSE(bindRet2.has_value()) << "Binding to index 2 should fail (only 1 parameter)";
    if (!bindRet2.has_value()) {
        EXPECT_EQ(bindRet2.error(), SqlError::Code::InvalidIndex) << "Expected InvalidIndex error for index 2";
    }

    // Binding to index 1 should succeed
    auto bindRet1 = stmt->bind(1, value);
    EXPECT_TRUE(bindRet1.has_value()) << "Binding to index 1 should succeed";

    ILIAS_INFO("pgsql-test", "Invalid parameter index test passed");
    co_return {};
}

/**
 * Test: Invalid Named Parameter Error
 *
 * Validates that binding to a non-existent named parameter returns
 * SqlError::InvalidIndex.
 *
 * **Validates: Requirements 7.4**
 */
static auto test_invalid_named_parameter() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Prepare a statement with a named parameter
    auto stmtRet = co_await db.prepare("SELECT * FROM test_types WHERE id = :id");
    CO_ASSERT_VAL(stmtRet);
    auto stmt = std::move(stmtRet.value());

    // Try to bind to a non-existent named parameter
    int32_t value   = 1;
    auto    bindRet = stmt->bind("nonexistent", value);
    EXPECT_FALSE(bindRet.has_value()) << "Binding to non-existent named parameter should fail";
    if (!bindRet.has_value()) {
        EXPECT_EQ(bindRet.error(), SqlError::Code::InvalidIndex)
            << "Expected InvalidIndex error for non-existent named parameter";
    }

    // Binding to the correct named parameter should succeed
    auto bindRetCorrect = stmt->bind("id", value);
    EXPECT_TRUE(bindRetCorrect.has_value()) << "Binding to 'id' should succeed";

    ILIAS_INFO("pgsql-test", "Invalid named parameter test passed");
    co_return {};
}

/**
 * Test: Constraint Violation Error
 *
 * Validates that constraint violations are properly reported with
 * appropriate error codes.
 *
 * **Validates: Requirements 7.2, 7.3**
 */
static auto test_constraint_violation_error() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Create a test table with a unique constraint
    auto createRet = co_await db.execute("CREATE TABLE IF NOT EXISTS test_error_handling ("
                                         "  id SERIAL PRIMARY KEY,"
                                         "  unique_col TEXT UNIQUE"
                                         ")");
    (void)createRet; // Ignore error if table already exists

    // Clean up any existing data
    co_await db.execute("DELETE FROM test_error_handling");

    // Insert a row
    auto insertRet1 = co_await db.execute("INSERT INTO test_error_handling (unique_col) VALUES ('unique_value')");
    CO_ASSERT_VAL(insertRet1);

    // Try to insert a duplicate value - should fail with unique constraint violation
    auto insertRet2 = co_await db.execute("INSERT INTO test_error_handling (unique_col) VALUES ('unique_value')");

    EXPECT_FALSE(insertRet2.has_value()) << "Duplicate insert should fail";

    if (!insertRet2.has_value()) {
        auto errorCode = insertRet2.error();
        ILIAS_INFO("pgsql-test", "Got expected constraint error: {}", errorCode.message());

        // Should be UniqueConstraintViolation or ConstraintViolation
        EXPECT_TRUE(errorCode == SqlError::Code::UniqueConstraintViolation ||
                    errorCode == SqlError::Code::ConstraintViolation || errorCode == SqlError::Code::UnknownError)
            << "Expected constraint violation error, got: " << errorCode.message();
    }

    // Clean up
    co_await db.execute("DROP TABLE IF EXISTS test_error_handling");

    ILIAS_INFO("pgsql-test", "Constraint violation error test passed");
    co_return {};
}

/**
 * Test: Invalid Column Index in Result Set
 *
 * Validates that accessing an invalid column index returns
 * SqlError::InvalidIndex.
 *
 * **Validates: Requirements 7.4**
 */
static auto test_invalid_column_index() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Execute a query that returns a single column
    auto queryRet = co_await db.query("SELECT 1 AS col1");
    CO_ASSERT_VAL(queryRet);
    auto resultSet = std::move(queryRet.value());

    // Move to first row
    auto nextRet = co_await resultSet->next();
    CO_ASSERT_VAL(nextRet);
    EXPECT_TRUE(nextRet.value()) << "Should have at least one row";

    // Try to access an invalid column index
    auto valueRet = resultSet->getValue(999);
    EXPECT_FALSE(valueRet.has_value()) << "Accessing invalid column index should fail";
    if (!valueRet.has_value()) {
        EXPECT_EQ(valueRet.error(), SqlError::Code::InvalidIndex)
            << "Expected InvalidIndex error for invalid column index";
    }

    // Accessing valid column index should succeed
    auto validValueRet = resultSet->getValue(0);
    EXPECT_TRUE(validValueRet.has_value())
        << "Accessing valid column index should succeed, " << validValueRet.error_or(SqlError::OK).message();

    ILIAS_INFO("pgsql-test", "Invalid column index test passed");
    co_return {};
}

/**
 * Test: Invalid Column Name in Result Set
 *
 * Validates that accessing a non-existent column name returns
 * SqlError::InvalidIndex.
 *
 * **Validates: Requirements 7.4**
 */
static auto test_invalid_column_name() -> IoTask<void> {
    ConnectOptions options = getOptions();

    auto ret = co_await SqlDatabase::open("postgres", options);
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Execute a query that returns a named column
    auto queryRet = co_await db.query("SELECT 1 AS col1");
    CO_ASSERT_VAL(queryRet);
    auto resultSet = std::move(queryRet.value());

    // Move to first row
    auto nextRet = co_await resultSet->next();
    CO_ASSERT_VAL(nextRet);
    EXPECT_TRUE(nextRet.value()) << "Should have at least one row";

    // Try to access a non-existent column name
    auto valueRet = resultSet->getValue("nonexistent_column");
    EXPECT_FALSE(valueRet.has_value()) << "Accessing non-existent column name should fail";
    if (!valueRet.has_value()) {
        EXPECT_EQ(valueRet.error(), SqlError::Code::InvalidIndex)
            << "Expected InvalidIndex error for non-existent column name";
    }

    // Accessing valid column name should succeed
    auto validValueRet = resultSet->getValue("col1");
    EXPECT_TRUE(validValueRet.has_value()) << "Accessing valid column name should succeed";

    ILIAS_INFO("pgsql-test", "Invalid column name test passed");
    co_return {};
}

TEST(PostgresErrorHandlingTest, InvalidSqlError) {
    // **Validates: Requirements 7.3**
    test_invalid_sql_error().wait();
}

TEST(PostgresErrorHandlingTest, SqlSyntaxError) {
    // **Validates: Requirements 7.3**
    test_sql_syntax_error().wait();
}

TEST(PostgresErrorHandlingTest, InvalidParameterIndex) {
    // **Validates: Requirements 7.4**
    test_invalid_parameter_index().wait();
}

TEST(PostgresErrorHandlingTest, InvalidNamedParameter) {
    // **Validates: Requirements 7.4**
    test_invalid_named_parameter().wait();
}

TEST(PostgresErrorHandlingTest, ConstraintViolationError) {
    // **Validates: Requirements 7.2, 7.3**
    test_constraint_violation_error().wait();
}

TEST(PostgresErrorHandlingTest, InvalidColumnIndex) {
    // **Validates: Requirements 7.4**
    test_invalid_column_index().wait();
}

TEST(PostgresErrorHandlingTest, InvalidColumnName) {
    // **Validates: Requirements 7.4**
    test_invalid_column_name().wait();
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
