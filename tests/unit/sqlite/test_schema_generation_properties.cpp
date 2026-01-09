#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <algorithm>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Property testing framework for schema generation
class SchemaGenerationPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> length_dist{1, 255};
    
public:
    SchemaGenerationPropertyTest() : gen(std::random_device{}()) {}
    
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
    
    // Generate SqlTags with specific constraint
    SqlTags generatePrimaryKeyTags() {
        SqlTags tags;
        tags.primary_key = true;
        return tags;
    }
    
    SqlTags generateNotNullTags() {
        SqlTags tags;
        tags.not_null = true;
        return tags;
    }
    
    SqlTags generateUniqueTags() {
        SqlTags tags;
        tags.unique = true;
        return tags;
    }
    
    SqlTags generateAutoIncrementTags() {
        SqlTags tags;
        tags.auto_increment = true;
        return tags;
    }
    
    SqlTags generateIndexTags() {
        SqlTags tags;
        tags.index = true;
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

// Test fixture for schema generation property tests
class SchemaGenerationPropertyTestFixture : public ::testing::Test {
protected:
    SchemaGenerationPropertyTest propertyTester;
};

// **Property 1: Primary key constraint generation**
// *For any* field configuration with primary_key = true, the generated SQL schema should contain PRIMARY KEY constraint
// **Validates: Requirements 1.1**
TEST_F(SchemaGenerationPropertyTestFixture, Property1_PrimaryKeyConstraintGeneration) {
    // Feature: sql-tags-enhancement, Property 1: Primary key constraint generation
    propertyTester.runPropertyTest([&]() {
        // Test with primary key enabled
        auto tags = propertyTester.generatePrimaryKeyTags();
        
        // Test SQLite dialect
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "PRIMARY KEY")) 
            << "SQLite column definition should contain PRIMARY KEY: " << sqliteColumn;
        
        // Test MySQL dialect
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "PRIMARY KEY")) 
            << "MySQL column definition should contain PRIMARY KEY: " << mysqlColumn;
        
        // Test PostgreSQL dialect
        std::string postgresColumn = Dialect<PostgresTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "PRIMARY KEY")) 
            << "PostgreSQL column definition should contain PRIMARY KEY: " << postgresColumn;
        
        // Test with random tags that have primary_key = true
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.primary_key = true;
        
        std::string sqliteRandomColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteRandomColumn, "PRIMARY KEY"));
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "PRIMARY KEY"));
        
        std::string postgresRandomColumn = Dialect<PostgresTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresRandomColumn, "PRIMARY KEY"));
    }, 100);
}

// **Property 2: Not null constraint generation**
// *For any* field configuration with not_null = true, the generated SQL schema should contain NOT NULL constraint
// **Validates: Requirements 1.2**
TEST_F(SchemaGenerationPropertyTestFixture, Property2_NotNullConstraintGeneration) {
    // Feature: sql-tags-enhancement, Property 2: Not null constraint generation
    propertyTester.runPropertyTest([&]() {
        // Test with not_null enabled
        auto tags = propertyTester.generateNotNullTags();
        
        // Test SQLite dialect
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "NOT NULL")) 
            << "SQLite column definition should contain NOT NULL: " << sqliteColumn;
        
        // Test MySQL dialect
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "NOT NULL")) 
            << "MySQL column definition should contain NOT NULL: " << mysqlColumn;
        
        // Test with random tags that have not_null = true
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.not_null = true;
        
        std::string sqliteRandomColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteRandomColumn, "NOT NULL"));
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "NOT NULL"));
    }, 100);
}

// **Property 3: Unique constraint generation**
// *For any* field configuration with unique = true, the generated SQL schema should contain UNIQUE constraint
// **Validates: Requirements 1.3**
TEST_F(SchemaGenerationPropertyTestFixture, Property3_UniqueConstraintGeneration) {
    // Feature: sql-tags-enhancement, Property 3: Unique constraint generation
    propertyTester.runPropertyTest([&]() {
        // Test with unique enabled (but not primary key to avoid conflicts)
        auto tags = propertyTester.generateUniqueTags();
        tags.primary_key = false; // Ensure no conflict with primary key
        
        // Test SQLite dialect
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "UNIQUE")) 
            << "SQLite column definition should contain UNIQUE: " << sqliteColumn;
        
        // Test MySQL dialect
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "UNIQUE")) 
            << "MySQL column definition should contain UNIQUE: " << mysqlColumn;
        
        // Test with random tags that have unique = true but not primary_key
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.unique = true;
        randomTags.primary_key = false;
        
        std::string sqliteRandomColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteRandomColumn, "UNIQUE"));
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "UNIQUE"));
    }, 100);
}

