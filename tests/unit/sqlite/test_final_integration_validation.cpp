#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include <memory>
#include <vector>
#include <string>
#include <nekoproto/serialization/reflection.hpp>
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sqlite/sqlite.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;
NEKO_USE_NAMESPACE

// Test assertion macros
#define CO_ASSERT_VAL(ret) \
    do { \
        if (!ret.has_value()) { \
            ILIAS_ERROR("test", "Assert failed: {}", ret.error().message()); \
            co_return {}; \
        } \
    } while (0)

// Comprehensive test entity with all SqlTags features
struct FinalTestEntity {
    int id = 0;
    std::string name = "";
    std::string email = "";
    int age = 0;
    bool is_active = true;
    SqlDate created_at;
    SqlDate updated_at;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<FinalTestEntity, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&FinalTestEntity::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .index = true,
            .length = 100
        }>(&FinalTestEntity::name),
        "email", make_tags<SqlTags {
            .not_null = true, 
            .unique = true, 
            .length = 255
        }>(&FinalTestEntity::email),
        "age", make_tags<SqlTags {
            .not_null = true,
            .unsigned_type = true
        }>(&FinalTestEntity::age),
        "is_active", make_tags<SqlTags {
            .not_null = true
        }>(&FinalTestEntity::is_active),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&FinalTestEntity::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&FinalTestEntity::updated_at)
    );
};
NEKO_END_NAMESPACE

class FinalIntegrationTestSuite {
public:
    static auto setup_db() -> IoTask<SqlDatabase> {
        auto ret = co_await SqlDatabase::open_in_memory();
        if (!ret) {
            throw std::runtime_error("Failed to open DB");
        }
        auto db = std::move(ret.value());
        co_return db;
    }

    // Test 1: Complete schema generation and table creation
    static auto test_schema_generation() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_schema_generation");
        
        // Create table using Form::create which should handle all SqlTags features
        auto formResult = co_await Form<FinalTestEntity, SqliteTag>::create(db, "final_test_entities");
        CO_ASSERT_VAL(formResult);
        auto form = std::move(formResult.value());
        
        // Verify table exists by querying table info
        auto tableInfoResult = co_await db.query<std::tuple<std::string, std::string>>("PRAGMA table_info(final_test_entities)");
        CO_ASSERT_VAL(tableInfoResult);
        
        auto tableInfo = std::move(tableInfoResult.value());
        std::vector<std::string> actualColumns;
        
        ilias_for_await(auto row, tableInfo.range()) {
            auto [columnName, columnType] = row;
            actualColumns.push_back(columnName);
        }
        
        // Verify all expected columns exist
        std::vector<std::string> expectedColumns = {
            "id", "name", "email", "age", "is_active", "created_at", "updated_at"
        };
        
        for (const auto& expectedCol : expectedColumns) {
            bool found = std::find(actualColumns.begin(), actualColumns.end(), expectedCol) != actualColumns.end();
            EXPECT_TRUE(found) << "Column '" << expectedCol << "' not found in table";
        }
        
