#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <vector>
#include <string>
#include <random>
#include <nekoproto/serialization/reflection.hpp>
#include "ilias/sql_orm/orm_form.hpp"
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

// Simple entity without enhanced SqlTags (baseline)
struct SimpleEntity {
    int id = 0;
    std::string name = "";
    std::string email = "";
    int age = 0;
    bool is_active = true;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<SimpleEntity, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {.primary_key = true}>(&SimpleEntity::id),
        "name", make_tags<SqlTags {}>(&SimpleEntity::name),
        "email", make_tags<SqlTags {}>(&SimpleEntity::email),
        "age", make_tags<SqlTags {}>(&SimpleEntity::age),
        "is_active", make_tags<SqlTags {}>(&SimpleEntity::is_active)
    );
};
NEKO_END_NAMESPACE

// Enhanced entity with full SqlTags features
struct EnhancedEntity {
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
struct Meta<EnhancedEntity, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {
            .primary_key = true, 
            .not_null = true, 
            .auto_increment = true
        }>(&EnhancedEntity::id),
        "name", make_tags<SqlTags {
            .not_null = true, 
            .index = true,
            .length = 100
        }>(&EnhancedEntity::name),
        "email", make_tags<SqlTags {
            .not_null = true, 
            .unique = true, 
            .length = 255
        }>(&EnhancedEntity::email),
        "age", make_tags<SqlTags {
            .not_null = true,
            .unsigned_type = true
        }>(&EnhancedEntity::age),
        "is_active", make_tags<SqlTags {
            .not_null = true
        }>(&EnhancedEntity::is_active),
        "created_at", make_tags<SqlTags {
            .not_null = true, 
            .created_at = true
        }>(&EnhancedEntity::created_at),
        "updated_at", make_tags<SqlTags {
            .not_null = true, 
            .updated_at = true
        }>(&EnhancedEntity::updated_at)
    );
};
NEKO_END_NAMESPACE

class PerformanceImpactBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create separate databases for baseline and enhanced tests
        auto baselineDbResult = SqlDatabase::create("sqlite", ":memory:");
        ASSERT_TRUE(baselineDbResult.has_value()) << "Failed to create baseline database";
        baselineDb = std::move(baselineDbResult.value());
        
        auto enhancedDbResult = SqlDatabase::create("sqlite", ":memory:");
        ASSERT_TRUE(enhancedDbResult.has_value()) << "Failed to create enhanced database";
        enhancedDb = std::move(enhancedDbResult.value());
        
        // Create tables
        createBaselineTable();
        createEnhancedTable();
        
        // Initialize random number generator
        rng.seed(std::chrono::steady_clock::now().time_since_epoch().count());
    }
    
    void TearDown() override {
        if (baselineDb) {
            baselineDb.reset();
        }
        if (enhancedDb) {
            enhancedDb.reset();
        }
    }
    
    void createBaselineTable() {
        std::string createTableSQL = Form<SimpleEntity, SqliteTag>::generateCreateTable("simple_entities");
        auto result = baselineDb->exec(createTableSQL);
        ASSERT_TRUE(result.has_value()) << "Failed to create baseline table: " << result.error().message();
    }
    
    void createEnhancedTable() {
        std::string createTableSQL = Form<EnhancedEntity, SqliteTag>::generateCreateTable("enhanced_entities");
        auto result = enhancedDb->exec(createTableSQL);
        ASSERT_TRUE(result.has_value()) << "Failed to create enhanced table: " << result.error().message();
        
        // Create indexes
        auto indexStatements = Form<EnhancedEntity, SqliteTag>::generateIndexStatements("enhanced_entities");
        for (const auto& indexSQL : indexStatements) {
            auto indexResult = enhancedDb->exec(indexSQL);
            ASSERT_TRUE(indexResult.has_value()) << "Failed to create index: " << indexResult.error().message();
        }
    }
    
    std::string generateRandomString(size_t length) {
        const std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::uniform_int_distribution<> dis(0, chars.size() - 1);
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(rng)];
        }
        return result;
    }
    
    int generateRandomAge() {
        std::uniform_int_distribution<> dis(18, 80);
        return dis(rng);
    }
    
    bool generateRandomBool() {
        std::uniform_int_distribution<> dis(0, 1);
        return dis(rng) == 1;
    }
    
    std::unique_ptr<SqlDatabase> baselineDb;
    std::unique_ptr<SqlDatabase> enhancedDb;
    std::mt19937 rng;
};

