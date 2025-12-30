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
#include "ilias/mysql/mysql.hpp"
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

// Comprehensive test entity with all SqlTags features for MySQL
struct MySQLTestEntity {
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
struct Meta<MySQLTestEntity, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&MySQLTestEntity::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .index = true,
            .length = 100
        }>(&MySQLTestEntity::name),
        "email", make_tags<SqlTags {
            .not_null = true, 
            .unique = true, 
            .length = 255
        }>(&MySQLTestEntity::email),
        "description", make_tags<SqlTags {
            .length = 0  // Should use TEXT type
        }>(&MySQLTestEntity::description),
        "age", make_tags<SqlTags {
            .not_null = true,
            .unsigned_type = true
        }>(&MySQLTestEntity::age),
        "balance", make_tags<SqlTags {
            .not_null = true,
            .unsigned_type = true
        }>(&MySQLTestEntity::balance),
        "is_active", make_tags<SqlTags {
            .not_null = true
        }>(&MySQLTestEntity::is_active),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&MySQLTestEntity::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&MySQLTestEntity::updated_at)
    );
};
NEKO_END_NAMESPACE

class MySQLEndToEndIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Try to create MySQL database connection
        // Note: This test requires a running MySQL server
        // For CI/CD environments, this might need to be skipped or use a test container
        
        // Try common MySQL connection strings
        std::vector<std::string> connectionStrings = {
            "mysql://root@localhost/test",
            "mysql://test:test@localhost/test",
            "mysql://root:@localhost/test"
        };
        
        bool connected = false;
        for (const auto& connStr : connectionStrings) {
            auto dbResult = SqlDatabase::create("mysql", connStr);
            if (dbResult.has_value()) {
                db = std::move(dbResult.value());
                connected = true;
                break;
            }
        }
        
        if (!connected) {
            GTEST_SKIP() << "MySQL database not available for testing. Skipping MySQL end-to-end tests.";
            return;
        }
        
        // Drop table if it exists and create fresh table
        db->exec("DROP TABLE IF EXISTS mysql_test_entities");
        createTestTable();
    }
    
    void TearDown() override {
        if (db) {
            // Clean up test table
            db->exec("DROP TABLE IF EXISTS mysql_test_entities");
            db.reset();
        }
    }
    
    void createTestTable() {
        // Generate CREATE TABLE statement using enhanced SqlTags for MySQL
        std::string createTableSQL = Form<MySQLTestEntity, MysqlTag>::generateCreateTable("mysql_test_entities");
        
        // Execute CREATE TABLE
        auto result = db->exec(createTableSQL);
        ASSERT_TRUE(result.has_value()) << "Failed to create table: " << result.error().message();
        
        // Generate and execute index statements
        auto indexStatements = Form<MySQLTestEntity, MysqlTag>::generateIndexStatements("mysql_test_entities");
        for (const auto& indexSQL : indexStatements) {
            auto indexResult = db->exec(indexSQL);
            ASSERT_TRUE(indexResult.has_value()) << "Failed to create index: " << indexResult.error().message();
        }
    }
    
    std::unique_ptr<SqlDatabase> db;
};

// Test MySQL-specific schema generation
TEST_F(MySQLEndToEndIntegrationTest, MySQLSchemaGeneration) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    // Verify table was created with MySQL-specific syntax
    auto tableInfoResult = db->exec("SHOW CREATE TABLE mysql_test_entities");
    ASSERT_TRUE(tableInfoResult.has_value()) << "Failed to get table info: " << tableInfoResult.error().message();
    
    auto tableInfo = tableInfoResult.value();
    ASSERT_TRUE(tableInfo.has_value()) << "Table info should not be empty";
    
    if (tableInfo->next()) {
        auto createTableSQL = tableInfo->get<std::string>("Create Table");
        ASSERT_TRUE(createTableSQL.has_value()) << "Should get CREATE TABLE SQL";
        
        std::string sql = createTableSQL.value();
        
        // Verify MySQL-specific syntax elements
        EXPECT_TRUE(sql.find("AUTO_INCREMENT") != std::string::npos) 
            << "Should use MySQL AUTO_INCREMENT syntax";
        EXPECT_TRUE(sql.find("PRIMARY KEY") != std::string::npos) 
            << "Should have PRIMARY KEY constraint";
        EXPECT_TRUE(sql.find("UNIQUE KEY") != std::string::npos || sql.find("UNIQUE") != std::string::npos) 
            << "Should have UNIQUE constraint for email";
        EXPECT_TRUE(sql.find("UNSIGNED") != std::string::npos) 
            << "Should have UNSIGNED modifiers for numeric fields";
        EXPECT_TRUE(sql.find("DEFAULT CURRENT_TIMESTAMP") != std::string::npos) 
            << "Should have timestamp defaults";
        EXPECT_TRUE(sql.find("ON UPDATE CURRENT_TIMESTAMP") != std::string::npos) 
            << "Should have timestamp update behavior";
    }
}

