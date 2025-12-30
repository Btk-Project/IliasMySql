#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <algorithm>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Property testing framework for PostgreSQL compatibility
class PostgreSQLCompatibilityPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> length_dist{1, 255};
    
public:
    PostgreSQLCompatibilityPropertyTest() : gen(std::random_device{}()) {}
    
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
                                  bool index = false, bool created_at = false) {
        SqlTags tags;
        tags.primary_key = primary_key;
        tags.not_null = not_null;
        tags.unique = unique;
        tags.auto_increment = auto_increment;
        tags.index = index;
        tags.created_at = created_at;
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
    
    // Helper to check if generated SQL is valid PostgreSQL syntax
    bool isValidPostgreSQLSyntax(const std::string& sql) {
        // Basic PostgreSQL syntax validation
        // Check for proper quoting with double quotes
        if (sql.find('`') != std::string::npos) {
            return false; // PostgreSQL doesn't use backticks
        }
        
        // Check for PostgreSQL-specific keywords
        if (containsIgnoreCase(sql, "AUTO_INCREMENT")) {
            return false; // PostgreSQL uses SERIAL instead
        }
        
        return true;
    }
};

// Test fixture for PostgreSQL compatibility property tests
class PostgreSQLCompatibilityPropertyTestFixture : public ::testing::Test {
protected:
    PostgreSQLCompatibilityPropertyTest propertyTester;
};

// **Property 20: PostgreSQL syntax compatibility**
// *For any* constraint configuration when using PostgreSQL driver, the generated SQL should be valid PostgreSQL syntax
// **Validates: Requirements 5.3**
TEST_F(PostgreSQLCompatibilityPropertyTestFixture, Property20_PostgreSQLSyntaxCompatibility) {
    // Feature: sql-tags-enhancement, Property 20: PostgreSQL syntax compatibility
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateRandomTags();
        
        // Test PostgreSQL dialect with various data types
        std::string intColumn = Dialect<PostgresTag>::generate_column_definition<int>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidPostgreSQLSyntax(intColumn)) 
            << "PostgreSQL integer column should have valid syntax: " << intColumn;
        
        std::string stringColumn = Dialect<PostgresTag>::generate_column_definition<std::string>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidPostgreSQLSyntax(stringColumn)) 
            << "PostgreSQL string column should have valid syntax: " << stringColumn;
        
        std::string boolColumn = Dialect<PostgresTag>::generate_column_definition<bool>("test_col", tags);
        EXPECT_TRUE(propertyTester.isValidPostgreSQLSyntax(boolColumn)) 
            << "PostgreSQL boolean column should have valid syntax: " << boolColumn;
        
        // Check for PostgreSQL-specific syntax elements
        if (tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "PRIMARY KEY"));
        }
        
        if (tags.not_null) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "NOT NULL"));
        }
        
        if (tags.unique && !tags.primary_key) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "UNIQUE"));
        }
        
        // Check for proper identifier quoting (double quotes)
        EXPECT_TRUE(intColumn.find("\"test_col\"") != std::string::npos) 
            << "PostgreSQL should use double quotes for identifiers: " << intColumn;
        
        // Check auto increment uses SERIAL
        if (tags.auto_increment) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "SERIAL")) 
                << "PostgreSQL should use SERIAL for auto increment: " << intColumn;
        }
        
        // Check timestamp defaults
        if (tags.created_at) {
            EXPECT_TRUE(propertyTester.containsIgnoreCase(intColumn, "DEFAULT CURRENT_TIMESTAMP")) 
                << "PostgreSQL should support CURRENT_TIMESTAMP default: " << intColumn;
        }
    }, 100);
}

// **Property 21: Unsupported constraint handling**
// *For any* constraint not supported by the target database, the system should either provide equivalent functionality or clear error messages
// **Validates: Requirements 5.4**
TEST_F(PostgreSQLCompatibilityPropertyTestFixture, Property21_UnsupportedConstraintHandling) {
    // Feature: sql-tags-enhancement, Property 21: Unsupported constraint handling
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateRandomTags();
        
        // Test unsigned_type constraint (not supported in PostgreSQL)
        tags.unsigned_type = true;
        
        std::string intColumn = Dialect<PostgresTag>::generate_column_definition<int>("test_col", tags);
        
        // PostgreSQL doesn't support UNSIGNED, but should handle gracefully
        EXPECT_FALSE(propertyTester.containsIgnoreCase(intColumn, "UNSIGNED")) 
            << "PostgreSQL should not include UNSIGNED modifier: " << intColumn;
        
        // Should still generate valid column definition
        EXPECT_TRUE(propertyTester.isValidPostgreSQLSyntax(intColumn)) 
            << "PostgreSQL should generate valid syntax even with unsupported constraints: " << intColumn;
        
        // Test updated_at constraint (requires application-level handling in PostgreSQL)
        tags.updated_at = true;
        
        std::string timestampColumn = Dialect<PostgresTag>::generate_column_definition<SqlDate>("test_col", tags);
        
        // PostgreSQL doesn't support ON UPDATE CURRENT_TIMESTAMP like MySQL
        EXPECT_FALSE(propertyTester.containsIgnoreCase(timestampColumn, "ON UPDATE")) 
            << "PostgreSQL should not include ON UPDATE syntax: " << timestampColumn;
        
        // Should still generate valid column definition
        EXPECT_TRUE(propertyTester.isValidPostgreSQLSyntax(timestampColumn)) 
            << "PostgreSQL should generate valid syntax for timestamp columns: " << timestampColumn;
    }, 100);
}

