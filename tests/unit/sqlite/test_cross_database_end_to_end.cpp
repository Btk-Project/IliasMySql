#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <nekoproto/serialization/reflection.hpp>
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sqlite/sqlite.hpp"
#ifdef ENABLE_MYSQL_PLUGINS
#include "ilias/mysql/mysql.hpp"
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
#include "ilias/postgres/postgres.hpp"
#endif
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

// Universal test entity that should work across all databases
struct UniversalTestEntity {
    int id = 0;
    std::string name = "";
    std::string email = "";
    int age = 0;
    bool is_active = true;
    std::chrono::system_clock::time_point created_at = {};
    std::chrono::system_clock::time_point updated_at = {};
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<UniversalTestEntity, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&UniversalTestEntity::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .length = 100
        }>(&UniversalTestEntity::name),
        "email", make_tags<SqlTags {
            .not_null = true, 
            .unique = true, 
            .length = 255
        }>(&UniversalTestEntity::email),
        "age", make_tags<SqlTags {
            .not_null = true
        }>(&UniversalTestEntity::age),
        "is_active", make_tags<SqlTags {
            .not_null = true
        }>(&UniversalTestEntity::is_active),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&UniversalTestEntity::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&UniversalTestEntity::updated_at)
    );
};
NEKO_END_NAMESPACE

// Database connection info
struct DatabaseInfo {
    std::string name;
    std::string driver;
    std::string connectionString;
    bool available = false;
};

class CrossDatabaseEndToEndTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize available databases
        initializeDatabases();
    }
    
    void TearDown() override {
        // Clean up all database connections
        for (auto& [name, db] : databases) {
            if (db) {
                // Clean up test tables
                db->exec("DROP TABLE IF EXISTS universal_test_entities");
                db.reset();
            }
        }
        databases.clear();
    }
    
    void initializeDatabases() {
        std::vector<DatabaseInfo> dbConfigs = {
            {"SQLite", "sqlite", ":memory:", false},
#ifdef ENABLE_MYSQL_PLUGINS
            {"MySQL", "mysql", "mysql://root@localhost/test", false},
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
            {"PostgreSQL", "postgres", "postgresql://postgres@localhost/test", false}
#endif
        };
        
        for (auto& config : dbConfigs) {
            auto dbResult = SqlDatabase::create(config.driver, config.connectionString);
            if (dbResult.has_value()) {
                databases[config.name] = std::move(dbResult.value());
                config.available = true;
                availableDatabases.push_back(config.name);
                
                // Clean up any existing test table
                databases[config.name]->exec("DROP TABLE IF EXISTS universal_test_entities");
            }
        }
        
        // Ensure we have at least SQLite available
        ASSERT_FALSE(databases.empty()) << "At least one database should be available for testing";
    }
    
    template<typename Tag>
    void createTestTable(const std::string& dbName) {
        auto& db = databases[dbName];
        ASSERT_TRUE(db != nullptr) << "Database " << dbName << " should be available";
        
        // Generate and execute CREATE TABLE statement
        std::string createTableSQL = Form<UniversalTestEntity, Tag>::generateCreateTable("universal_test_entities");
        auto result = db->exec(createTableSQL);
        ASSERT_TRUE(result.has_value()) << "Failed to create table in " << dbName << ": " << result.error().message();
        
        // Generate and execute index statements
        auto indexStatements = Form<UniversalTestEntity, Tag>::generateIndexStatements("universal_test_entities");
        for (const auto& indexSQL : indexStatements) {
            auto indexResult = db->exec(indexSQL);
            ASSERT_TRUE(indexResult.has_value()) << "Failed to create index in " << dbName << ": " << indexResult.error().message();
        }
    }
    
    std::map<std::string, std::unique_ptr<SqlDatabase>> databases;
    std::vector<std::string> availableDatabases;
};