// Benchmark insert operations
TEST_F(PerformanceImpactBenchmarkTest, InsertOperationsBenchmark) {
    const int numOperations = 1000;
    
    // Benchmark baseline (simple SqlTags)
    std::vector<SimpleEntity> baselineEntities;
    baselineEntities.reserve(numOperations);
    
    for (int i = 0; i < numOperations; ++i) {
        SimpleEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@baseline.com";
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        baselineEntities.push_back(entity);
    }
    
    auto baselineStartTime = std::chrono::high_resolution_clock::now();
    
    for (auto& entity : baselineEntities) {
        auto result = Form<SimpleEntity, SqliteTag>::insert(*baselineDb, entity);
        ASSERT_TRUE(result.has_value()) << "Baseline insert should succeed";
    }
    
    auto baselineEndTime = std::chrono::high_resolution_clock::now();
    auto baselineDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        baselineEndTime - baselineStartTime).count();
    
    // Benchmark enhanced (full SqlTags features)
    std::vector<EnhancedEntity> enhancedEntities;
    enhancedEntities.reserve(numOperations);
    
    for (int i = 0; i < numOperations; ++i) {
        EnhancedEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@enhanced.com";
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        enhancedEntities.push_back(entity);
    }
    
    auto enhancedStartTime = std::chrono::high_resolution_clock::now();
    
    for (auto& entity : enhancedEntities) {
        auto result = Form<EnhancedEntity, SqliteTag>::insert(*enhancedDb, entity);
        ASSERT_TRUE(result.has_value()) << "Enhanced insert should succeed";
    }
    
    auto enhancedEndTime = std::chrono::high_resolution_clock::now();
    auto enhancedDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        enhancedEndTime - enhancedStartTime).count();
    
    // Calculate performance impact
    double performanceRatio = (double)enhancedDuration / baselineDuration;
    double performanceOverhead = ((double)enhancedDuration - baselineDuration) / baselineDuration * 100.0;
    
    // Log performance metrics
    std::cout << "Insert Operations Benchmark (" << numOperations << " operations):" << std::endl;
    std::cout << "  Baseline (Simple SqlTags): " << baselineDuration << " μs" << std::endl;
    std::cout << "  Enhanced (Full SqlTags):   " << enhancedDuration << " μs" << std::endl;
    std::cout << "  Performance Ratio:         " << performanceRatio << "x" << std::endl;
    std::cout << "  Performance Overhead:      " << performanceOverhead << "%" << std::endl;
    std::cout << "  Baseline avg per op:       " << (double)baselineDuration / numOperations << " μs" << std::endl;
    std::cout << "  Enhanced avg per op:       " << (double)enhancedDuration / numOperations << " μs" << std::endl;
    
    // Performance expectations - enhanced should not be more than 50% slower
    EXPECT_LT(performanceRatio, 1.5) 
        << "Enhanced SqlTags should not add more than 50% overhead to insert operations";
    
    // Both should complete within reasonable time
    EXPECT_LT(baselineDuration, 10000000) << "Baseline inserts should complete within 10 seconds";
    EXPECT_LT(enhancedDuration, 15000000) << "Enhanced inserts should complete within 15 seconds";
}

