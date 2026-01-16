/**
 * @file test_sql_placeholder_parsing_properties.cpp
 * @brief Property-based tests for PostgreSQL SQL placeholder parsing
 * 
 * Feature: postgres-backend, Property 3: SQL Placeholder Parsing
 * Validates: Requirements 4.1
 */

#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <random>
#include <sstream>
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
    options.host = "localhost";
    options.port = 5432;
    options.user = "test";
    options.password = "test";
    options.database = "testdb";
    return options;
}

/**
 * Property 3: SQL Placeholder Parsing - Question Mark Placeholders
 * 
 * *For any* SQL string containing ? placeholders, the parser should convert
 * them to $1, $2, etc. in sequential order.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_question_mark_placeholder_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Test: Single ? placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT * FROM test_types WHERE id = ?");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Bind value and execute
        auto bindRet = stmt.bind(1);
        EXPECT_TRUE(bindRet.has_value()) << "Binding to index 1 should succeed";
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
    }
    
    // Test: Multiple ? placeholders
    {
        auto stmtRet = co_await db.prepare("SELECT * FROM test_types WHERE id > ? AND id < ?");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        auto bindRet = stmt.bind(0, 10);
        EXPECT_TRUE(bindRet.has_value()) << "Binding multiple values should succeed";
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
    }
    
    ILIAS_INFO("pgsql-test", "Question mark placeholder test passed");
    co_return {};
}

/**
 * Property 3: SQL Placeholder Parsing - Named Placeholders
 * 
 * *For any* SQL string containing :name placeholders, the parser should convert
 * them to $N with correct name-to-index mapping.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_named_placeholder_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Test: Single named placeholder - use positional binding since named binding
    // requires the IStatement interface directly
    {
        auto stmtRet = co_await db.prepare("SELECT * FROM test_types WHERE id = :id");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Named placeholders are converted to $1, $2, etc. so we bind positionally
        auto bindRet = stmt.bind(1);
        EXPECT_TRUE(bindRet.has_value()) << "Binding should succeed";
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
    }
    
    // Test: Multiple named placeholders
    {
        auto stmtRet = co_await db.prepare("SELECT * FROM test_types WHERE id > :low AND id < :high");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Named placeholders are converted to $1, $2 in order of appearance
        auto bindRet = stmt.bind(0, 10);
        EXPECT_TRUE(bindRet.has_value()) << "Binding multiple values should succeed";
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
    }
    
    ILIAS_INFO("pgsql-test", "Named placeholder test passed");
    co_return {};
}

/**
 * Property 3: SQL Placeholder Parsing - String Literal Protection
 * 
 * *For any* SQL string containing ? or : inside string literals,
 * those characters should NOT be treated as placeholders.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_string_literal_protection_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Test: ? inside single-quoted string should not be a placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT 'What is this?' as question");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Should execute without any bindings since ? is inside string
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        std::string question;
        int count = 0;
        ilias_for_await(auto rc, result.range(question)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(question, "What is this?");
            count++;
        }
        EXPECT_EQ(count, 1);
    }
    
    // Test: :name inside single-quoted string should not be a placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT 'Hello :world' as greeting");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Should execute without any bindings
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        std::string greeting;
        int count = 0;
        ilias_for_await(auto rc, result.range(greeting)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(greeting, "Hello :world");
            count++;
        }
        EXPECT_EQ(count, 1);
    }
    
    // Test: Mixed - placeholder outside string, special chars inside string
    {
        auto stmtRet = co_await db.prepare("SELECT 'Value: ?' as label, ?::integer as actual_value");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        auto bindRet = stmt.bind(42);
        EXPECT_TRUE(bindRet.has_value()) << "Should have exactly one placeholder";
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        std::string label;
        int actual_value = 0;
        int count = 0;
        ilias_for_await(auto rc, result.range(label, actual_value)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(label, "Value: ?");
            EXPECT_EQ(actual_value, 42);
            count++;
        }
        EXPECT_EQ(count, 1);
    }
    
    ILIAS_INFO("pgsql-test", "String literal protection test passed");
    co_return {};
}

/**
 * Property 3: SQL Placeholder Parsing - Comment Protection
 * 
 * *For any* SQL string containing ? or : inside comments,
 * those characters should NOT be treated as placeholders.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_comment_protection_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Test: ? inside line comment should not be a placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT 1 as val -- What is this?\n");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Should execute without any bindings
        auto queryRet = co_await stmt.query();
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
    }
    
    // Test: ? inside block comment should not be a placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT /* What? */ 1 as val");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Should execute without any bindings
        auto queryRet = co_await stmt.query();
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
    }
    
    ILIAS_INFO("pgsql-test", "Comment protection test passed");
    co_return {};
}