// Test schema generation consistency across databases
TEST_F(CrossDatabaseEndToEndTest, SchemaGenerationConsistency) {
    // Create tables in all available databases
    for (const auto& dbName : availableDatabases) {
        if (dbName == "SQLite") {
            createTestTable<SqliteTag>(dbName);
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            createTestTable<MysqlTag>(dbName);
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            createTestTable<PostgresTag>(dbName);
        }
#endif
    }
    
    // Verify all tables were created successfully
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        
        // Test that we can describe the table structure
        std::string querySQL;
        if (dbName == "SQLite") {
            querySQL = "PRAGMA table_info(universal_test_entities)";
        } else if (dbName == "MySQL") {
            querySQL = "DESCRIBE universal_test_entities";
        } else if (dbName == "PostgreSQL") {
            querySQL = "SELECT column_name, data_type FROM information_schema.columns WHERE table_name = 'universal_test_entities'";
        }
        
        if (!querySQL.empty()) {
            auto result = db->exec(querySQL);
            EXPECT_TRUE(result.has_value()) << "Should be able to query table structure in " << dbName;
            
            if (result.has_value()) {
                auto tableInfo = result.value();
                EXPECT_TRUE(tableInfo.has_value()) << "Table should exist in " << dbName;
            }
        }
    }
}

// Test basic CRUD operations consistency
TEST_F(CrossDatabaseEndToEndTest, CRUDOperationsConsistency) {
    // Create tables in all available databases
    for (const auto& dbName : availableDatabases) {
        if (dbName == "SQLite") {
            createTestTable<SqliteTag>(dbName);
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            createTestTable<MysqlTag>(dbName);
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            createTestTable<PostgresTag>(dbName);
        }
#endif
    }
    
    // Test INSERT operations across all databases
    std::map<std::string, UniversalTestEntity> insertedEntities;
    
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        
        UniversalTestEntity entity;
        entity.name = "Cross DB User " + dbName;
        entity.email = "crossdb" + dbName + "@example.com";
        entity.age = 25;
        entity.is_active = true;
        
        // Insert using appropriate tag type
        std::optional<UniversalTestEntity> insertResult;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::insert(*db, entity);
            CO_ASSERT_VAL(result);
            insertResult = result.value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::insert(*db, entity);
            CO_ASSERT_VAL(result);
            insertResult = result.value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::insert(*db, entity);
            CO_ASSERT_VAL(result);
            insertResult = result.value();
        }
#endif
        
        ASSERT_TRUE(insertResult.has_value()) << "Insert should succeed in " << dbName;
        insertedEntities[dbName] = insertResult.value();
        
        // Verify auto-increment ID was assigned
        EXPECT_GT(insertedEntities[dbName].id, 0) << "Auto-increment ID should be assigned in " << dbName;
        
        // Verify timestamps were set
        EXPECT_NE(insertedEntities[dbName].created_at, std::chrono::system_clock::time_point{}) 
            << "created_at should be set in " << dbName;
        EXPECT_NE(insertedEntities[dbName].updated_at, std::chrono::system_clock::time_point{}) 
            << "updated_at should be set in " << dbName;
    }
    
    // Test SELECT operations across all databases
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        const auto& originalEntity = insertedEntities[dbName];
        
        std::optional<UniversalTestEntity> selectResult;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::select(*db)
                .where("email = ?", originalEntity.email)
                .first();
            CO_ASSERT_VAL(result);
            selectResult = result.value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::select(*db)
                .where("email = ?", originalEntity.email)
                .first();
            CO_ASSERT_VAL(result);
            selectResult = result.value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::select(*db)
                .where("email = ?", originalEntity.email)
                .first();
            CO_ASSERT_VAL(result);
            selectResult = result.value();
        }
#endif
        
        ASSERT_TRUE(selectResult.has_value()) << "Select should succeed in " << dbName;
        
        // Verify data consistency
        EXPECT_EQ(selectResult->id, originalEntity.id) << "ID should match in " << dbName;
        EXPECT_EQ(selectResult->name, originalEntity.name) << "Name should match in " << dbName;
        EXPECT_EQ(selectResult->email, originalEntity.email) << "Email should match in " << dbName;
        EXPECT_EQ(selectResult->age, originalEntity.age) << "Age should match in " << dbName;
        EXPECT_EQ(selectResult->is_active, originalEntity.is_active) << "is_active should match in " << dbName;
    }
}

