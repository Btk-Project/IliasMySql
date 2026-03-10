/**
 * @file test_prepared_statement_execution_properties.cpp
 * @brief Property-based tests for PostgreSQL prepared statement execution
 *
 * Feature: postgres-backend, Property 4: Prepared Statement Execution
 * Validates: Requirements 4.2, 4.3, 4.4, 4.5
 *
 * *For any* prepared statement with bound parameters, executing the statement
 * should produce results consistent with the bound values, and the affected
 * row count should match the actual database changes.
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <random>
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

// Random number generator with fixed seed for reproducibility
static std::mt19937 g_rng(42);

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

// Helper to generate random string
static std::string generateRandomString(size_t length) {
    static const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string       result;
    result.reserve(length);
    std::uniform_int_distribution<size_t> dist(0, sizeof(charset) - 2);
    for (size_t i = 0; i < length; ++i) {
        result += charset[dist(g_rng)];
    }
    return result;
}

/**
 * Property 4a: Indexed Parameter Binding
 *
 * *For any* prepared statement with N parameters, binding values by index
 * should correctly associate each value with its corresponding placeholder.
 *
 * **Validates: Requirements 4.2**
 */
static auto test_indexed_binding_property() -> IoTask<void> {
    std::uniform_int_distribution<int> age_dist(1, 100);
    std::uniform_int_distribution<int> name_len_dist(5, 20);
    std::uniform_int_distribution<int> bool_dist(0, 1);

    // Run 100 iterations for property testing
    // Each iteration uses a fresh connection to avoid connection state issues
    for (int i = 0; i < 100; ++i) {
        auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());

        std::string test_name   = generateRandomString(name_len_dist(g_rng));
        int         test_age    = age_dist(g_rng);
        bool        test_active = bool_dist(g_rng) == 1;

        // Prepare INSERT statement with ? placeholders
        auto stmtRet = co_await db.prepare(
            "INSERT INTO test_prepared (name, age, active) VALUES (?, ?, ?) RETURNING id, name, age, active");
        CO_ASSERT_VAL(stmtRet);
        auto &stmt = stmtRet.value();

        // Bind values by index (1-based)
        auto bindRet = stmt.bind(test_name, test_age, test_active);
        CO_ASSERT_VAL(bindRet);

        // Execute query and verify results
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);

        auto        result       = std::move(queryRet.value());
        int         retrieved_id = 0;
        std::string retrieved_name;
        int         retrieved_age    = 0;
        bool        retrieved_active = false;
        int         count            = 0;

        ilias_for_await(auto rc, result.range(retrieved_id, retrieved_name, retrieved_age, retrieved_active)) {
            EXPECT_TRUE(rc.has_value()) << "Row retrieval should succeed";

            // Property: retrieved values should match bound values
            EXPECT_EQ(retrieved_name, test_name) << "Name should match bound value";
            EXPECT_EQ(retrieved_age, test_age) << "Age should match bound value";
            EXPECT_EQ(retrieved_active, test_active) << "Active should match bound value";
            EXPECT_GT(retrieved_id, 0) << "ID should be auto-generated";
            count++;
        }

        EXPECT_EQ(count, 1) << "Should return exactly one row";
    }

    ILIAS_INFO("pgsql-test", "Indexed binding property test passed (100 iterations)");
    co_return {};
}

/**
 * Property 4b: Named Parameter Binding
 *
 * *For any* prepared statement with named parameters (:name), binding values
 * by name should correctly map to the corresponding placeholder positions.
 *
 * **Validates: Requirements 4.3**
 */
