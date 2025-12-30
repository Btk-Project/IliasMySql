#include <cstdint>
#include <chrono>
#include <thread>
#include <gtest/gtest.h>
#include <nekoproto/serialization/reflection.hpp>
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql_orm/detail/timestamp_manager.hpp"
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sqlite/sqlite.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;
NEKO_USE_NAMESPACE

// Test assertion macros
#define CO_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            ILIAS_ERROR("test", "Assertion failed: {}", #condition); \
            co_return {}; \
        } \
    } while (0)

#define CO_ASSERT_VAL(ret) \
    do { \
        if (!ret.has_value()) { \
            ILIAS_ERROR("test", "Assert failed: {}", ret.error().message()); \
            co_return {}; \
        } \
    } while (0)

// Test entities with enhanced SqlTags
struct EnhancedUser {
    int id = 0;
    std::string name = "";
    std::string email = "";
    std::chrono::system_clock::time_point created_at = {};
    std::chrono::system_clock::time_point updated_at = {};
    bool is_active = true;
};

struct ProductRecord {
    int id = 0;
    std::string name = "";
    double price = 0.0;
    int stock_quantity = 0;
    std::chrono::system_clock::time_point created_at = {};
    std::chrono::system_clock::time_point updated_at = {};
};

NEKO_BEGIN_NAMESPACE
// Enhanced SqlTags configurations for testing
template <>
struct Meta<EnhancedUser, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&EnhancedUser::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .length = 100
        }>(&EnhancedUser::name),
        "email", make_tags<SqlTags {
            .not_null = true, 
            .unique = true, 
            .length = 255
        }>(&EnhancedUser::email),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&EnhancedUser::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&EnhancedUser::updated_at),
        "is_active", make_tags<SqlTags {
            .not_null = true
        }>(&EnhancedUser::is_active)
    );
};

template <>
struct Meta<ProductRecord, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&ProductRecord::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .index = true, 
            .length = 200
        }>(&ProductRecord::name),
        "price", make_tags<SqlTags {
            .not_null = true, 
            .unsigned_type = true
        }>(&ProductRecord::price),
        "stock_quantity", make_tags<SqlTags {
            .not_null = true, 
            .unsigned_type = true
        }>(&ProductRecord::stock_quantity),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&ProductRecord::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&ProductRecord::updated_at)
    );
};
NEKO_END_NAMESPACE

// Simple user structure for backward compatibility testing
struct SimpleUser {
    int id = 0;
    std::string name = "";
    std::optional<int> score = 0;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {.primary_key = true, .not_null = true, .unique = true}>(&SimpleUser::id),
        "name", make_tags<SqlTags {.not_null = true}>(&SimpleUser::name),
        "score", make_tags<SqlTags {}>(&SimpleUser::score)
    );
};
NEKO_END_NAMESPACE

class ORMFormsIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize any test setup if needed
    }
};

// Test SqlTags helper methods
TEST_F(ORMFormsIntegrationTest, SqlTagsHelperMethods) {
    // Test createPrimaryKeyTags helper
    auto primaryKeyTags = Form<EnhancedUser, SqliteTag>::createPrimaryKeyTags(true);
    EXPECT_TRUE(primaryKeyTags.primary_key);
    EXPECT_TRUE(primaryKeyTags.not_null);
    EXPECT_TRUE(primaryKeyTags.unique);
    EXPECT_TRUE(primaryKeyTags.auto_increment);
    
    // Test createUniqueIndexTags helper
    auto uniqueIndexTags = Form<EnhancedUser, SqliteTag>::createUniqueIndexTags(255);
    EXPECT_TRUE(uniqueIndexTags.not_null);
    EXPECT_TRUE(uniqueIndexTags.unique);
    EXPECT_TRUE(uniqueIndexTags.index);
    EXPECT_EQ(uniqueIndexTags.length, 255);
    
    // Test createTimestampTags helper
    auto createdAtTags = Form<EnhancedUser, SqliteTag>::createTimestampTags(true);
    EXPECT_TRUE(createdAtTags.not_null);
    EXPECT_TRUE(createdAtTags.created_at);
    EXPECT_FALSE(createdAtTags.updated_at);
    
    auto updatedAtTags = Form<EnhancedUser, SqliteTag>::createTimestampTags(false);
    EXPECT_TRUE(updatedAtTags.not_null);
    EXPECT_FALSE(updatedAtTags.created_at);
    EXPECT_TRUE(updatedAtTags.updated_at);
    
    // Test createStringTags helper
    auto stringTags = Form<EnhancedUser, SqliteTag>::createStringTags(100, true, true);
    EXPECT_TRUE(stringTags.not_null);
    EXPECT_TRUE(stringTags.index);
    EXPECT_EQ(stringTags.length, 100);
    
    // Test createNumericTags helper
    auto numericTags = Form<EnhancedUser, SqliteTag>::createNumericTags(true, true);
    EXPECT_TRUE(numericTags.not_null);
    EXPECT_TRUE(numericTags.unsigned_type);
}