// Test timestamp behavior consistency
TEST_F(CrossDatabaseEndToEndTest, TimestampBehaviorConsistency) {
    // Create tables in all available databases
    for (const auto& dbName : availableDatabases) {
        if (dbName == "SQLite") {
            createTestTable<SqliteTag>(dbName);
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            createTestTable<MysqlTag>(dbName);
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            createTestTable<PostgresTag>(dbName);
        }
#endif
    }
    
    std::map<std::string, UniversalTestEntity> entities;
    
    // Insert entities and record timestamps
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        
        UniversalTestEntity entity;
        entity.name = "Timestamp Test " + dbName;
        entity.email = "timestamp" + dbName + "@example.com";
        entity.age = 30;
        entity.is_active = true;
        
        auto beforeInsert = std::chrono::system_clock::now();
        
        std::optional<UniversalTestEntity> insertResult;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::insert(*db, entity);
            CO_ASSERT_VAL(result);
            insertResult = result.value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::insert(*db, entity);
            CO_ASSERT_VAL(result);
            insertResult = result.value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::insert(*db, entity);
            CO_ASSERT_VAL(result);
            insertResult = result.value();
        }
#endif
        
        auto afterInsert = std::chrono::system_clock::now();
        
        ASSERT_TRUE(insertResult.has_value()) << "Insert should succeed in " << dbName;
        entities[dbName] = insertResult.value();
        
        // Verify timestamp behavior is consistent across databases
        EXPECT_GE(entities[dbName].created_at, beforeInsert) 
            << "created_at should be after insert start in " << dbName;
        EXPECT_LE(entities[dbName].created_at, afterInsert) 
            << "created_at should be before insert end in " << dbName;
        
        EXPECT_GE(entities[dbName].updated_at, beforeInsert) 
            << "updated_at should be after insert start in " << dbName;
        EXPECT_LE(entities[dbName].updated_at, afterInsert) 
            << "updated_at should be before insert end in " << dbName;
    }
    
    // Test UPDATE timestamp behavior
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        auto& originalEntity = entities[dbName];
        
        auto beforeUpdate = std::chrono::system_clock::now();
        
        std::optional<int> updateResult;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::update(*db)
                .set("age", 31)
                .where("id = ?", originalEntity.id)
                .exec();
            CO_ASSERT_VAL(result);
            updateResult = result.value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::update(*db)
                .set("age", 31)
                .where("id = ?", originalEntity.id)
                .exec();
            CO_ASSERT_VAL(result);
            updateResult = result.value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::update(*db)
                .set("age", 31)
                .where("id = ?", originalEntity.id)
                .exec();
            CO_ASSERT_VAL(result);
            updateResult = result.value();
        }
#endif
        
        auto afterUpdate = std::chrono::system_clock::now();
        
        ASSERT_TRUE(updateResult.has_value()) << "Update should succeed in " << dbName;
        EXPECT_EQ(updateResult.value(), 1) << "Should update exactly 1 row in " << dbName;
        
        // Retrieve updated entity and verify timestamp behavior
        std::optional<UniversalTestEntity> selectResult;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::select(*db)
                .where("id = ?", originalEntity.id)
                .first();
            CO_ASSERT_VAL(result);
            selectResult = result.value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::select(*db)
                .where("id = ?", originalEntity.id)
                .first();
            CO_ASSERT_VAL(result);
            selectResult = result.value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::select(*db)
                .where("id = ?", originalEntity.id)
                .first();
            CO_ASSERT_VAL(result);
            selectResult = result.value();
        }
#endif
        
        ASSERT_TRUE(selectResult.has_value()) << "Select after update should succeed in " << dbName;
        
        // Verify timestamp update behavior is consistent
        EXPECT_EQ(selectResult->created_at, originalEntity.created_at) 
            << "created_at should not change during update in " << dbName;
        EXPECT_GT(selectResult->updated_at, originalEntity.updated_at) 
            << "updated_at should be updated in " << dbName;
        EXPECT_GE(selectResult->updated_at, beforeUpdate) 
            << "updated_at should be after update start in " << dbName;
        EXPECT_LE(selectResult->updated_at, afterUpdate) 
            << "updated_at should be before update end in " << dbName;
    }
}