// **Property 4: Auto increment constraint generation**
// *For any* numeric field configuration with auto_increment = true, the generated SQL schema should contain the appropriate auto-increment syntax for the target database
// **Validates: Requirements 1.4**
TEST_F(SchemaGenerationPropertyTestFixture, Property4_AutoIncrementConstraintGeneration) {
    // Feature: sql-tags-enhancement, Property 4: Auto increment constraint generation
    propertyTester.runPropertyTest([&]() {
        // Test with auto_increment enabled
        auto tags = propertyTester.generateAutoIncrementTags();
        
        // Test SQLite dialect - should contain AUTOINCREMENT
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        if (tags.primary_key) {
            // SQLite only supports AUTOINCREMENT with PRIMARY KEY
            EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "AUTOINCREMENT")) 
                << "SQLite column definition should contain AUTOINCREMENT when primary_key is true: " << sqliteColumn;
        }
        
        // Test MySQL dialect - should contain AUTO_INCREMENT
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "AUTO_INCREMENT")) 
            << "MySQL column definition should contain AUTO_INCREMENT: " << mysqlColumn;
        
        // Test with random tags that have auto_increment = true
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.auto_increment = true;
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "AUTO_INCREMENT"));
        
        // For SQLite, test with primary key enabled
        randomTags.primary_key = true;
        std::string sqliteRandomColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteRandomColumn, "AUTOINCREMENT"));
    }, 100);
}

// **Property 5: Index statement generation**
// *For any* field configuration with index = true, the system should generate appropriate CREATE INDEX statements
// **Validates: Requirements 1.5**
TEST_F(SchemaGenerationPropertyTestFixture, Property5_IndexStatementGeneration) {
    // Feature: sql-tags-enhancement, Property 5: Index statement generation
    propertyTester.runPropertyTest([&]() {
        // Test with index enabled
        auto tags = propertyTester.generateIndexTags();
        tags.primary_key = false; // Avoid primary key which already creates index
        tags.unique = false; // Avoid unique which already creates index
        
        // Test MySQL dialect - should contain KEY for index
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "KEY")) 
            << "MySQL column definition should contain KEY for index: " << mysqlColumn;
        
        // Test with random tags that have index = true but not primary_key or unique
        auto randomTags = propertyTester.generateRandomTags();
        randomTags.index = true;
        randomTags.primary_key = false;
        randomTags.unique = false;
        
        std::string mysqlRandomColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", randomTags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlRandomColumn, "KEY"));
        
        // Note: SQLite doesn't generate inline index syntax in column definition
        // Index creation would be handled separately via CREATE INDEX statements
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        // SQLite column definition doesn't include index syntax inline
        EXPECT_FALSE(sqliteColumn.empty()); // Just ensure it generates something valid
    }, 100);
}

// Additional test to verify that constraints don't appear when flags are false
TEST_F(SchemaGenerationPropertyTestFixture, ConstraintsAbsentWhenFlagsAreFalse) {
    propertyTester.runPropertyTest([&]() {
        SqlTags tags; // All flags are false by default
        
        // Test SQLite dialect
        std::string sqliteColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_FALSE(propertyTester.containsIgnoreCase(sqliteColumn, "PRIMARY KEY"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(sqliteColumn, "NOT NULL"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(sqliteColumn, "UNIQUE"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(sqliteColumn, "AUTOINCREMENT"));
        
        // Test MySQL dialect
        std::string mysqlColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlColumn, "PRIMARY KEY"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlColumn, "NOT NULL"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlColumn, "UNIQUE"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlColumn, "AUTO_INCREMENT"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(mysqlColumn, "KEY"));
    }, 100);
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}