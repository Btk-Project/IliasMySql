#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <nekoproto/serialization/reflection.hpp>
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sql_orm/detail/timestamp_manager.hpp"
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

// Comprehensive test entity with all SqlTags features
struct ComprehensiveTestEntity {
    int id = 0;
    std::string name = "";
    std::string email = "";
    std::string description = "";
    int age = 0;
    double balance = 0.0;
    bool is_active = true;
    std::chrono::system_clock::time_point created_at = {};
    std::chrono::system_clock::time_point updated_at = {};
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<ComprehensiveTestEntity, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&ComprehensiveTestEntity::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .index = true,
            .length = 100
        }>(&ComprehensiveTestEntity::name),
        "email", make_tags<SqlTags {
            .not_null = true, 
            .unique = true, 
            .length = 255
        }>(&ComprehensiveTestEntity::email),
        "description", make_tags<SqlTags {
            .length = 0  // Should use TEXT type
        }>(&ComprehensiveTestEntity::description),
        "age", make_tags<SqlTags {
            .not_null = true,
            .unsigned_type = true
        }>(&ComprehensiveTestEntity::age),
        "balance", make_tags<SqlTags {
            .not_null = true,
            .unsigned_type = true
        }>(&ComprehensiveTestEntity::balance),
        "is_active", make_tags<SqlTags {
            .not_null = true
        }>(&ComprehensiveTestEntity::is_active),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&ComprehensiveTestEntity::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&ComprehensiveTestEntity::updated_at)
    );
};
NEKO_END_NAMESPACE

class EndToEndIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // This test will be run asynchronously
    }
    
    void TearDown() override {
        // Cleanup will be handled by the async tests
    }
    
    static auto setup_db() -> IoTask<SqlDatabase> {
        auto ret = co_await SqlDatabase::open_in_memory();
        if (!ret) {
            throw std::runtime_error("Failed to open DB");
        }
        co_return ret.value();
    }
    
    template<typename EntityType, typename TagType>
    static auto createTestTable(SqlDatabase& db, const std::string& tableName) -> IoTask<void> {
        // Create table using Form::create
        auto formResult = co_await Form<EntityType, TagType>::create(db, tableName);
        if (!formResult.has_value()) {
            throw std::runtime_error("Failed to create table: " + formResult.error().message());
        }
        co_return {};
    }
};

// Test complete schema generation and validation
static auto test_schema_generation_and_validation() -> IoTask<void> {
    auto db = (co_await setup_db()).value();
    
    // Create table with enhanced SqlTags
    co_await createTestTable<ComprehensiveTestEntity, SqliteTag>(db, "test_entities");
    
    // Verify table was created successfully by querying table info
    auto tableInfoResult = co_await db.query<std::tuple<std::string, std::string>>("PRAGMA table_info(test_entities)");
    CO_ASSERT_VAL(tableInfoResult);
    
    auto tableInfo = std::move(tableInfoResult.value());
    
    // Verify all columns exist with correct properties
    std::vector<std::string> expectedColumns = {
        "id", "name", "email", "description", "age", "balance", "is_active", "created_at", "updated_at"
    };
    
    std::vector<std::string> actualColumns;
    ilias_for_await(auto row, tableInfo.range()) {
        auto [columnName, columnType] = row;
        actualColumns.push_back(columnName);
    }
    
    // Check that all expected columns are present
    for (const auto& expectedCol : expectedColumns) {
        bool found = std::find(actualColumns.begin(), actualColumns.end(), expectedCol) != actualColumns.end();
        EXPECT_TRUE(found) << "Column '" << expectedCol << "' not found in table";
    }
    
    co_return {};
}

TEST(EndToEndIntegration, SchemaGenerationAndValidation) {
    test_schema_generation_and_validation().wait();
}