// Test SqlTags validation functionality
TEST_F(ORMFormsIntegrationTest, SqlTagsValidation) {
    // Test validation of table configuration
    auto validationErrors = Form<EnhancedUser, SqliteTag>::validateTableConfiguration();
    
    // EnhancedUser should have valid configuration
    EXPECT_TRUE(validationErrors.empty()) << "EnhancedUser should have valid SqlTags configuration";
    
    // Test timestamp field detection
    auto timestampFields = Form<EnhancedUser, SqliteTag>::getTimestampFields();
    EXPECT_GE(timestampFields.size(), 2) << "EnhancedUser should have at least 2 timestamp fields";
    
    auto createdAtFields = Form<EnhancedUser, SqliteTag>::getCreatedAtFields();
    EXPECT_GE(createdAtFields.size(), 1) << "EnhancedUser should have at least 1 created_at field";
    
    auto updatedAtFields = Form<EnhancedUser, SqliteTag>::getUpdatedAtFields();
    EXPECT_GE(updatedAtFields.size(), 1) << "EnhancedUser should have at least 1 updated_at field";
}

// Test index statement generation
TEST_F(ORMFormsIntegrationTest, IndexStatementGeneration) {
    // Test index statement generation for ProductRecord (has index = true on name field)
    auto indexStatements = Form<ProductRecord, SqliteTag>::generateIndexStatements("test_products");
    
    // ProductRecord should generate at least one index statement for the name field
    EXPECT_GE(indexStatements.size(), 1) << "ProductRecord should generate index statements";
    
    // Check that the generated SQL contains expected elements
    bool foundNameIndex = false;
    for (const auto& statement : indexStatements) {
        if (statement.find("name") != std::string::npos && 
            statement.find("CREATE INDEX") != std::string::npos) {
            foundNameIndex = true;
            break;
        }
    }
    EXPECT_TRUE(foundNameIndex) << "Should generate index for name field";
}

// Test backward compatibility with existing SqlTags usage
TEST_F(ORMFormsIntegrationTest, BackwardCompatibility) {
    // Test that existing functionality still works with SimpleUser
    auto validationErrors = Form<SimpleUser, SqliteTag>::validateTableConfiguration();
    EXPECT_TRUE(validationErrors.empty()) << "SimpleUser should have valid SqlTags configuration";
    
    // Test helper methods work with SimpleUser too
    auto primaryKeyTags = Form<SimpleUser, SqliteTag>::createPrimaryKeyTags(false);
    EXPECT_TRUE(primaryKeyTags.primary_key);
    EXPECT_TRUE(primaryKeyTags.not_null);
    EXPECT_TRUE(primaryKeyTags.unique);
    EXPECT_FALSE(primaryKeyTags.auto_increment);
}

// Test TableAlias helper methods
TEST_F(ORMFormsIntegrationTest, TableAliasHelperMethods) {
    // Create a mock database and form (we won't actually use them for database operations)
    // Just test that the helper methods are accessible through TableAlias
    
    // Test that TableAlias delegates helper methods correctly
    auto primaryKeyTags = TableAlias<EnhancedUser, SqliteTag>::createPrimaryKeyTags(true);
    EXPECT_TRUE(primaryKeyTags.primary_key);
    EXPECT_TRUE(primaryKeyTags.auto_increment);
    
    auto timestampFields = TableAlias<EnhancedUser, SqliteTag>::getTimestampFields();
    EXPECT_GE(timestampFields.size(), 2) << "TableAlias should delegate timestamp field detection";
    
    auto validationErrors = TableAlias<EnhancedUser, SqliteTag>::validateTableConfiguration();
    EXPECT_TRUE(validationErrors.empty()) << "TableAlias should delegate validation";
}