// Benchmark select operations
TEST_F(PerformanceImpactBenchmarkTest, SelectOperationsBenchmark) {
    const int numInserts = 500;
    const int numSelects = 200;
    
    // Populate baseline database
    for (int i = 0; i < numInserts; ++i) {
        SimpleEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@baseline.com";
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        
        auto result = Form<SimpleEntity, SqliteTag>::insert(*baselineDb, entity);
        ASSERT_TRUE(result.has_value()) << "Baseline insert should succeed";
    }
    
    // Populate enhanced database
    for (int i = 0; i < numInserts; ++i) {
        EnhancedEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@enhanced.com";
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        
        auto result = Form<EnhancedEntity, SqliteTag>::insert(*enhancedDb, entity);
        ASSERT_TRUE(result.has_value()) << "Enhanced insert should succeed";
    }
    
    // Benchmark baseline selects
    std::uniform_int_distribution<> selectDis(0, numInserts - 1);
    
    auto baselineStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numSelects; ++i) {
        int randomId = selectDis(rng);
        std::string email = "user" + std::to_string(randomId) + "@baseline.com";
        
        auto result = Form<SimpleEntity, SqliteTag>::select(*baselineDb)
            .where("email = ?", email)
            .first();
        ASSERT_TRUE(result.has_value()) << "Baseline select should succeed";
    }
    
    auto baselineEndTime = std::chrono::high_resolution_clock::now();
    auto baselineDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        baselineEndTime - baselineStartTime).count();
    
    // Benchmark enhanced selects
    auto enhancedStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numSelects; ++i) {
        int randomId = selectDis(rng);
        std::string email = "user" + std::to_string(randomId) + "@enhanced.com";
        
        auto result = Form<EnhancedEntity, SqliteTag>::select(*enhancedDb)
            .where("email = ?", email)
            .first();
        ASSERT_TRUE(result.has_value()) << "Enhanced select should succeed";
    }
    
    auto enhancedEndTime = std::chrono::high_resolution_clock::now();
    auto enhancedDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        enhancedEndTime - enhancedStartTime).count();
    
    // Calculate performance impact
    double performanceRatio = (double)enhancedDuration / baselineDuration;
    double performanceOverhead = ((double)enhancedDuration - baselineDuration) / baselineDuration * 100.0;
    
    // Log performance metrics
    std::cout << "Select Operations Benchmark (" << numSelects << " operations):" << std::endl;
    std::cout << "  Baseline (Simple SqlTags): " << baselineDuration << " μs" << std::endl;
    std::cout << "  Enhanced (Full SqlTags):   " << enhancedDuration << " μs" << std::endl;
    std::cout << "  Performance Ratio:         " << performanceRatio << "x" << std::endl;
    std::cout << "  Performance Overhead:      " << performanceOverhead << "%" << std::endl;
    std::cout << "  Baseline avg per op:       " << (double)baselineDuration / numSelects << " μs" << std::endl;
    std::cout << "  Enhanced avg per op:       " << (double)enhancedDuration / numSelects << " μs" << std::endl;
    
    // Enhanced might be faster due to indexes, but should not be significantly slower
    EXPECT_LT(performanceRatio, 2.0) 
        << "Enhanced SqlTags should not add more than 100% overhead to select operations";
    
    // Both should complete within reasonable time
    EXPECT_LT(baselineDuration, 5000000) << "Baseline selects should complete within 5 seconds";
    EXPECT_LT(enhancedDuration, 10000000) << "Enhanced selects should complete within 10 seconds";
}