// Test automatic timestamp behavior during insert operations
static auto test_automatic_timestamp_insert() -> IoTask<void> {
    auto db = (co_await setup_db()).value();
    
    // Create table with enhanced SqlTags
    auto formResult = co_await Form<ComprehensiveTestEntity, SqliteTag>::create(db, "test_entities");
    CO_ASSERT_VAL(formResult);
    auto form = std::move(formResult.value());
    
    // Create entity without setting timestamps
    ComprehensiveTestEntity entity;
    entity.name = "Test User";
    entity.email = "test@example.com";
    entity.description = "Test description";
    entity.age = 25;
    entity.balance = 100.50;
    entity.is_active = true;
    // Note: created_at and updated_at are not set manually
    
    // Record time before insert
    auto beforeInsert = std::chrono::system_clock::now();
    
    // Insert using ORM with automatic timestamp handling
    auto insertResult = co_await form.insert(entity);
    CO_ASSERT_VAL(insertResult);
    
    // Record time after insert
    auto afterInsert = std::chrono::system_clock::now();
    
    // Retrieve the inserted entity
    auto selectResult = co_await form.select().where(form.sql(&ComprehensiveTestEntity::email) == entity.email).query();
    CO_ASSERT_VAL(selectResult);
    
    auto result = std::move(selectResult.value());
    ComprehensiveTestEntity retrievedEntity;
    bool found = false;
    
    ilias_for_await(auto row, result.range()) {
        retrievedEntity = row;
        found = true;
        break;
    }
    
    EXPECT_TRUE(found) << "Should find the inserted entity";
    
    // Verify timestamps were automatically set
    EXPECT_NE(retrievedEntity.created_at, std::chrono::system_clock::time_point{}) 
        << "created_at should be automatically set";
    EXPECT_NE(retrievedEntity.updated_at, std::chrono::system_clock::time_point{}) 
        << "updated_at should be automatically set";
    
    // Verify timestamps are within reasonable range
    EXPECT_GE(retrievedEntity.created_at, beforeInsert) 
        << "created_at should be after insert start time";
    EXPECT_LE(retrievedEntity.created_at, afterInsert) 
        << "created_at should be before insert end time";
    
    // For insert operations, both created_at and updated_at should be similar
    auto timeDiff = std::chrono::duration_cast<std::chrono::seconds>(
        retrievedEntity.updated_at - retrievedEntity.created_at).count();
    EXPECT_LE(std::abs(timeDiff), 1) 
        << "created_at and updated_at should be very close for insert operations";
    
    co_return {};
}

TEST(EndToEndIntegration, AutomaticTimestampInsert) {
    test_automatic_timestamp_insert().wait();
}

// Test automatic timestamp behavior during update operations
TEST_F(EndToEndIntegrationTest, AutomaticTimestampUpdate) {
    // First, insert an entity
    ComprehensiveTestEntity entity;
    entity.name = "Test User";
    entity.email = "test@example.com";
    entity.description = "Original description";
    entity.age = 25;
    entity.balance = 100.50;
    entity.is_active = true;
    
    auto insertResult = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity);
    CO_ASSERT_VAL(insertResult);
    
    // Retrieve the inserted entity to get its ID and original timestamps
    auto selectResult = Form<ComprehensiveTestEntity, SqliteTag>::select(*db)
        .where("email = ?", entity.email)
        .first();
    CO_ASSERT_VAL(selectResult);
    
    auto originalEntity = selectResult.value();
    auto originalCreatedAt = originalEntity.created_at;
    auto originalUpdatedAt = originalEntity.updated_at;
    
    // Wait a bit to ensure timestamp difference
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Update the entity
    auto beforeUpdate = std::chrono::system_clock::now();
    
    auto updateResult = Form<ComprehensiveTestEntity, SqliteTag>::update(*db)
        .set("description", "Updated description")
        .set("age", 26)
        .where("id = ?", originalEntity.id)
        .exec();
    CO_ASSERT_VAL(updateResult);
    
    auto afterUpdate = std::chrono::system_clock::now();
    
    // Retrieve the updated entity
    auto updatedSelectResult = Form<ComprehensiveTestEntity, SqliteTag>::select(*db)
        .where("id = ?", originalEntity.id)
        .first();
    CO_ASSERT_VAL(updatedSelectResult);
    
    auto updatedEntity = updatedSelectResult.value();
    
    // Verify created_at remained unchanged
    EXPECT_EQ(updatedEntity.created_at, originalCreatedAt) 
        << "created_at should not change during updates";
    
    // Verify updated_at was automatically updated
    EXPECT_GT(updatedEntity.updated_at, originalUpdatedAt) 
        << "updated_at should be automatically updated";
    EXPECT_GE(updatedEntity.updated_at, beforeUpdate) 
        << "updated_at should be after update start time";
    EXPECT_LE(updatedEntity.updated_at, afterUpdate) 
        << "updated_at should be before update end time";
    
    // Verify the actual data was updated
    EXPECT_EQ(updatedEntity.description, "Updated description");
    EXPECT_EQ(updatedEntity.age, 26);
}