// Test MySQL timestamp behavior
TEST_F(MySQLEndToEndIntegrationTest, MySQLTimestampBehavior) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    // Insert entity and verify MySQL timestamp handling
    MySQLTestEntity entity;
    entity.name = "MySQL Test User";
    entity.email = "mysql@example.com";
    entity.description = "MySQL test description";
    entity.age = 25;
    entity.balance = 100.50;
    entity.is_active = true;
    
    auto beforeInsert = std::chrono::system_clock::now();
    
    auto insertResult = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity);
    CO_ASSERT_VAL(insertResult);
    
    auto afterInsert = std::chrono::system_clock::now();
    
    // Retrieve and verify timestamps
    auto selectResult = Form<MySQLTestEntity, MysqlTag>::select(*db)
        .where("email = ?", entity.email)
        .first();
    CO_ASSERT_VAL(selectResult);
    
    auto retrievedEntity = selectResult.value();
    
    // Verify MySQL timestamp behavior
    EXPECT_NE(retrievedEntity.created_at, std::chrono::system_clock::time_point{}) 
        << "MySQL should automatically set created_at";
    EXPECT_NE(retrievedEntity.updated_at, std::chrono::system_clock::time_point{}) 
        << "MySQL should automatically set updated_at";
    
    EXPECT_GE(retrievedEntity.created_at, beforeInsert) 
        << "created_at should be within expected range";
    EXPECT_LE(retrievedEntity.created_at, afterInsert) 
        << "created_at should be within expected range";
}

// Test MySQL constraint enforcement
TEST_F(MySQLEndToEndIntegrationTest, MySQLConstraintEnforcement) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    // Test UNIQUE constraint with MySQL
    MySQLTestEntity entity1;
    entity1.name = "MySQL User 1";
    entity1.email = "unique@mysql.com";
    entity1.age = 25;
    entity1.balance = 100.0;
    
    auto insertResult1 = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity1);
    CO_ASSERT_VAL(insertResult1);
    
    // Try duplicate email
    MySQLTestEntity entity2;
    entity2.name = "MySQL User 2";
    entity2.email = "unique@mysql.com";  // Same email
    entity2.age = 30;
    entity2.balance = 200.0;
    
    auto insertResult2 = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity2);
    EXPECT_FALSE(insertResult2.has_value()) 
        << "MySQL should enforce UNIQUE constraint";
    
    if (!insertResult2.has_value()) {
        auto error = insertResult2.error();
        std::string errorMsg = error.message();
        
        // MySQL should provide specific error about duplicate entry
        EXPECT_TRUE(errorMsg.find("Duplicate") != std::string::npos || 
                   errorMsg.find("duplicate") != std::string::npos ||
                   errorMsg.find("UNIQUE") != std::string::npos)
            << "MySQL error should indicate duplicate key: " << errorMsg;
    }
}

// Test MySQL unsigned type handling
TEST_F(MySQLEndToEndIntegrationTest, MySQLUnsignedTypeHandling) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    // Test that MySQL properly handles unsigned types
    MySQLTestEntity entity;
    entity.name = "Unsigned Test";
    entity.email = "unsigned@mysql.com";
    entity.age = 4294967295U;  // Large unsigned value
    entity.balance = 999999.99;
    entity.is_active = true;
    
    auto insertResult = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity);
    CO_ASSERT_VAL(insertResult);
    
    // Retrieve and verify unsigned values are preserved
    auto selectResult = Form<MySQLTestEntity, MysqlTag>::select(*db)
        .where("email = ?", entity.email)
        .first();
    CO_ASSERT_VAL(selectResult);
    
    auto retrievedEntity = selectResult.value();
    EXPECT_EQ(retrievedEntity.age, entity.age) 
        << "MySQL should preserve large unsigned integer values";
    EXPECT_DOUBLE_EQ(retrievedEntity.balance, entity.balance) 
        << "MySQL should preserve unsigned double values";
}