// Benchmark update operations
TEST_F(PerformanceImpactBenchmarkTest, UpdateOperationsBenchmark) {
    const int numInserts = 200;
    const int numUpdates = 100;
    
    // Populate and benchmark baseline updates
    std::vector<int> baselineIds;
    for (int i = 0; i < numInserts; ++i) {
        SimpleEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@baseline.com";
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        
        auto result = Form<SimpleEntity, SqliteTag>::insert(*baselineDb, entity);
        ASSERT_TRUE(result.has_value()) << "Baseline insert should succeed";
        baselineIds.push_back(result.value().id);
    }
    
    std::uniform_int_distribution<> updateDis(0, baselineIds.size() - 1);
    
    auto baselineStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numUpdates; ++i) {
        int randomIndex = updateDis(rng);
        int id = baselineIds[randomIndex];
        
        auto result = Form<SimpleEntity, SqliteTag>::update(*baselineDb)
            .set("age", generateRandomAge())
            .set("is_active", generateRandomBool())
            .where("id = ?", id)
            .exec();
        ASSERT_TRUE(result.has_value()) << "Baseline update should succeed";
    }
    
    auto baselineEndTime = std::chrono::high_resolution_clock::now();
    auto baselineDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        baselineEndTime - baselineStartTime).count();
    
    // Populate and benchmark enhanced updates
    std::vector<int> enhancedIds;
    for (int i = 0; i < numInserts; ++i) {
        EnhancedEntity entity;
        entity.name = "User " + std::to_string(i);
        entity.email = "user" + std::to_string(i) + "@enhanced.com";
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        
        auto result = Form<EnhancedEntity, SqliteTag>::insert(*enhancedDb, entity);
        ASSERT_TRUE(result.has_value()) << "Enhanced insert should succeed";
        enhancedIds.push_back(result.value().id);
    }
    
    auto enhancedStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numUpdates; ++i) {
        int randomIndex = updateDis(rng);
        int id = enhancedIds[randomIndex];
        
        auto result = Form<EnhancedEntity, SqliteTag>::update(*enhancedDb)
            .set("age", generateRandomAge())
            .set("is_active", generateRandomBool())
            .where("id = ?", id)
            .exec();
        ASSERT_TRUE(result.has_value()) << "Enhanced update should succeed";
    }
    
    auto enhancedEndTime = std::chrono::high_resolution_clock::now();
    auto enhancedDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        enhancedEndTime - enhancedStartTime).count();
    
    // Calculate performance impact
    double performanceRatio = (double)enhancedDuration / baselineDuration;
    double performanceOverhead = ((double)enhancedDuration - baselineDuration) / baselineDuration * 100.0;
    
    // Log performance metrics
    std::cout << "Update Operations Benchmark (" << numUpdates << " operations):" << std::endl;
    std::cout << "  Baseline (Simple SqlTags): " << baselineDuration << " μs" << std::endl;
    std::cout << "  Enhanced (Full SqlTags):   " << enhancedDuration << " μs" << std::endl;
    std::cout << "  Performance Ratio:         " << performanceRatio << "x" << std::endl;
    std::cout << "  Performance Overhead:      " << performanceOverhead << "%" << std::endl;
    std::cout << "  Baseline avg per op:       " << (double)baselineDuration / numUpdates << " μs" << std::endl;
    std::cout << "  Enhanced avg per op:       " << (double)enhancedDuration / numUpdates << " μs" << std::endl;
    
    // Enhanced updates include timestamp management, so some overhead is expected
    EXPECT_LT(performanceRatio, 2.0) 
        << "Enhanced SqlTags should not add more than 100% overhead to update operations";
    
    // Both should complete within reasonable time
    EXPECT_LT(baselineDuration, 5000000) << "Baseline updates should complete within 5 seconds";
    EXPECT_LT(enhancedDuration, 10000000) << "Enhanced updates should complete within 10 seconds";
}