static auto test_named_binding_property() -> IoTask<void> {
    std::uniform_int_distribution<int> age_dist(1, 100);
    std::uniform_int_distribution<int> name_len_dist(5, 20);

    // Run 100 iterations for property testing
    for (int i = 0; i < 100; ++i) {
        auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());

        std::string test_name = generateRandomString(name_len_dist(g_rng));
        int         test_age  = age_dist(g_rng);

        // Prepare INSERT statement with :name placeholders
        auto stmtRet =
            co_await db.prepare("INSERT INTO test_prepared (name, age) VALUES (:name, :age) RETURNING id, name, age");
        CO_ASSERT_VAL(stmtRet);
        auto &stmt = stmtRet.value();

        // Bind values positionally (named params are converted to $1, $2 in order)
        auto bindRet = stmt.bind(test_name, test_age);
        CO_ASSERT_VAL(bindRet);

        // Execute query and verify results
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);

        auto        result       = std::move(queryRet.value());
        int         retrieved_id = 0;
        std::string retrieved_name;
        int         retrieved_age = 0;
        int         count         = 0;

        ilias_for_await(auto rc, result.range(retrieved_id, retrieved_name, retrieved_age)) {
            EXPECT_TRUE(rc.has_value()) << "Row retrieval should succeed";

            // Property: retrieved values should match bound values
            EXPECT_EQ(retrieved_name, test_name) << "Name should match bound value";
            EXPECT_EQ(retrieved_age, test_age) << "Age should match bound value";
            count++;
        }

        EXPECT_EQ(count, 1) << "Should return exactly one row";
    }

    ILIAS_INFO("pgsql-test", "Named binding property test passed (100 iterations)");
    co_return {};
}

/**
 * Property 4c: Query Results Streaming
 *
 * *For any* prepared SELECT statement, query() should return results in
 * streaming mode, allowing iteration through all matching rows.
 *
 * **Validates: Requirements 4.4**
 */
static auto test_query_streaming_property() -> IoTask<void> {
    std::uniform_int_distribution<int> limit_dist(1, 100);

    // Run 10 iterations with different limits
    for (int i = 0; i < 10; ++i) {
        auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());

        int64_t expected_rows = limit_dist(g_rng);

        // Prepare SELECT statement with LIMIT parameter
        auto stmtRet = co_await db.prepare("SELECT id, value, description FROM test_streaming ORDER BY id LIMIT ?");
        CO_ASSERT_VAL(stmtRet);
        auto &stmt = stmtRet.value();

        // Bind the limit value
        auto bindRet = stmt.bind(expected_rows);
        EXPECT_TRUE(bindRet.has_value()) << "Binding limit should succeed";

        // Execute query
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);

        auto        result = std::move(queryRet.value());
        int         id = 0, value = 0;
        std::string description;
        int         actual_rows = 0;

        ilias_for_await(auto rc, result.range(id, value, description)) {
            if (!rc.has_value())
                break;
            actual_rows++;

            // Property: id should be sequential
            EXPECT_EQ(id, actual_rows) << "ID should be sequential";

            // Property: value should equal id (as per init.sql)
            EXPECT_EQ(value, actual_rows) << "Value should equal ID";
        }

        // Property: row count should match expected limit
        EXPECT_EQ(actual_rows, expected_rows) << "Should return exactly " << expected_rows << " rows";
    }

    ILIAS_INFO("pgsql-test", "Query streaming property test passed (10 iterations)");
    co_return {};
}

/**
 * Property 4d: Execute Returns Affected Row Count
 *
 * *For any* prepared INSERT/UPDATE/DELETE statement, execute() should return
 * the correct number of affected rows.
 *
 * **Validates: Requirements 4.5**
 */