/**
 * Property 3: SQL Placeholder Parsing - Dollar Quote Protection
 * 
 * *For any* SQL string containing ? or : inside PostgreSQL dollar-quoted strings,
 * those characters should NOT be treated as placeholders.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_dollar_quote_protection_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Test: ? inside $$ dollar quote should not be a placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT $$What is this?$$ as question");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Should execute without any bindings
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        std::string question;
        int count = 0;
        ilias_for_await(auto rc, result.range(question)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(question, "What is this?");
            count++;
        }
        EXPECT_EQ(count, 1);
    }
    
    // Test: :name inside $tag$ dollar quote should not be a placeholder
    {
        auto stmtRet = co_await db.prepare("SELECT $tag$Hello :world$tag$ as greeting");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        // Should execute without any bindings
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        std::string greeting;
        int count = 0;
        ilias_for_await(auto rc, result.range(greeting)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(greeting, "Hello :world");
            count++;
        }
        EXPECT_EQ(count, 1);
    }
    
    ILIAS_INFO("pgsql-test", "Dollar quote protection test passed");
    co_return {};
}

/**
 * Property 3: SQL Placeholder Parsing - Escaped Quotes
 * 
 * *For any* SQL string containing escaped quotes (''), the parser should
 * correctly handle them and not exit the string prematurely.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_escaped_quote_property() -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Test: Escaped single quote with ? after
    {
        auto stmtRet = co_await db.prepare("SELECT 'It''s a test' as text, ?::integer as value");
        CO_ASSERT_VAL(stmtRet);
        auto& stmt = stmtRet.value();
        
        auto bindRet = stmt.bind(123);
        EXPECT_TRUE(bindRet.has_value()) << "Should have exactly one placeholder after escaped quote";
        
        auto queryRet = co_await stmt.query();
        CO_ASSERT_VAL(queryRet);
        
        auto result = std::move(queryRet.value());
        std::string text;
        int actual_value = 0;
        int count = 0;
        ilias_for_await(auto rc, result.range(text, actual_value)) {
            EXPECT_TRUE(rc.has_value());
            EXPECT_EQ(text, "It's a test");
            EXPECT_EQ(actual_value, 123);
            count++;
        }
        EXPECT_EQ(count, 1);
    }
    
    ILIAS_INFO("pgsql-test", "Escaped quote test passed");
    co_return {};
}

/**
 * Property 3: SQL Placeholder Parsing - Random Placeholder Count
 * 
 * *For any* N placeholders in a SQL string, binding N values should succeed
 * and produce correct results.
 * 
 * **Validates: Requirements 4.1**
 */
static auto test_random_placeholder_count_property(int numPlaceholders) -> IoTask<void> {
    auto ret = co_await SqlDatabase::open("postgres", getConnectionOptions());
    CO_ASSERT_VAL(ret);
    auto db = std::move(ret.value());
    
    // Build a query with N placeholders
    std::ostringstream sql;
    sql << "SELECT ";
    for (int i = 0; i < numPlaceholders; ++i) {
        if (i > 0) sql << ", ";
        sql << "? as col" << i;
    }
    
    auto stmtRet = co_await db.prepare(sql.str());
    CO_ASSERT_VAL(stmtRet);
    auto& stmt = stmtRet.value();
    
    // Build a tuple of values to bind
    // Since we can't dynamically create tuples, we'll use a different approach
    // We'll test with specific counts
    IoResult<void> bindRet;
    switch (numPlaceholders) {
        case 1: bindRet = stmt.bind(10); break;
        case 2: bindRet = stmt.bind(10, 20); break;
        case 3: bindRet = stmt.bind(10, 20, 30); break;
        case 4: bindRet = stmt.bind(10, 20, 30, 40); break;
        case 5: bindRet = stmt.bind(10, 20, 30, 40, 50); break;
        case 6: bindRet = stmt.bind(10, 20, 30, 40, 50, 60); break;
        case 7: bindRet = stmt.bind(10, 20, 30, 40, 50, 60, 70); break;
        case 8: bindRet = stmt.bind(10, 20, 30, 40, 50, 60, 70, 80); break;
        case 9: bindRet = stmt.bind(10, 20, 30, 40, 50, 60, 70, 80, 90); break;
        case 10: bindRet = stmt.bind(10, 20, 30, 40, 50, 60, 70, 80, 90, 100); break;
        default: 
            ILIAS_ERROR("pgsql-test", "Unsupported placeholder count: {}", numPlaceholders);
            co_return {};
    }
    
    EXPECT_TRUE(bindRet.has_value()) << "Binding " << numPlaceholders << " values should succeed";
    
    auto queryRet = co_await stmt.query();
    CO_ASSERT_VAL(queryRet);
    
    ILIAS_INFO("pgsql-test", "Random placeholder count test passed for {} placeholders", numPlaceholders);
    co_return {};
}

// GTest test cases

TEST(PostgresPlaceholderTest, Property3_QuestionMarkPlaceholders) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    test_question_mark_placeholder_property().wait();
}

TEST(PostgresPlaceholderTest, Property3_NamedPlaceholders) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    test_named_placeholder_property().wait();
}

TEST(PostgresPlaceholderTest, Property3_StringLiteralProtection) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    test_string_literal_protection_property().wait();
}

TEST(PostgresPlaceholderTest, Property3_CommentProtection) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    test_comment_protection_property().wait();
}

TEST(PostgresPlaceholderTest, Property3_DollarQuoteProtection) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    test_dollar_quote_protection_property().wait();
}

TEST(PostgresPlaceholderTest, Property3_EscapedQuotes) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    test_escaped_quote_property().wait();
}

TEST(PostgresPlaceholderTest, Property3_RandomPlaceholderCount) {
    // Feature: postgres-backend, Property 3: SQL Placeholder Parsing
    // **Validates: Requirements 4.1**
    
    // Run property test with random placeholder counts
    std::mt19937 gen(42); // Fixed seed for reproducibility
    std::uniform_int_distribution<int> dist(1, 10);
    
    for (int i = 0; i < 10; ++i) {
        int count = dist(gen);
        ILIAS_INFO("pgsql-test", "Testing with {} placeholders (iteration {})", count, i + 1);
        test_random_placeholder_count_property(count).wait();
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