// **Property 22: Database-specific timestamp types**
// *For any* timestamp field configuration, the system should use appropriate timestamp types for the target database (DATETIME, TIMESTAMP, etc.)
// **Validates: Requirements 5.5**
TEST_F(PostgreSQLCompatibilityPropertyTestFixture, Property22_DatabaseSpecificTimestampTypes) {
    // Feature: sql-tags-enhancement, Property 22: Database-specific timestamp types
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateConstraintTags(false, false, false, false, false, true);
        
        // Test PostgreSQL timestamp type
        std::string postgresTimestamp = Dialect<PostgresTag>::generate_column_definition<SqlDate>("timestamp_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresTimestamp, "TIMESTAMP")) 
            << "PostgreSQL should use TIMESTAMP type: " << postgresTimestamp;
        
        // Compare with other databases to ensure different types are used appropriately
        std::string mysqlTimestamp = Dialect<MysqlTag>::generate_column_definition<SqlDate>("timestamp_col", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlTimestamp, "DATETIME")) 
            << "MySQL should use DATETIME type: " << mysqlTimestamp;
        
        // Test with created_at flag
        std::string postgresCreatedAt = Dialect<PostgresTag>::generate_column_definition<SqlDate>("created_at", tags);
        EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresCreatedAt, "TIMESTAMP")) 
            << "PostgreSQL created_at should use TIMESTAMP: " << postgresCreatedAt;
        EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresCreatedAt, "DEFAULT CURRENT_TIMESTAMP")) 
            << "PostgreSQL created_at should have default: " << postgresCreatedAt;
        
        // Test different timestamp configurations
        auto randomTags = propertyTester.generateRandomTags();
        if (randomTags.created_at || randomTags.updated_at) {
            std::string randomTimestamp = Dialect<PostgresTag>::generate_column_definition<SqlDate>("ts_col", randomTags);
            EXPECT_TRUE(propertyTester.containsIgnoreCase(randomTimestamp, "TIMESTAMP")) 
                << "PostgreSQL should consistently use TIMESTAMP: " << randomTimestamp;
        }
    }, 100);
}

// Additional test to verify PostgreSQL dialect detection
TEST_F(PostgreSQLCompatibilityPropertyTestFixture, PostgreSQLDialectDetection) {
    // Test various PostgreSQL name variations
    EXPECT_TRUE(Dialect<PostgresTag>::check("postgresql"));
    EXPECT_TRUE(Dialect<PostgresTag>::check("postgres"));
    EXPECT_TRUE(Dialect<PostgresTag>::check("pgsql"));
    EXPECT_TRUE(Dialect<PostgresTag>::check("PostgreSQL"));
    EXPECT_TRUE(Dialect<PostgresTag>::check("POSTGRES"));
    EXPECT_TRUE(Dialect<PostgresTag>::check("PgSQL"));
    
    // Test that it doesn't match other database names
    EXPECT_FALSE(Dialect<PostgresTag>::check("mysql"));
    EXPECT_FALSE(Dialect<PostgresTag>::check("sqlite"));
    EXPECT_FALSE(Dialect<PostgresTag>::check("mariadb"));
}

// Test PostgreSQL-specific type mappings
TEST_F(PostgreSQLCompatibilityPropertyTestFixture, PostgreSQLTypeMapping) {
    SqlTags tags;
    
    // Test boolean type
    std::string boolType = Dialect<PostgresTag>::type_name<bool>(tags);
    EXPECT_EQ(boolType, "BOOLEAN");
    
    // Test integer types
    std::string intType = Dialect<PostgresTag>::type_name<int>(tags);
    EXPECT_EQ(intType, "INTEGER");
    
    std::string bigintType = Dialect<PostgresTag>::type_name<int64_t>(tags);
    EXPECT_EQ(bigintType, "BIGINT");
    
    // Test floating point types
    std::string floatType = Dialect<PostgresTag>::type_name<float>(tags);
    EXPECT_EQ(floatType, "REAL");
    
    std::string doubleType = Dialect<PostgresTag>::type_name<double>(tags);
    EXPECT_EQ(doubleType, "DOUBLE PRECISION");
    
    // Test string types
    std::string textType = Dialect<PostgresTag>::type_name<std::string>(tags);
    EXPECT_EQ(textType, "TEXT");
    
    tags.length = 100;
    std::string varcharType = Dialect<PostgresTag>::type_name<std::string>(tags);
    EXPECT_EQ(varcharType, "VARCHAR(100)");
    
    // Test blob type
    std::string blobType = Dialect<PostgresTag>::type_name<SqlBlob>(tags);
    EXPECT_EQ(blobType, "BYTEA");
    
    // Test timestamp type
    std::string timestampType = Dialect<PostgresTag>::type_name<SqlDate>(tags);
    EXPECT_EQ(timestampType, "TIMESTAMP");
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}