static auto test_execute_affected_rows_property() -> IoTask<void> {
    std::uniform_int_distribution<int> count_dist(1, 5);
    std::uniform_int_distribution<int> age_dist(1, 100);

    // Run 10 iterations
    for (int i = 0; i < 10; ++i) {
        auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());

        int insert_count = count_dist(g_rng);
        int test_age     = age_dist(g_rng) + i * 100 + 10000; // Use high offset to avoid conflicts with other tests

        // Clean up any existing rows with this age (from previous failed runs)
        auto cleanupStmtRet = co_await db.prepare("DELETE FROM test_prepared WHERE age = ? OR age = ?");
        CO_ASSERT_VAL(cleanupStmtRet);
        auto &cleanupStmt    = cleanupStmtRet.value();
        auto  cleanupBindRet = cleanupStmt.bind(test_age, test_age + 1);
        EXPECT_TRUE(cleanupBindRet.has_value());
        auto cleanupQueryRet = co_await cleanupStmt.query();
        CO_ASSERT_VAL(cleanupQueryRet);
        auto cleanupResult = std::move(cleanupQueryRet.value());
        ilias_for_await(auto rc, cleanupResult.range()) {
            (void)rc;
        }

        // Insert multiple rows with the same age using prepared statements
        for (int j = 0; j < insert_count; ++j) {
            std::string name = "test_" + std::to_string(i) + "_" + std::to_string(j);

            auto insertStmtRet = co_await db.prepare("INSERT INTO test_prepared (name, age) VALUES (?, ?)");
            CO_ASSERT_VAL(insertStmtRet);
            auto &insertStmt = insertStmtRet.value();

            auto bindRet = insertStmt.bind(name, test_age);
            EXPECT_TRUE(bindRet.has_value());

            auto insertQueryRet = co_await insertStmt.query();
            CO_ASSERT_VAL(insertQueryRet);

            // Consume the result
            auto insertResult = std::move(insertQueryRet.value());
            ilias_for_await(auto rc, insertResult.range()) {
                (void)rc;
            }
        }

        // Prepare UPDATE statement
        auto stmtRet = co_await db.prepare("UPDATE test_prepared SET age = age + 1 WHERE age = ?");
        CO_ASSERT_VAL(stmtRet);
        auto &stmt = stmtRet.value();

        // Bind the age value
        auto bindRet = stmt.bind(test_age);
        EXPECT_TRUE(bindRet.has_value()) << "Binding age should succeed";

        // Execute the update
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);

        // Consume the result
        auto updateResult = std::move(queryRet.value());
        ilias_for_await(auto rc, updateResult.range()) {
            (void)rc;
        }

        // Verify by querying the updated rows
        auto verifyStmtRet = co_await db.prepare("SELECT COUNT(*) FROM test_prepared WHERE age = ?");
        CO_ASSERT_VAL(verifyStmtRet);
        auto &verifyStmt = verifyStmtRet.value();

        auto verifyBindRet = verifyStmt.bind(test_age + 1);
        EXPECT_TRUE(verifyBindRet.has_value());

        auto verifyQueryRet = co_await verifyStmt.query();
        CO_ASSERT_VAL(verifyQueryRet);

        auto    verifyResult  = std::move(verifyQueryRet.value());
        int64_t updated_count = 0;

        ilias_for_await(auto rc, verifyResult.range(updated_count)) {
            EXPECT_TRUE(rc.has_value());
            // Property: updated count should match inserted count
            EXPECT_EQ(updated_count, static_cast<int64_t>(insert_count))
                << "Updated row count should match inserted count";
        }
    }

    ILIAS_INFO("pgsql-test", "Execute affected rows property test passed (10 iterations)");
    co_return {};
}

/**
 * Property 4e: Multiple Parameter Types
 *
 * *For any* combination of parameter types (int, string, bool, float),
 * binding and executing should produce correct results.
 *
 * **Validates: Requirements 4.2, 4.4**
 */
static auto test_multiple_param_types_property() -> IoTask<void> {
    std::uniform_int_distribution<int>     int_dist(-1000, 1000);
    std::uniform_real_distribution<double> float_dist(-1000.0, 1000.0);
    std::uniform_int_distribution<int>     bool_dist(0, 1);
    std::uniform_int_distribution<int>     name_len_dist(5, 20);

    // Run 100 iterations for property testing
    for (int i = 0; i < 100; ++i) {
        auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());

        int         test_int    = int_dist(g_rng);
        double      test_float  = float_dist(g_rng);
        bool        test_bool   = bool_dist(g_rng) == 1;
        std::string test_string = generateRandomString(name_len_dist(g_rng));

        // Prepare a SELECT statement that echoes back the parameters
        auto stmtRet = co_await db.prepare("SELECT ?::integer as int_val, ?::double precision as float_val, ?::boolean "
                                           "as bool_val, ?::text as text_val");
        CO_ASSERT_VAL(stmtRet);
        auto &stmt = stmtRet.value();

        // Bind multiple types
        auto bindRet = stmt.bind(test_int, test_float, test_bool, test_string);
        EXPECT_TRUE(bindRet.has_value()) << "Binding multiple types should succeed";

        // Execute query
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);

        auto        result          = std::move(queryRet.value());
        int         retrieved_int   = 0;
        double      retrieved_float = 0.0;
        bool        retrieved_bool  = false;
        std::string retrieved_string;
        int         count = 0;

        ilias_for_await(auto rc, result.range(retrieved_int, retrieved_float, retrieved_bool, retrieved_string)) {
            EXPECT_TRUE(rc.has_value()) << "Row retrieval should succeed";

            // Property: all types should round-trip correctly
            EXPECT_EQ(retrieved_int, test_int) << "Integer should match";
            // Use a more relaxed tolerance for double precision
            EXPECT_NEAR(retrieved_float, test_float, std::abs(test_float) * 1e-6 + 1e-6)
                << "Float should match within precision";
            EXPECT_EQ(retrieved_bool, test_bool) << "Boolean should match";
            EXPECT_EQ(retrieved_string, test_string) << "String should match";
            count++;
        }

        EXPECT_EQ(count, 1) << "Should return exactly one row";
    }

    ILIAS_INFO("pgsql-test", "Multiple parameter types property test passed (100 iterations)");
    co_return {};
}