// Test constraint enforcement consistency
TEST_F(CrossDatabaseEndToEndTest, ConstraintEnforcementConsistency) {
    // Create tables in all available databases
    for (const auto& dbName : availableDatabases) {
        if (dbName == "SQLite") {
            createTestTable<SqliteTag>(dbName);
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            createTestTable<MysqlTag>(dbName);
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            createTestTable<PostgresTag>(dbName);
        }
#endif
    }
    
    // Test UNIQUE constraint enforcement across all databases
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        
        // Insert first entity
        UniversalTestEntity entity1;
        entity1.name = "Constraint Test 1";
        entity1.email = "constraint@" + dbName + ".com";
        entity1.age = 25;
        entity1.is_active = true;
        
        std::optional<UniversalTestEntity> insertResult1;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::insert(*db, entity1);
            CO_ASSERT_VAL(result);
            insertResult1 = result.value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::insert(*db, entity1);
            CO_ASSERT_VAL(result);
            insertResult1 = result.value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::insert(*db, entity1);
            CO_ASSERT_VAL(result);
            insertResult1 = result.value();
        }
#endif
        
        ASSERT_TRUE(insertResult1.has_value()) << "First insert should succeed in " << dbName;
        
        // Try to insert duplicate email (should fail)
        UniversalTestEntity entity2;
        entity2.name = "Constraint Test 2";
        entity2.email = "constraint@" + dbName + ".com";  // Same email
        entity2.age = 30;
        entity2.is_active = false;
        
        bool insertFailed = false;
        if (dbName == "SQLite") {
            auto result = Form<UniversalTestEntity, SqliteTag>::insert(*db, entity2);
            insertFailed = !result.has_value();
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            auto result = Form<UniversalTestEntity, MysqlTag>::insert(*db, entity2);
            insertFailed = !result.has_value();
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            auto result = Form<UniversalTestEntity, PostgresTag>::insert(*db, entity2);
            insertFailed = !result.has_value();
        }
#endif
        
        EXPECT_TRUE(insertFailed) << "Duplicate email insert should fail in " << dbName;
    }
}

// Test performance consistency across databases
TEST_F(CrossDatabaseEndToEndTest, PerformanceConsistency) {
    // Create tables in all available databases
    for (const auto& dbName : availableDatabases) {
        if (dbName == "SQLite") {
            createTestTable<SqliteTag>(dbName);
        }
#ifdef ENABLE_MYSQL_PLUGINS
        else if (dbName == "MySQL") {
            createTestTable<MysqlTag>(dbName);
        }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
        else if (dbName == "PostgreSQL") {
            createTestTable<PostgresTag>(dbName);
        }
#endif
    }
    
    const int numOperations = 20;  // Small number for cross-database comparison
    std::map<std::string, long> performanceMetrics;
    
    // Measure insert performance for each database
    for (const auto& dbName : availableDatabases) {
        auto& db = databases[dbName];
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < numOperations; ++i) {
            UniversalTestEntity entity;
            entity.name = "Perf Test " + std::to_string(i);
            entity.email = "perf" + std::to_string(i) + "@" + dbName + ".com";
            entity.age = 20 + (i % 50);
            entity.is_active = (i % 2 == 0);
            
            bool insertSucceeded = false;
            if (dbName == "SQLite") {
                auto result = Form<UniversalTestEntity, SqliteTag>::insert(*db, entity);
                insertSucceeded = result.has_value();
            }
#ifdef ENABLE_MYSQL_PLUGINS
            else if (dbName == "MySQL") {
                auto result = Form<UniversalTestEntity, MysqlTag>::insert(*db, entity);
                insertSucceeded = result.has_value();
            }
#endif
#ifdef ENABLE_POSTGRES_PLUGINS
            else if (dbName == "PostgreSQL") {
                auto result = Form<UniversalTestEntity, PostgresTag>::insert(*db, entity);
                insertSucceeded = result.has_value();
            }
#endif
            
            ASSERT_TRUE(insertSucceeded) << "Insert " << i << " should succeed in " << dbName;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        performanceMetrics[dbName] = duration;
        
        // Performance should be reasonable for all databases
        EXPECT_LT(duration, 10000) << "Insert operations should complete within 10 seconds in " << dbName;
    }
    
    // Log performance comparison
    std::cout << "Cross-Database Performance Comparison:" << std::endl;
    for (const auto& [dbName, duration] : performanceMetrics) {
        std::cout << "  " << dbName << ": " << duration << "ms for " << numOperations << " inserts" << std::endl;
        std::cout << "    Average: " << (double)duration / numOperations << "ms per insert" << std::endl;
    }
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}