// Test constraint enforcement
TEST_F(EndToEndIntegrationTest, ConstraintEnforcement) {
    // Test NOT NULL constraint
    ComprehensiveTestEntity entity1;
    entity1.email = "test1@example.com";
    // name is required (not_null = true) but not set
    
    auto insertResult1 = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity1);
    EXPECT_FALSE(insertResult1.has_value()) 
        << "Insert should fail when NOT NULL constraint is violated";
    
    // Test UNIQUE constraint
    ComprehensiveTestEntity entity2;
    entity2.name = "User 1";
    entity2.email = "unique@example.com";
    entity2.age = 25;
    entity2.balance = 100.0;
    
    auto insertResult2 = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity2);
    CO_ASSERT_VAL(insertResult2);
    
    // Try to insert another entity with the same email (should fail due to UNIQUE constraint)
    ComprehensiveTestEntity entity3;
    entity3.name = "User 2";
    entity3.email = "unique@example.com";  // Same email as entity2
    entity3.age = 30;
    entity3.balance = 200.0;
    
    auto insertResult3 = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity3);
    EXPECT_FALSE(insertResult3.has_value()) 
        << "Insert should fail when UNIQUE constraint is violated";
}

// Test bulk operations with timestamp consistency
TEST_F(EndToEndIntegrationTest, BulkOperationsTimestampConsistency) {
    // Insert multiple entities
    std::vector<ComprehensiveTestEntity> entities;
    for (int i = 0; i < 5; ++i) {
        ComprehensiveTestEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@example.com";
        entity.description = "Description " + std::to_string(i);
        entity.age = 20 + i;
        entity.balance = 100.0 * (i + 1);
        entity.is_active = (i % 2 == 0);
        entities.push_back(entity);
    }
    
    auto beforeBulkInsert = std::chrono::system_clock::now();
    
    // Insert all entities
    for (auto& entity : entities) {
        auto insertResult = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity);
        CO_ASSERT_VAL(insertResult);
    }
    
    auto afterBulkInsert = std::chrono::system_clock::now();
    
    // Retrieve all inserted entities
    auto selectResult = Form<ComprehensiveTestEntity, SqliteTag>::select(*db)
        .orderBy("id")
        .all();
    CO_ASSERT_VAL(selectResult);
    
    auto retrievedEntities = selectResult.value();
    EXPECT_EQ(retrievedEntities.size(), 5) << "Should retrieve all 5 inserted entities";
    
    // Verify timestamp consistency across bulk operations
    for (const auto& entity : retrievedEntities) {
        EXPECT_GE(entity.created_at, beforeBulkInsert) 
            << "All created_at timestamps should be after bulk insert start";
        EXPECT_LE(entity.created_at, afterBulkInsert) 
            << "All created_at timestamps should be before bulk insert end";
        
        EXPECT_GE(entity.updated_at, beforeBulkInsert) 
            << "All updated_at timestamps should be after bulk insert start";
        EXPECT_LE(entity.updated_at, afterBulkInsert) 
            << "All updated_at timestamps should be before bulk insert end";
    }
}