// Test MySQL performance characteristics
TEST_F(MySQLEndToEndIntegrationTest, MySQLPerformanceCharacteristics) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    const int numOperations = 50;  // Smaller number for network database
    
    // Measure MySQL insert performance
    auto insertStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numOperations; ++i) {
        MySQLTestEntity entity;
        entity.name = "MySQL Perf User " + std::to_string(i);
        entity.email = "mysqlperf" + std::to_string(i) + "@example.com";
        entity.description = "MySQL performance test " + std::to_string(i);
        entity.age = 20 + (i % 50);
        entity.balance = 100.0 + i;
        entity.is_active = (i % 2 == 0);
        
        auto insertResult = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity);
        ASSERT_TRUE(insertResult.has_value()) << "MySQL insert " << i << " should succeed";
    }
    
    auto insertEndTime = std::chrono::high_resolution_clock::now();
    auto insertDuration = std::chrono::duration_cast<std::chrono::milliseconds>(
        insertEndTime - insertStartTime).count();
    
    // MySQL performance should be reasonable (allowing for network overhead)
    EXPECT_LT(insertDuration, 10000) << "MySQL insert operations should complete within 10 seconds";
    
    std::cout << "MySQL Performance Metrics:" << std::endl;
    std::cout << "  Insert " << numOperations << " records: " << insertDuration << "ms" << std::endl;
    std::cout << "  Average insert time: " << (double)insertDuration / numOperations << "ms per record" << std::endl;
}

// Test MySQL index utilization
TEST_F(MySQLEndToEndIntegrationTest, MySQLIndexUtilization) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    // Insert test data
    for (int i = 0; i < 10; ++i) {
        MySQLTestEntity entity;
        entity.name = "Index Test User " + std::to_string(i);
        entity.email = "indextest" + std::to_string(i) + "@mysql.com";
        entity.age = 20 + i;
        entity.balance = 100.0 * i;
        entity.is_active = (i % 2 == 0);
        
        auto insertResult = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity);
        CO_ASSERT_VAL(insertResult);
    }
    
    // Test that indexes are being used for queries
    auto explainResult = db->exec("EXPLAIN SELECT * FROM mysql_test_entities WHERE name = 'Index Test User 5'");
    ASSERT_TRUE(explainResult.has_value()) << "EXPLAIN query should work";
    
    auto explainInfo = explainResult.value();
    if (explainInfo.has_value() && explainInfo->next()) {
        auto keyUsed = explainInfo->get<std::string>("key");
        if (keyUsed.has_value()) {
            // If an index is used, it should not be NULL
            EXPECT_NE(keyUsed.value(), "NULL") 
                << "MySQL should use index for name column queries";
        }
    }
}

// Test MySQL error handling specifics
TEST_F(MySQLEndToEndIntegrationTest, MySQLErrorHandling) {
    if (!db) {
        GTEST_SKIP() << "MySQL database not available";
    }
    
    // Test MySQL-specific error handling
    MySQLTestEntity entity;
    entity.name = "Error Test";
    entity.email = "error@mysql.com";
    entity.age = 25;
    entity.balance = 100.0;
    
    auto insertResult = Form<MySQLTestEntity, MysqlTag>::insert(*db, entity);
    CO_ASSERT_VAL(insertResult);
    
    // Try to violate NOT NULL constraint
    auto updateResult = db->exec("UPDATE mysql_test_entities SET name = NULL WHERE email = 'error@mysql.com'");
    EXPECT_FALSE(updateResult.has_value()) 
        << "MySQL should prevent NULL values in NOT NULL columns";
    
    if (!updateResult.has_value()) {
        auto error = updateResult.error();
        std::string errorMsg = error.message();
        
        // MySQL should provide specific error about NULL constraint
        EXPECT_TRUE(errorMsg.find("NULL") != std::string::npos || 
                   errorMsg.find("null") != std::string::npos)
            << "MySQL error should mention NULL constraint: " << errorMsg;
    }
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}