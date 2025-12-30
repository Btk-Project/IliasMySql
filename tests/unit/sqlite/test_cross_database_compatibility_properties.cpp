#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <algorithm>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Property testing framework for cross-database compatibility
class CrossDatabaseCompatibilityPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> length_dist{1, 255};
    
public:
    CrossDatabaseCompatibilityPropertyTest() : gen(std::random_device{}()) {}
    
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
    
    // Generate SqlTags with specific constraints for testing
    SqlTags generateConstraintTags(bool primary_key = false, bool not_null = false, 
                                  bool unique = false, bool auto_increment = false,
                                  bool index = false, bool unsigned_type = false,
                                  int length = 0, bool created_at = false, bool updated_at = false) {
        SqlTags tags;
        tags.primary_key = primary_key;
        tags.not_null = not_null;
        tags.unique = unique;
        tags.auto_increment = auto_increment;
        tags.index = index;
        tags.unsigned_type = unsigned_type;
        tags.length = length;
        tags.created_at = created_at;
        tags.updated_at = updated_at;
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
    
    // Helper to check if generated SQL is valid MySQL syntax
    bool isValidMySQLSyntax(const std::string& sql) {
        // Basic MySQL syntax validation
        // Check for proper quoting with backticks
        if (sql.find("\"") != std::string::npos && sql.find("`") == std::string::npos) {
            return false; // MySQL prefers backticks for identifiers
        }
        
        // Check for MySQL-specific keywords
        if (containsIgnoreCase(sql, "AUTOINCREMENT")) {
            return false; // MySQL uses AUTO_INCREMENT, not AUTOINCREMENT
        }
        
        if (containsIgnoreCase(sql, "SERIAL")) {
            return false; // MySQL doesn't use SERIAL type
        }
        
        return true;
    }
    
    // Helper to check if generated SQL is valid SQLite syntax
    bool isValidSQLiteSyntax(const std::string& sql) {
        // Basic SQLite syntax validation
        // Check for SQLite-specific constraints
        if (containsIgnoreCase(sql, "AUTO_INCREMENT")) {
            return false; // SQLite uses AUTOINCREMENT, not AUTO_INCREMENT
        }
        
        if (containsIgnoreCase(sql, "UNSIGNED")) {
            return false; // SQLite doesn't support UNSIGNED modifier
        }
        
        if (containsIgnoreCase(sql, "ON UPDATE")) {
            return false; // SQLite doesn't support ON UPDATE CURRENT_TIMESTAMP
        }
        
        return true;
    }
};

// Test fixture for cross-database compatibility property tests
class CrossDatabaseCompatibilityPropertyTestFixture : public ::testing::Test {
protected:
    CrossDatabaseCompatibilityPropertyTest propertyTester;
};

// **Property 18: MySQL syntax compatibility**
// *For any* constraint configuration when using MySQL driver, the generated SQL should be valid MySQL syntax
// **Validates: Requirements 5.1**
TEST_F(CrossDatabaseCompatibilityPropertyTestFixture, Property18_MySQLSyntaxCompatibility) {
    // Feature: sql-tags-enhancement, Property 18: MySQL syntax compatibility
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateRandomTags();
        
        // Test MySQL dialect with various data types
        std::string intColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(intColumn)) 
            << "MySQL integer column should have valid syntax: " << intColumn;
        
        std::string stringColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(stringColumn)) 
            << "MySQL string column should have valid syntax: " << stringColumn;
        
        std::string boolColumn = Dialect<MysqlTag>::generate_column_definition<bool>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(boolColumn)) 
            << "MySQL boolean column should have valid syntax: " << boolColumn;
        
        std::string dateColumn = Dialect<MysqlTag>::generate_column_definition<SqlDate>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(dateColumn)) 
            << "MySQL date column should have valid syntax: " << dateColumn;
        
        // Check for MySQL-specific syntax elements
        if (tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "PRIMARY KEY"));
        }
        
        if (tags.not_null) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "NOT NULL"));
        }
        
        if (tags.unique && !tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "UNIQUE"));
        }
        
        // Check for proper identifier quoting (backticks)
        EXPECT_TRUE(intColumn.find("`test_col`") != std::string::npos) 
            << "MySQL should use backticks for identifiers: " << intColumn;
        
        // Check auto increment uses AUTO_INCREMENT
        if (tags.auto_increment) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "AUTO_INCREMENT")) 
                << "MySQL should use AUTO_INCREMENT: " << intColumn;
        }
        
        // Check unsigned modifier for numeric types
        if (tags.unsigned_type) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "UNSIGNED")) 
                << "MySQL should support UNSIGNED modifier: " << intColumn;
        }
        
        // Check timestamp defaults and updates
        if (tags.created_at) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(dateColumn, "DEFAULT CURRENT_TIMESTAMP")) 
                << "MySQL should support CURRENT_TIMESTAMP default: " << dateColumn;
        }
        
        if (tags.updated_at) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(dateColumn, "ON UPDATE CURRENT_TIMESTAMP")) 
                << "MySQL should support ON UPDATE CURRENT_TIMESTAMP: " << dateColumn;
        }
        
        // Check string length specification
        if (tags.length > 0) {
            std::string expectedLength = "VARCHAR(" + std::to_string(tags.length) + ")";
            EXPECT_TRUE(propertyTester.containsIgnoreCase(stringColumn, expectedLength)) 
                << "MySQL should use VARCHAR with specified length: " << stringColumn;
        }
        
        // Check index syntax
        if (tags.index && !tags.primary_key && !tags.unique) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "KEY")) 
                << "MySQL should use KEY for index: " << intColumn;
        }
    }, 100);
}

