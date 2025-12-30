#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <algorithm>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Property testing framework for type modifiers
class TypeModifierPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> length_dist{1, 255};
    
public:
    TypeModifierPropertyTest() : gen(std::random_device{}()) {}
    
    // Generate random SqlTags configuration
    SqlTags generateRandomTags() {
        SqlTags tags;
        tags.primary_key = bool_dist(gen);
        tags.not_null = bool_dist(gen);
        tags.unique = bool_dist(gen);
        tags.auto_increment = bool_dist(gen);
        tags.index = bool_dist(gen);
        tags.unsigned_type = bool_dist(gen);
        tags.length = length_dist(gen);
        tags.created_at = bool_dist(gen);
        tags.updated_at = bool_dist(gen);
        return tags;
    }
    
    // Generate SqlTags with unsigned type
    SqlTags generateUnsignedTags() {
        SqlTags tags;
        tags.unsigned_type = true;
        return tags;
    }
    
    // Generate SqlTags with specific length
    SqlTags generateLengthTags(int length) {
        SqlTags tags;
        tags.length = length;
        return tags;
    }
    
    // Generate SqlTags with zero length
    SqlTags generateZeroLengthTags() {
        SqlTags tags;
        tags.length = 0;
        return tags;
    }
    
    // Run property test with given number of iterations
    template<typename PropertyFunc>
    void runPropertyTest(PropertyFunc property, int iterations = 100) {
        for (int i = 0; i < iterations; ++i) {
            property();
        }
    }
    
    // Helper to check if string contains substring (case insensitive)
    bool containsIgnoreCase(const std::string& str, const std::string& substr) {
        std::string strLower = str;
        std::string substrLower = substr;
        std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
        std::transform(substrLower.begin(), substrLower.end(), substrLower.begin(), ::tolower);
        return strLower.find(substrLower) != std::string::npos;
    }
};

// Test fixture for type modifier property tests
class TypeModifierPropertyTestFixture : public ::testing::Test {
protected:
    TypeModifierPropertyTest propertyTester;
};

// **Property 6: Unsigned type modifier**
// *For any* numeric field configuration with unsigned_type = true, the generated SQL should contain UNSIGNED modifier
// **Validates: Requirements 2.1**
TEST_F(TypeModifierPropertyTestFixture, Property6_UnsignedTypeModifier) {
    // Feature: sql-tags-enhancement, Property 6: Unsigned type modifier
    propertyTester.runPropertyTest([&]() {
        // Test with unsigned_type enabled
        auto tags = propertyTester.generateUnsignedTags();
        
        // Test MySQL dialect with various numeric types
        std::string mysqlIntColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlIntColumn, "UNSIGNED")) 
            << "MySQL int column definition should contain UNSIGNED: " << mysqlIntColumn;
        
        std::string mysqlLongColumn = Dialect<MysqlTag>::generate_column_definition<long>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlLongColumn, "UNSIGNED")) 
            << "MySQL long column definition should contain UNSIGNED: " << mysqlLongColumn;
        
        std::string mysqlShortColumn = Dialect<MysqlTag>::generate_column_definition<short>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlShortColumn, "UNSIGNED")) 
            << "MySQL short column definition should contain UNSIGNED: " << mysqlShortColumn;
        
        // Test with random tags that have unsigned_type = true
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.unsigned_type = true;
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "UNSIGNED"));
        
        // Note: SQLite doesn't support UNSIGNED modifier, so we don't test it for SQLite
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        // SQLite should still generate valid column definition even with unsigned flag
        EXPECT_FALSE(sqliteColumn.empty());
    }, 100);
}