// Test performance impact measurement
TEST_F(EndToEndIntegrationTest, PerformanceImpactMeasurement) {
    const int numOperations = 100;
    
    // Measure insert performance
    auto insertStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        ComprehensiveTestEntity entity;
        entity.name = "Perf User " + std::to_string(i);
        entity.email = "perf" + std::to_string(i) + "@example.com";
        entity.description = "Performance test description " + std::to_string(i);
        entity.age = 20 + (i % 50);
        entity.balance = 100.0 + i;
        entity.is_active = (i % 2 == 0);
        
        auto insertResult = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity);
        ASSERT_TRUE(insertResult.has_value()) << "Insert " << i << " should succeed";
    }
    
    auto insertEndTime = std::chrono::high_resolution_clock::now();
    auto insertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        insertEndTime - insertStartTime).count();
    
    // Measure select performance
    auto selectStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        auto selectResult = Form<ComprehensiveTestEntity, SqliteTag>::select(*db)
            .where("name = ?", "Perf User " + std::to_string(i))
            .first();
        ASSERT_TRUE(selectResult.has_value()) << "Select " << i << " should succeed";
    }
    
    auto selectEndTime = std::chrono::high_resolution_clock::now();
    auto selectDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        selectEndTime - selectStartTime).count();
    
    // Performance expectations (these are reasonable for SQLite in-memory operations)
    EXPECT_LT(insertDuration, 5000) << "Insert operations should complete within 5 seconds";
    EXPECT_LT(selectDuration, 2000) << "Select operations should complete within 2 seconds";
    
    // Log performance metrics for analysis
    std::cout << "Performance Metrics:" << std::endl;
    std::cout << "  Insert " << numOperations << " records: " << insertDuration << "ms" << std::endl;
    std::cout << "  Select " << numOperations << " records: " << selectDuration << "ms" << std::endl;
    std::cout << "  Average insert time: " << (double)insertDuration / numOperations << "ms per record" << std::endl;
    std::cout << "  Average select time: " << (double)selectDuration / numOperations << "ms per record" << std::endl;
}

// Test error handling and reporting
TEST_F(EndToEndIntegrationTest, ErrorHandlingAndReporting) {
    // Test constraint violation error context
    ComprehensiveTestEntity entity1;
    entity1.name = "Test User";
    entity1.email = "test@example.com";
    entity1.age = 25;
    entity1.balance = 100.0;
    
    // Insert first entity successfully
    auto insertResult1 = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity1);
    CO_ASSERT_VAL(insertResult1);
    
    // Try to insert duplicate email (should provide meaningful error)
    ComprehensiveTestEntity entity2;
    entity2.name = "Another User";
    entity2.email = "test@example.com";  // Duplicate email
    entity2.age = 30;
    entity2.balance = 200.0;
    
    auto insertResult2 = Form<ComprehensiveTestEntity, SqliteTag>::insert(*db, entity2);
    EXPECT_FALSE(insertResult2.has_value()) << "Duplicate email insert should fail";
    
    if (!insertResult2.has_value()) {
        auto error = insertResult2.error();
        std::string errorMsg = error.message();
        
        // Error should contain context about the constraint violation
        EXPECT_TRUE(errorMsg.find("UNIQUE") != std::string::npos || 
                   errorMsg.find("unique") != std::string::npos ||
                   errorMsg.find("constraint") != std::string::npos)
            << "Error message should indicate constraint violation: " << errorMsg;
    }
}

// Test validation system integration
TEST_F(EndToEndIntegrationTest, ValidationSystemIntegration) {
    // Test SqlTags validation
    auto validationErrors = Form<ComprehensiveTestEntity, SqliteTag>::validateTableConfiguration();
    EXPECT_TRUE(validationErrors.empty()) 
        << "ComprehensiveTestEntity should have valid SqlTags configuration";
    
    // Test timestamp field detection
    auto timestampFields = Form<ComprehensiveTestEntity, SqliteTag>::getTimestampFields();
    EXPECT_GE(timestampFields.size(), 2) 
        << "Should detect at least 2 timestamp fields (created_at, updated_at)";
    
    auto createdAtFields = Form<ComprehensiveTestEntity, SqliteTag>::getCreatedAtFields();
    EXPECT_GE(createdAtFields.size(), 1) 
        << "Should detect at least 1 created_at field";
    
    auto updatedAtFields = Form<ComprehensiveTestEntity, SqliteTag>::getUpdatedAtFields();
    EXPECT_GE(updatedAtFields.size(), 1) 
        << "Should detect at least 1 updated_at field";
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}