// Benchmark schema generation performance
TEST_F(PerformanceImpactBenchmarkTest, SchemaGenerationBenchmark) {
    const int numGenerations = 1000;
    
    // Benchmark baseline schema generation
    auto baselineStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numGenerations; ++i) {
        std::string tableName = "test_table_" + std::to_string(i);
        std::string createTableSQL = Form<SimpleEntity, SqliteTag>::generateCreateTable(tableName);
        EXPECT_FALSE(createTableSQL.empty()) << "Should generate CREATE TABLE SQL";
    }
    
    auto baselineEndTime = std::chrono::high_resolution_clock::now();
    auto baselineDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        baselineEndTime - baselineStartTime).count();
    
    // Benchmark enhanced schema generation
    auto enhancedStartTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numGenerations; ++i) {
        std::string tableName = "test_table_" + std::to_string(i);
        std::string createTableSQL = Form<EnhancedEntity, SqliteTag>::generateCreateTable(tableName);
        EXPECT_FALSE(createTableSQL.empty()) << "Should generate CREATE TABLE SQL";
        
        auto indexStatements = Form<EnhancedEntity, SqliteTag>::generateIndexStatements(tableName);
        // Index statements are optional, so we don't assert their presence
    }
    
    auto enhancedEndTime = std::chrono::high_resolution_clock::now();
    auto enhancedDuration = std::chrono::duration_cast<std::chrono::microseconds>(
        enhancedEndTime - enhancedStartTime).count();
    
    // Calculate performance impact
    double performanceRatio = (double)enhancedDuration / baselineDuration;
    double performanceOverhead = ((double)enhancedDuration - baselineDuration) / baselineDuration * 100.0;
    
    // Log performance metrics
    std::cout << "Schema Generation Benchmark (" << numGenerations << " generations):" << std::endl;
    std::cout << "  Baseline (Simple SqlTags): " << baselineDuration << " μs" << std::endl;
    std::cout << "  Enhanced (Full SqlTags):   " << enhancedDuration << " μs" << std::endl;
    std::cout << "  Performance Ratio:         " << performanceRatio << "x" << std::endl;
    std::cout << "  Performance Overhead:      " << performanceOverhead << "%" << std::endl;
    std::cout << "  Baseline avg per gen:      " << (double)baselineDuration / numGenerations << " μs" << std::endl;
    std::cout << "  Enhanced avg per gen:      " << (double)enhancedDuration / numGenerations << " μs" << std::endl;
    
    // Schema generation is typically done once, so higher overhead is acceptable
    EXPECT_LT(performanceRatio, 5.0) 
        << "Enhanced SqlTags should not add more than 400% overhead to schema generation";
    
    // Both should complete within reasonable time
    EXPECT_LT(baselineDuration, 1000000) << "Baseline schema generation should complete within 1 second";
    EXPECT_LT(enhancedDuration, 5000000) << "Enhanced schema generation should complete within 5 seconds";
}

// Memory usage comparison test
TEST_F(PerformanceImpactBenchmarkTest, MemoryUsageComparison) {
    const int numEntities = 1000;
    
    // Create baseline entities
    std::vector<SimpleEntity> baselineEntities;
    baselineEntities.reserve(numEntities);
    
    for (int i = 0; i < numEntities; ++i) {
        SimpleEntity entity;
        entity.name = generateRandomString(20);
        entity.email = generateRandomString(30);
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        baselineEntities.push_back(entity);
    }
    
    // Create enhanced entities
    std::vector<EnhancedEntity> enhancedEntities;
    enhancedEntities.reserve(numEntities);
    
    for (int i = 0; i < numEntities; ++i) {
        EnhancedEntity entity;
        entity.name = generateRandomString(20);
        entity.email = generateRandomString(30);
        entity.age = generateRandomAge();
        entity.is_active = generateRandomBool();
        entity.created_at = std::chrono::system_clock::now();
        entity.updated_at = std::chrono::system_clock::now();
        enhancedEntities.push_back(entity);
    }
    
    // Calculate memory usage (approximate)
    size_t baselineMemory = sizeof(SimpleEntity) * numEntities;
    size_t enhancedMemory = sizeof(EnhancedEntity) * numEntities;
    
    double memoryRatio = (double)enhancedMemory / baselineMemory;
    double memoryOverhead = ((double)enhancedMemory - baselineMemory) / baselineMemory * 100.0;
    
    // Log memory usage
    std::cout << "Memory Usage Comparison (" << numEntities << " entities):" << std::endl;
    std::cout << "  Baseline entity size:      " << sizeof(SimpleEntity) << " bytes" << std::endl;
    std::cout << "  Enhanced entity size:      " << sizeof(EnhancedEntity) << " bytes" << std::endl;
    std::cout << "  Baseline total memory:     " << baselineMemory << " bytes" << std::endl;
    std::cout << "  Enhanced total memory:     " << enhancedMemory << " bytes" << std::endl;
    std::cout << "  Memory Ratio:              " << memoryRatio << "x" << std::endl;
    std::cout << "  Memory Overhead:           " << memoryOverhead << "%" << std::endl;
    
    // Memory overhead is expected due to additional timestamp fields
    // But it should be reasonable (timestamp fields add ~16 bytes each)
    EXPECT_LT(memoryRatio, 3.0) 
        << "Enhanced entities should not use more than 3x the memory of baseline entities";
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}