// **Property 19: SQLite syntax compatibility**
// *For any* constraint configuration when using SQLite driver, the generated SQL should be valid SQLite syntax
// **Validates: Requirements 5.2**
TEST_F(CrossDatabaseCompatibilityPropertyTestFixture, Property19_SQLiteSyntaxCompatibility) {
    // Feature: sql-tags-enhancement, Property 19: SQLite syntax compatibility
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateRandomTags();
        
        // Test SQLite dialect with various data types
        std::string intColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(intColumn)) 
            << "SQLite integer column should have valid syntax: " << intColumn;
        
        std::string stringColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(stringColumn)) 
            << "SQLite string column should have valid syntax: " << stringColumn;
        
        std::string boolColumn = Dialect<SqliteTag>::generate_column_definition<bool>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(boolColumn)) 
            << "SQLite boolean column should have valid syntax: " << boolColumn;
        
        std::string dateColumn = Dialect<SqliteTag>::generate_column_definition<SqlDate>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(dateColumn)) 
            << "SQLite date column should have valid syntax: " << dateColumn;
        
        // Check for SQLite-specific syntax elements
        if (tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "PRIMARY KEY"));
        }
        
        if (tags.not_null) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "NOT NULL"));
        }
        
        if (tags.unique && !tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "UNIQUE"));
        }
        
        // Check that SQLite doesn't use backticks or double quotes for simple identifiers
        EXPECT_TRUE(intColumn.find("test_col") != std::string::npos) 
            << "SQLite should use unquoted identifiers: " << intColumn;
        
        // Check auto increment uses AUTOINCREMENT (only with PRIMARY KEY)
        if (tags.auto_increment && tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "AUTOINCREMENT")) 
                << "SQLite should use AUTOINCREMENT with PRIMARY KEY: " << intColumn;
        }
        
        // Check that unsigned modifier is not used (SQLite doesn't support it)
        EXPECT_FALSE(propertyTester.containsIgnoreCase(intColumn, "UNSIGNED")) 
            << "SQLite should not use UNSIGNED modifier: " << intColumn;
        
        // Check timestamp defaults (SQLite supports DEFAULT CURRENT_TIMESTAMP)
        if (tags.created_at) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(dateColumn, "DEFAULT CURRENT_TIMESTAMP")) 
                << "SQLite should support CURRENT_TIMESTAMP default: " << dateColumn;
        }
        
        // Check that ON UPDATE is not used (SQLite doesn't support it)
        EXPECT_FALSE(propertyTester.containsIgnoreCase(dateColumn, "ON UPDATE")) 
            << "SQLite should not use ON UPDATE syntax: " << dateColumn;
        
        // Check SQLite type mapping (everything maps to basic types)
        EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "INTEGER")) 
            << "SQLite should use INTEGER for numeric types: " << intColumn;
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(stringColumn, "TEXT")) 
            << "SQLite should use TEXT for string types: " << stringColumn;
        
        // Check that index syntax is not inline (SQLite uses separate CREATE INDEX)
        if (tags.index && !tags.primary_key && !tags.unique) {
            EXPECT_FALSE(propertyTester.containsIgnoreCase(intColumn, "KEY")) 
                << "SQLite should not use inline KEY syntax: " << intColumn;
        }
    }, 100);
}