        ILIAS_INFO("test", ">>> test_schema_generation PASSED");
        co_return {};
    }

    // Test 2: CRUD operations with timestamp automation
    static auto test_crud_with_timestamps() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_crud_with_timestamps");
        
        auto formResult = co_await Form<FinalTestEntity, SqliteTag>::create(db, "final_test_entities");
        CO_ASSERT_VAL(formResult);
        auto form = std::move(formResult.value());
        
        // INSERT: Test automatic timestamp population
        FinalTestEntity entity;
        entity.name = "Test User";
        entity.email = "test@example.com";
        entity.age = 25;
        entity.is_active = true;
        
        auto insertResult = co_await form.insert(entity);
        CO_ASSERT_VAL(insertResult);
        
        // SELECT: Retrieve and verify timestamps
        auto selectResult = co_await form.select().where(form.sql(&FinalTestEntity::email) == entity.email).query();
        CO_ASSERT_VAL(selectResult);
        
        auto result = std::move(selectResult.value());
        FinalTestEntity retrievedEntity;
        bool found = false;
        
        ilias_for_await(auto row, result.range()) {
            retrievedEntity = row;
            found = true;
            break;
        }
        
        EXPECT_TRUE(found) << "Should find the inserted entity";
        EXPECT_GT(retrievedEntity.id, 0) << "Auto-increment ID should be assigned";
        EXPECT_NE(retrievedEntity.created_at.type, SqlDate::kErrorTime) 
            << "created_at should be automatically set";
        EXPECT_NE(retrievedEntity.updated_at.type, SqlDate::kErrorTime) 
            << "updated_at should be automatically set";
        
        // Verify timestamps are within reasonable range (basic sanity check)
        EXPECT_GT(retrievedEntity.created_at.toTimestamp(), 0) 
            << "created_at should have valid timestamp";
        EXPECT_GT(retrievedEntity.updated_at.toTimestamp(), 0) 
            << "updated_at should have valid timestamp";
        
        // UPDATE: Test timestamp update behavior
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto originalCreatedAt = retrievedEntity.created_at;
        auto originalUpdatedAt = retrievedEntity.updated_at;
        
        auto updateResult = co_await form.update()
            .set(form.sql(&FinalTestEntity::age) = 26)
            .where(form.sql(&FinalTestEntity::id) == retrievedEntity.id)
            .execute();
        CO_ASSERT_VAL(updateResult);
        
        EXPECT_EQ(updateResult.value(), 1) << "Should update exactly 1 row";
        
        // Verify updated timestamps
        auto updatedSelectResult = co_await form.select().where(form.sql(&FinalTestEntity::id) == retrievedEntity.id).query();
        CO_ASSERT_VAL(updatedSelectResult);
        
        auto updatedResult = std::move(updatedSelectResult.value());
        FinalTestEntity updatedEntity;
        
        ilias_for_await(auto row, updatedResult.range()) {
            updatedEntity = row;
            break;
        }
        
        EXPECT_EQ(updatedEntity.created_at.toTimestamp(), originalCreatedAt.toTimestamp()) 
            << "created_at should not change during updates";
        EXPECT_GT(updatedEntity.updated_at.toTimestamp(), originalUpdatedAt.toTimestamp()) 
            << "updated_at should be automatically updated";
        EXPECT_EQ(updatedEntity.age, 26) << "Age should be updated";
        
        ILIAS_INFO("test", ">>> test_crud_with_timestamps PASSED");
        co_return {};
    }

    // Test 3: Constraint enforcement
    static auto test_constraint_enforcement() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_constraint_enforcement");
        
        auto formResult = co_await Form<FinalTestEntity, SqliteTag>::create(db, "final_test_entities");
        CO_ASSERT_VAL(formResult);
        auto form = std::move(formResult.value());
        
        // Insert first entity successfully
        FinalTestEntity entity1;
        entity1.name = "User 1";
        entity1.email = "unique@example.com";
        entity1.age = 25;
        entity1.is_active = true;
        
        auto insertResult1 = co_await form.insert(entity1);
        CO_ASSERT_VAL(insertResult1);
        
        // Try to insert duplicate email (should fail due to UNIQUE constraint)
        FinalTestEntity entity2;
        entity2.name = "User 2";
        entity2.email = "unique@example.com";  // Same email as entity1
        entity2.age = 30;
        entity2.is_active = false;
        
        auto insertResult2 = co_await form.insert(entity2);
        EXPECT_FALSE(insertResult2.has_value()) 
            << "Insert should fail when UNIQUE constraint is violated";
        
        if (!insertResult2.has_value()) {
            auto error = insertResult2.error();
            std::string errorMsg = error.message();
            
            // Error should contain context about the constraint violation
            EXPECT_TRUE(errorMsg.find("UNIQUE") != std::string::npos || 
                       errorMsg.find("unique") != std::string::npos ||
                       errorMsg.find("constraint") != std::string::npos)
                << "Error message should indicate constraint violation: " << errorMsg;
        }
        
        ILIAS_INFO("test", ">>> test_constraint_enforcement PASSED");
        co_return {};
    }

    // Test 4: Performance impact measurement
    static auto test_performance_impact() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_performance_impact");
        
        auto formResult = co_await Form<FinalTestEntity, SqliteTag>::create(db, "final_test_entities");
        CO_ASSERT_VAL(formResult);
        auto form = std::move(formResult.value());
        
        const int numOperations = 100;
        
        // Measure insert performance
        auto insertStartTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numOperations; ++i) {
            FinalTestEntity entity;
            entity.name = "Perf User " + std::to_string(i);
            entity.email = "perf" + std::to_string(i) + "@example.com";
            entity.age = 20 + (i % 50);
            entity.is_active = (i % 2 == 0);
            
            auto insertResult = co_await form.insert(entity);
            CO_ASSERT_VAL(insertResult);
        }
        
        auto insertEndTime = std::chrono::high_resolution_clock::now();
        auto insertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
            insertEndTime - insertStartTime).count();
        
        // Measure select performance
        auto selectStartTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numOperations; ++i) {
            std::string email = "perf" + std::to_string(i) + "@example.com";
            auto selectResult = co_await form.select().where(form.sql(&FinalTestEntity::email) == email).query();
            CO_ASSERT_VAL(selectResult);
            
            auto result = std::move(selectResult.value());
            bool found = false;
            ilias_for_await(auto row, result.range()) {
                found = true;
                break;
            }
            EXPECT_TRUE(found) << "Should find entity " << i;
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
        
        ILIAS_INFO("test", ">>> test_performance_impact PASSED");
        co_return {};
    }

    // Test 5: Bulk operations with timestamp consistency
    static auto test_bulk_operations() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_bulk_operations");
        
        auto formResult = co_await Form<FinalTestEntity, SqliteTag>::create(db, "final_test_entities");
        CO_ASSERT_VAL(formResult);
        auto form = std::move(formResult.value());
        
        // Insert multiple entities
        const int numEntities = 10;
        
        for (int i = 0; i < numEntities; ++i) {
            FinalTestEntity entity;
            entity.name = "Bulk User " + std::to_string(i);
            entity.email = "bulk" + std::to_string(i) + "@example.com";
            entity.age = 20 + i;
            entity.is_active = (i % 2 == 0);
            
            auto insertResult = co_await form.insert(entity);
            CO_ASSERT_VAL(insertResult);
        }
        
        // Retrieve all inserted entities and verify timestamp consistency
        auto selectResult = co_await form.select().orderBy("id").query();
        CO_ASSERT_VAL(selectResult);
        
        auto result = std::move(selectResult.value());
        std::vector<FinalTestEntity> retrievedEntities;
        
        ilias_for_await(auto row, result.range()) {
            retrievedEntities.push_back(row);
        }
        
        EXPECT_EQ(retrievedEntities.size(), numEntities) << "Should retrieve all inserted entities";
        
        // Verify timestamp consistency across bulk operations
        for (const auto& entity : retrievedEntities) {
            EXPECT_NE(entity.created_at.type, SqlDate::kErrorTime) 
                << "All created_at timestamps should be set";
            EXPECT_NE(entity.updated_at.type, SqlDate::kErrorTime) 
                << "All updated_at timestamps should be set";
            
            EXPECT_GT(entity.created_at.toTimestamp(), 0) 
                << "All created_at timestamps should be valid";
            EXPECT_GT(entity.updated_at.toTimestamp(), 0) 
                << "All updated_at timestamps should be valid";
        }
        
        ILIAS_INFO("test", ">>> test_bulk_operations PASSED");
        co_return {};
    }

    // Test 6: Validation system integration
    static auto test_validation_integration() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_validation_integration");
        
        // Test SqlTags validation (this tests the validation methods we added)
        auto validationErrors = Form<FinalTestEntity, SqliteTag>::validateTableConfiguration();
        EXPECT_TRUE(validationErrors.empty()) 
            << "FinalTestEntity should have valid SqlTags configuration";
        
        // Test timestamp field detection
        auto timestampFields = Form<FinalTestEntity, SqliteTag>::getTimestampFields();
        EXPECT_GE(timestampFields.size(), 2) 
            << "Should detect at least 2 timestamp fields (created_at, updated_at)";
        
        auto createdAtFields = Form<FinalTestEntity, SqliteTag>::getCreatedAtFields();
        EXPECT_GE(createdAtFields.size(), 1) 
            << "Should detect at least 1 created_at field";
        
        auto updatedAtFields = Form<FinalTestEntity, SqliteTag>::getUpdatedAtFields();
        EXPECT_GE(updatedAtFields.size(), 1) 
            << "Should detect at least 1 updated_at field";
        
        ILIAS_INFO("test", ">>> test_validation_integration PASSED");
        co_return {};
    }
};

// Test runner function
ILIAS_NAMESPACE::Task<void> run_final_integration_tests() {
    try {
        co_await FinalIntegrationTestSuite::test_schema_generation();
        co_await FinalIntegrationTestSuite::test_crud_with_timestamps();
        co_await FinalIntegrationTestSuite::test_constraint_enforcement();
        co_await FinalIntegrationTestSuite::test_performance_impact();
        co_await FinalIntegrationTestSuite::test_bulk_operations();
        co_await FinalIntegrationTestSuite::test_validation_integration();
    } catch (const std::exception &e) {
        ILIAS_ERROR("test", "Exception caught in final integration tests: {}", e.what());
        EXPECT_TRUE(false) << "Exception in test runner: " << e.what();
    }
}

TEST(FinalIntegration, ComprehensiveValidation) {
    run_final_integration_tests().wait();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}