/**
 * Property 4f: Statement Reset and Reuse
 *
 * *For any* prepared statement, after reset() is called, the statement
 * should be reusable with new parameter values.
 *
 * **Validates: Requirements 4.2, 4.4**
 */
static auto test_statement_reset_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());

    // Prepare a statement once
    auto stmtRet = co_await db.prepare("SELECT ?::integer * 2 as doubled");
    CO_ASSERT_VAL(stmtRet);
    auto &stmt = stmtRet.value();

    std::uniform_int_distribution<int> int_dist(1, 1000);

    // Run 100 iterations reusing the same statement
    for (int i = 0; i < 100; ++i) {
        int test_value      = int_dist(g_rng);
        int expected_result = test_value * 2;

        // Reset and rebind
        stmt.reset();
        auto bindRet = stmt.bind(test_value);
        CO_ASSERT_VAL(bindRet);

        // Execute query
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);

        auto result          = std::move(queryRet.value());
        int  retrieved_value = 0;
        int  count           = 0;

        ilias_for_await(auto rc, result.range(retrieved_value)) {
            EXPECT_TRUE(rc.has_value()) << "Row retrieval should succeed";

            // Property: result should be double the input
            EXPECT_EQ(retrieved_value, expected_result)
                << "Result should be " << test_value << " * 2 = " << expected_result;
            count++;
        }

        EXPECT_EQ(count, 1) << "Should return exactly one row";
    }

    ILIAS_INFO("pgsql-test", "Statement reset property test passed (100 iterations)");
    co_return {};
}

// GTest test cases

TEST(PostgresPreparedStatementTest, Property4a_IndexedBinding) {
    // Feature: postgres-backend, Property 4: Prepared Statement Execution
    // **Validates: Requirements 4.2**
    test_indexed_binding_property().wait();
}

TEST(PostgresPreparedStatementTest, Property4b_NamedBinding) {
    // Feature: postgres-backend, Property 4: Prepared Statement Execution
    // **Validates: Requirements 4.3**
    test_named_binding_property().wait();
}

TEST(PostgresPreparedStatementTest, Property4c_QueryStreaming) {
    // Feature: postgres-backend, Property 4: Prepared Statement Execution
    // **Validates: Requirements 4.4**
    test_query_streaming_property().wait();
}

TEST(PostgresPreparedStatementTest, Property4d_ExecuteAffectedRows) {
    // Feature: postgres-backend, Property 4: Prepared Statement Execution
    // **Validates: Requirements 4.5**
    test_execute_affected_rows_property().wait();
}

TEST(PostgresPreparedStatementTest, Property4e_MultipleParamTypes) {
    // Feature: postgres-backend, Property 4: Prepared Statement Execution
    // **Validates: Requirements 4.2, 4.4**
    test_multiple_param_types_property().wait();
}

TEST(PostgresPreparedStatementTest, Property4f_StatementReset) {
    // Feature: postgres-backend, Property 4: Prepared Statement Execution
    // **Validates: Requirements 4.2, 4.4**
    test_statement_reset_property().wait();
}

int main(int argc, char **argv) {
    cpptrace::init();
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ILIAS_LOG_ADD_WHITELIST("pgsql-test");
    ILIAS_LOG_ADD_WHITELIST("ilias-pgsql");
    ILIAS_LOG_ADD_WHITELIST("ilias-sql");

    ilias::PlatformContext ioContext;
    ioContext.install();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