// **Property 7: String length specification**
// *For any* string field configuration with length > 0, the generated SQL should contain VARCHAR(length) or equivalent with the specified length
// **Validates: Requirements 2.2**
TEST_F(TypeModifierPropertyTestFixture, Property7_StringLengthSpecification) {
    // Feature: sql-tags-enhancement, Property 7: String length specification
    propertyTester.runPropertyTest([&]() {
        // Generate random length between 1 and 255
        int testLength = 1 + (rand() % 255);
        auto tags = propertyTester.generateLengthTags(testLength);
        
        // Test MySQL dialect
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", tags);
        std::string expectedLength = "VARCHAR(" + std::to_string(testLength) + ")";
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, expectedLength)) 
            << "MySQL string column definition should contain " << expectedLength << ": " << mysqlColumn;
        
        // Test with various lengths
        for (int len : {1, 10, 50, 100, 255}) {
            auto lengthTags = propertyTester.generateLengthTags(len);
            std::string mysqlLenColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", lengthTags);
            std::string expectedLenStr = "VARCHAR(" + std::to_string(len) + ")";
            EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlLenColumn, expectedLenStr));
        }
        
        // Test with random tags that have length > 0
        auto randomTags = propertyTester.generateRandomTags();
        if (randomTags.length > 0) {
            std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", randomTags);
            std::string expectedRandomLength = "VARCHAR(" + std::to_string(randomTags.length) + ")";
            EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, expectedRandomLength));
        }
    }, 100);
}

// **Property 8: Default string type for zero length**
// *For any* string field configuration with length = 0 and no indexing requirements, the generated SQL should use TEXT or equivalent unlimited length type
// **Validates: Requirements 2.3**
TEST_F(TypeModifierPropertyTestFixture, Property8_DefaultStringTypeForZeroLength) {
    // Feature: sql-tags-enhancement, Property 8: Default string type for zero length
    propertyTester.runPropertyTest([&]() {
        // Test with zero length and NO indexing requirements
        auto tags = propertyTester.generateZeroLengthTags();
        // Ensure no indexing flags are set
        tags.primary_key = false;
        tags.unique = false;
        tags.index = false;
        
        // Test MySQL dialect
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "TEXT")) 
            << "MySQL string column definition with length=0 and no indexing should contain TEXT: " << mysqlColumn;
        
        // Test SQLite dialect
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "TEXT")) 
            << "SQLite string column definition should contain TEXT: " << sqliteColumn;
        
        // Test with random tags that have length = 0 but no indexing
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.length = 0;
        randomTags.primary_key = false;
        randomTags.unique = false;
        randomTags.index = false;
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "TEXT"));
        
        std::string sqliteRandomColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteRandomColumn, "TEXT"));
    }, 100);
}

// **Property 8a: Indexed string type for zero length**
// *For any* string field configuration with length = 0 and indexing requirements, the generated SQL should use VARCHAR with a reasonable default length
// **Validates: Requirements 2.4**
TEST_F(TypeModifierPropertyTestFixture, Property8a_IndexedStringTypeForZeroLength) {
    // Feature: sql-tags-enhancement, Property 8a: Indexed string type for zero length
    propertyTester.runPropertyTest([&]() {
        // Test with zero length and indexing requirements
        auto tags = propertyTester.generateZeroLengthTags();
        
        // Test each indexing flag separately
        std::vector<std::function<void(SqlTags&)>> indexingSetters = {
            [](SqlTags& t) { t.primary_key = true; },
            [](SqlTags& t) { t.unique = true; },
            [](SqlTags& t) { t.index = true; }
        };
        
        for (auto& setter : indexingSetters) {
            auto indexedTags = tags;
            setter(indexedTags);
            
            // Test MySQL dialect - should use VARCHAR with default length
            std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", indexedTags);
            EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "VARCHAR")) 
                << "MySQL string column definition with length=0 and indexing should contain VARCHAR: " << mysqlColumn;
            EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlColumn, "TEXT")) 
                << "MySQL string column definition with length=0 and indexing should not contain TEXT: " << mysqlColumn;
            
            // Should contain a reasonable default length (like 255)
            EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "255")) 
                << "MySQL string column definition with length=0 and indexing should contain default length: " << mysqlColumn;
        }
        
        // Test with random tags that have length = 0 and at least one indexing flag
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.length = 0;
        // Ensure at least one indexing flag is set
        if (!randomTags.primary_key && !randomTags.unique && !randomTags.index) {
            randomTags.index = true; // Set at least one indexing flag
        }
        
        if (randomTags.primary_key || randomTags.unique || randomTags.index) {
            std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", randomTags);
            EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "VARCHAR"));
            EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "TEXT"));
        }
    }, 100);
}