// Additional test to verify dialect detection works correctly
TEST_F(CrossDatabaseCompatibilityPropertyTestFixture, DialectDetection) {
    // Test MySQL dialect detection
    EXPECT_TRUE(Dialect<MysqlTag>::check("mysql"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MySQL"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MYSQL"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("mariadb"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MariaDB"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MARIADB"));
    
    // Test SQLite dialect detection (case-sensitive)
    EXPECT_TRUE(Dialect<SqliteTag>::check("sqlite"));
    EXPECT_FALSE(Dialect<SqliteTag>::check("SQLite")); // Case-sensitive
    EXPECT_FALSE(Dialect<SqliteTag>::check("SQLITE")); // Case-sensitive
    
    // Test that dialects don't cross-match
    EXPECT_FALSE(Dialect<MysqlTag>::check("sqlite"));
    EXPECT_FALSE(Dialect<MysqlTag>::check("postgresql"));
    EXPECT_FALSE(Dialect<SqliteTag>::check("mysql"));
    EXPECT_FALSE(Dialect<SqliteTag>::check("postgresql"));
}

// Test specific constraint combinations for MySQL
TEST_F(CrossDatabaseCompatibilityPropertyTestFixture, MySQLConstraintCombinations) {
    propertyTester.runPropertyTest([&]() {
        // Test primary key + auto increment combination
        auto tags = propertyTester.generateConstraintTags(true, true, false, true, false);
        std::string column = Dialect<MysqlTag>::generate_column_definition<int>("id", tags);
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(column, "PRIMARY KEY"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(column, "AUTO_INCREMENT"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(column, "NOT NULL"));
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(column));
        
        // Test unique + not null combination
        auto uniqueTags = propertyTester.generateConstraintTags(false, true, true, false, false);
        std::string uniqueColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("name", uniqueTags);
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(uniqueColumn, "UNIQUE"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(uniqueColumn, "NOT NULL"));
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(uniqueColumn));
        
        // Test timestamp with both created_at and updated_at
        auto timestampTags = propertyTester.generateConstraintTags(false, false, false, false, false, false, 0, true, true);
        std::string timestampColumn = Dialect<MysqlTag>::generate_column_definition<SqlDate>("timestamp_col", timestampTags);
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(timestampColumn, "DEFAULT CURRENT_TIMESTAMP"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(timestampColumn, "ON UPDATE CURRENT_TIMESTAMP"));
        EXPECT_TRUE(propertyTester.isValidMySQLSyntax(timestampColumn));
    }, 50);
}

// Test specific constraint combinations for SQLite
TEST_F(CrossDatabaseCompatibilityPropertyTestFixture, SQLiteConstraintCombinations) {
    propertyTester.runPropertyTest([&]() {
        // Test primary key + auto increment combination (SQLite requires both together)
        auto tags = propertyTester.generateConstraintTags(true, true, false, true, false);
        std::string column = Dialect<SqliteTag>::generate_column_definition<int>("id", tags);
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(column, "PRIMARY KEY"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(column, "AUTOINCREMENT"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(column, "NOT NULL"));
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(column));
        
        // Test unique + not null combination
        auto uniqueTags = propertyTester.generateConstraintTags(false, true, true, false, false);
        std::string uniqueColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("name", uniqueTags);
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(uniqueColumn, "UNIQUE"));
        EXPECT_TRUE(propertyTester.containsIgnoreCase(uniqueColumn, "NOT NULL"));
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(uniqueColumn));
        
        // Test timestamp with created_at (SQLite doesn't support updated_at in column definition)
        auto timestampTags = propertyTester.generateConstraintTags(false, false, false, false, false, false, 0, true, false);
        std::string timestampColumn = Dialect<SqliteTag>::generate_column_definition<SqlDate>("created_at", timestampTags);
        
        EXPECT_TRUE(propertyTester.containsIgnoreCase(timestampColumn, "DEFAULT CURRENT_TIMESTAMP"));
        EXPECT_FALSE(propertyTester.containsIgnoreCase(timestampColumn, "ON UPDATE"));
        EXPECT_TRUE(propertyTester.isValidSQLiteSyntax(timestampColumn));
    }, 50);
}

// Test type mapping consistency across databases
TEST_F(CrossDatabaseCompatibilityPropertyTestFixture, TypeMappingConsistency) {
    SqlTags tags;
    
    // Test integer types
    std::string mysqlInt = Dialect<MysqlTag>::type_name<int>(tags);
    std::string sqliteInt = Dialect<SqliteTag>::type_name<int>(tags);
    
    EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlInt, "INT"));
    EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteInt, "INTEGER"));
    
    // Test string types
    std::string mysqlString = Dialect<MysqlTag>::type_name<std::string>(tags);
    std::string sqliteString = Dialect<SqliteTag>::type_name<std::string>(tags);
    
    EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlString, "TEXT"));
    EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteString, "TEXT"));
    
    // Test boolean types
    std::string mysqlBool = Dialect<MysqlTag>::type_name<bool>(tags);
    std::string sqliteBool = Dialect<SqliteTag>::type_name<bool>(tags);
    
    EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlBool, "TINYINT"));
    EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteBool, "INTEGER"));
    
    // Test date types
    std::string mysqlDate = Dialect<MysqlTag>::type_name<SqlDate>(tags);
    std::string sqliteDate = Dialect<SqliteTag>::type_name<SqlDate>(tags);
    
    EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlDate, "DATETIME"));
    EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteDate, "TEXT"));
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}