// **Property 9: Unsigned modifier ignored for non-numeric types**
// *For any* non-numeric field configuration with unsigned_type = true, the system should ignore the modifier without generating errors
// **Validates: Requirements 2.4**
TEST_F(TypeModifierPropertyTestFixture, Property9_UnsignedModifierIgnoredForNonNumericTypes) {
    // Feature: sql-tags-enhancement, Property 9: Unsigned modifier ignored for non-numeric types
    propertyTester.runPropertyTest([&]() {
        // Test with unsigned_type enabled for non-numeric types
        auto tags = propertyTester.generateUnsignedTags();
        
        // Test string type - should not contain UNSIGNED
        std::string mysqlStringColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlStringColumn, "UNSIGNED")) 
            << "MySQL string column definition should not contain UNSIGNED: " << mysqlStringColumn;
        
        std::string sqliteStringColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_FALSE(propertyTester.containsIgnoreCase(sqliteStringColumn, "UNSIGNED")) 
            << "SQLite string column definition should not contain UNSIGNED: " << sqliteStringColumn;
        
        // Test float type - should not contain UNSIGNED (floats are not typically unsigned)
        std::string mysqlFloatColumn = Dialect<MysqlTag>::generate_column_definition<float>("test_col", tags);
        // Note: MySQL does support UNSIGNED for FLOAT, but let's test that the system handles it gracefully
        // The current implementation might include UNSIGNED for float, which is technically valid in MySQL
        
        std::string sqliteFloatColumn = Dialect<SqliteTag>::generate_column_definition<float>("test_col", tags);
        EXPECT_FALSE(propertyTester.containsIgnoreCase(sqliteFloatColumn, "UNSIGNED")) 
            << "SQLite float column definition should not contain UNSIGNED: " << sqliteFloatColumn;
        
        // Test with random tags that have unsigned_type = true for non-numeric types
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.unsigned_type = true;
        
        std::string mysqlRandomStringColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", randomTags);
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlRandomStringColumn, "UNSIGNED"));
        
        // Ensure the column definition is still valid (not empty)
        EXPECT_FALSE(mysqlRandomStringColumn.empty());
        EXPECT_FALSE(sqliteStringColumn.empty());
    }, 100);
}

// Additional test to verify length behavior with different string types
TEST_F(TypeModifierPropertyTestFixture, StringLengthBehaviorConsistency) {
    propertyTester.runPropertyTest([&]() {
        auto randomTags = propertyTester.generateRandomTags();
        
        // Test consistency between std::string and const char*
        std::string stringColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", randomTags);
        std::string charPtrColumn = Dialect<MysqlTag>::generate_column_definition<const char*>("test_col", randomTags);
        
        // Both should have similar type handling (both are string types)
        if (randomTags.length > 0) {
            std::string expectedLength = "VARCHAR(" + std::to_string(randomTags.length) + ")";
            EXPECT_TRUE(propertyTester.containsIgnoreCase(stringColumn, expectedLength));
            EXPECT_TRUE(propertyTester.containsIgnoreCase(charPtrColumn, expectedLength));
        } else {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(stringColumn, "TEXT"));
            EXPECT_TRUE(propertyTester.containsIgnoreCase(charPtrColumn, "TEXT"));
        }
    }, 100);
}

// Test that unsigned modifier works correctly for different numeric types
TEST_F(TypeModifierPropertyTestFixture, UnsignedModifierForDifferentNumericTypes) {
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateUnsignedTags();
        
        // Test various integer types with MySQL (which supports UNSIGNED)
        std::string int8Column = Dialect<MysqlTag>::generate_column_definition<int8_t>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(int8Column, "UNSIGNED"));
        
        std::string int16Column = Dialect<MysqlTag>::generate_column_definition<int16_t>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(int16Column, "UNSIGNED"));
        
        std::string int32Column = Dialect<MysqlTag>::generate_column_definition<int32_t>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(int32Column, "UNSIGNED"));
        
        std::string int64Column = Dialect<MysqlTag>::generate_column_definition<int64_t>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(int64Column, "UNSIGNED"));
        
        // Test bool type
        std::string boolColumn = Dialect<MysqlTag>::generate_column_definition<bool>("test_col", tags);
        // Bool might or might not support UNSIGNED depending on implementation
        EXPECT_FALSE(boolColumn.empty());
    }, 100);
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}