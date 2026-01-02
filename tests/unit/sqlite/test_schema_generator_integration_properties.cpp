#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Property testing framework for schema generator integration
class SchemaGeneratorIntegrationPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> length_dist{1, 255};
    std::uniform_int_distribution<int> constraint_count_dist{1, 5};
    
public:
    SchemaGeneratorIntegrationPropertyTest() : gen(std::random_device{}()) {}
    
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
    
    // Generate valid SqlTags configuration (passes validation)
    SqlTags generateValidTags() {
        SqlTags tags = generateRandomTags();
        
        // Ensure configuration is valid by fixing common conflicts
        if (tags.auto_increment && !isNumericCompatible()) {
            tags.auto_increment = false; // Only allow auto_increment on numeric types
        }
        
        if (tags.primary_key) {
            tags.unique = true; // Primary key implies unique
        }
        
        return tags;
    }
    
    // Generate SqlTags with multiple constraints
    SqlTags generateMultiConstraintTags() {
        SqlTags tags;
        int constraintCount = constraint_count_dist(gen);
        
        if (constraintCount >= 1) tags.not_null = true;
        if (constraintCount >= 2) tags.unique = true;
        if (constraintCount >= 3) tags.index = true;
        if (constraintCount >= 4) tags.primary_key = true;
        if (constraintCount >= 5) tags.auto_increment = true;
        
        return tags;
    }
    
    // Generate invalid SqlTags configuration
    SqlTags generateInvalidTags() {
        SqlTags tags;
        
        // Create known invalid configurations
        int invalidType = bool_dist(gen) % 3;
        switch (invalidType) {
            case 0:
                // Negative length
                tags.length = -1;
                break;
            case 1:
                // Auto increment without numeric type (would need type context)
                tags.auto_increment = true;
                // This would be invalid for non-numeric types
                break;
            case 2:
                // Conflicting constraints (primary key without unique behavior)
                tags.primary_key = true;
                tags.unique = false; // This might be a conflict depending on implementation
                break;
        }
        
        return tags;
    }
    
    // Generate column information for testing
    std::vector<std::pair<std::string, SqlTags>> generateColumnInfo(int columnCount = 3) {
        std::vector<std::pair<std::string, SqlTags>> columns;
        
        for (int i = 0; i < columnCount; ++i) {
            std::string columnName = "col_" + std::to_string(i);
            SqlTags tags = generateValidTags();
            columns.emplace_back(columnName, tags);
        }
        
        return columns;
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
    
    // Helper to count occurrences of substring in string
    int countOccurrences(const std::string& str, const std::string& substr) {
        int count = 0;
        size_t pos = 0;
        while ((pos = str.find(substr, pos)) != std::string::npos) {
            count++;
            pos += substr.length();
        }
        return count;
    }
    
    // Check if type is numeric compatible (for auto_increment validation)
    bool isNumericCompatible() {
        // For testing purposes, assume we're testing with int type
        return true;
    }
    
    // Extract constraint keywords from SQL
    std::vector<std::string> extractConstraints(const std::string& sql) {
        std::vector<std::string> constraints;
        
        if (containsIgnoreCase(sql, "PRIMARY KEY")) constraints.push_back("PRIMARY KEY");
        if (containsIgnoreCase(sql, "NOT NULL")) constraints.push_back("NOT NULL");
        if (containsIgnoreCase(sql, "UNIQUE")) constraints.push_back("UNIQUE");
        if (containsIgnoreCase(sql, "AUTO_INCREMENT") || containsIgnoreCase(sql, "AUTOINCREMENT")) {
            constraints.push_back("AUTO_INCREMENT");
        }
        if (containsIgnoreCase(sql, "KEY") && !containsIgnoreCase(sql, "PRIMARY KEY") && !containsIgnoreCase(sql, "UNIQUE KEY")) {
            constraints.push_back("INDEX");
        }
        
        return constraints;
    }
};

// Test fixture for schema generator integration property tests
class SchemaGeneratorIntegrationPropertyTestFixture : public ::testing::Test {
protected:
    SchemaGeneratorIntegrationPropertyTest propertyTester;
};

// **Property 14: Comprehensive constraint inclusion**
// *For any* SqlTags configuration, all specified constraints should appear in the generated CREATE TABLE statement
// **Validates: Requirements 4.1**
TEST_F(SchemaGeneratorIntegrationPropertyTestFixture, Property14_ComprehensiveConstraintInclusion) {
    // Feature: sql-tags-enhancement, Property 14: Comprehensive constraint inclusion
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateValidTags();
        std::string columnName = "test_column";
        
        // Test SQLite dialect
        try {
            std::string sqliteColumn = detail::SchemaGenerator<SqliteTag>::generateColumnDefinition<int>(columnName, tags);
            
            // Verify all enabled constraints are present
            if (tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "PRIMARY KEY"))
                    << "SQLite should include PRIMARY KEY constraint: " << sqliteColumn;
            }
            if (tags.not_null) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "NOT NULL"))
                    << "SQLite should include NOT NULL constraint: " << sqliteColumn;
            }
            if (tags.unique && !tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "UNIQUE"))
                    << "SQLite should include UNIQUE constraint: " << sqliteColumn;
            }
            if (tags.auto_increment && tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "AUTOINCREMENT"))
                    << "SQLite should include AUTOINCREMENT constraint: " << sqliteColumn;
            }
        } catch (const std::exception& e) {
            // If validation fails, that's expected for some configurations
            EXPECT_FALSE(tags.isValid<int>()) << "Exception thrown for valid tags: " << e.what();
        }
        
        // Test MySQL dialect
        try {
            std::string mysqlColumn = detail::SchemaGenerator<MysqlTag>::generateColumnDefinition<int>(columnName, tags);
            
            // Verify all enabled constraints are present
            if (tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "PRIMARY KEY"))
                    << "MySQL should include PRIMARY KEY constraint: " << mysqlColumn;
            }
            if (tags.not_null) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "NOT NULL"))
                    << "MySQL should include NOT NULL constraint: " << mysqlColumn;
            }
            if (tags.unique && !tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "UNIQUE"))
                    << "MySQL should include UNIQUE constraint: " << mysqlColumn;
            }
            if (tags.auto_increment) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "AUTO_INCREMENT"))
                    << "MySQL should include AUTO_INCREMENT constraint: " << mysqlColumn;
            }
            if (tags.index && !tags.primary_key && !tags.unique) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "KEY"))
                    << "MySQL should include KEY for index: " << mysqlColumn;
            }
        } catch (const std::exception& e) {
            // If validation fails, that's expected for some configurations
            EXPECT_FALSE(tags.isValid<int>()) << "Exception thrown for valid tags: " << e.what();
        }
        
        // Test PostgreSQL dialect
        try {
            std::string postgresColumn = detail::SchemaGenerator<PostgresTag>::generateColumnDefinition<int>(columnName, tags);
            
            // Verify all enabled constraints are present
            if (tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "PRIMARY KEY"))
                    << "PostgreSQL should include PRIMARY KEY constraint: " << postgresColumn;
            }
            if (tags.not_null) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "NOT NULL"))
                    << "PostgreSQL should include NOT NULL constraint: " << postgresColumn;
            }
            if (tags.unique && !tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "UNIQUE"))
                    << "PostgreSQL should include UNIQUE constraint: " << postgresColumn;
            }
            if (tags.auto_increment) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "SERIAL") || 
                           propertyTester.containsIgnoreCase(postgresColumn, "BIGSERIAL"))
                    << "PostgreSQL should include SERIAL for auto_increment: " << postgresColumn;
            }
        } catch (const std::exception& e) {
            // If validation fails, that's expected for some configurations
            EXPECT_FALSE(tags.isValid<int>()) << "Exception thrown for valid tags: " << e.what();
        }
    }, 100);
}

// **Property 15: Multiple constraint combination**
// *For any* field with multiple constraints defined, all constraints should be present and correctly formatted in the generated SQL
// **Validates: Requirements 4.2**
TEST_F(SchemaGeneratorIntegrationPropertyTestFixture, Property15_MultipleConstraintCombination) {
    // Feature: sql-tags-enhancement, Property 15: Multiple constraint combination
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateMultiConstraintTags();
        std::string columnName = "multi_constraint_column";
        
        // Count expected constraints
        int expectedConstraints = 0;
        if (tags.primary_key) expectedConstraints++;
        if (tags.not_null) expectedConstraints++;
        if (tags.unique && !tags.primary_key) expectedConstraints++; // Primary key implies unique
        if (tags.auto_increment) expectedConstraints++;
        if (tags.index && !tags.primary_key && !tags.unique) expectedConstraints++; // Only if not already indexed
        
        if (expectedConstraints <= 1) return; // Skip single constraint cases
        
        // Test MySQL dialect (most comprehensive constraint support)
        try {
            std::string mysqlColumn = detail::SchemaGenerator<MysqlTag>::generateColumnDefinition<int>(columnName, tags);
            auto foundConstraints = propertyTester.extractConstraints(mysqlColumn);
            
            EXPECT_GE(foundConstraints.size(), 2) 
                << "Multiple constraints should be present in MySQL column: " << mysqlColumn
                << " Expected at least 2, found: " << foundConstraints.size();
            
            // Verify specific constraint combinations work correctly
            if (tags.primary_key && tags.not_null) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "PRIMARY KEY"));
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "NOT NULL"));
            }
            
            if (tags.unique && tags.not_null && !tags.primary_key) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "UNIQUE"));
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "NOT NULL"));
            }
            
        } catch (const std::exception& e) {
            // If validation fails, that's expected for some invalid combinations
            EXPECT_FALSE(tags.isValid<int>()) << "Exception thrown for valid multi-constraint tags: " << e.what();
        }
        
        // Test SQLite dialect
        try {
            std::string sqliteColumn = detail::SchemaGenerator<SqliteTag>::generateColumnDefinition<int>(columnName, tags);
            auto foundConstraints = propertyTester.extractConstraints(sqliteColumn);
            
            // SQLite should handle multiple constraints, though with some limitations
            EXPECT_GE(foundConstraints.size(), 1) 
                << "At least one constraint should be present in SQLite column: " << sqliteColumn;
                
        } catch (const std::exception& e) {
            EXPECT_FALSE(tags.isValid<int>()) << "Exception thrown for valid multi-constraint tags: " << e.what();
        }
    }, 100);
}

// **Property 16: Database-specific constraint translation**
// *For any* constraint configuration and target database type, constraints should be translated to the appropriate database-specific syntax
// **Validates: Requirements 4.3**
TEST_F(SchemaGeneratorIntegrationPropertyTestFixture, Property16_DatabaseSpecificConstraintTranslation) {
    // Feature: sql-tags-enhancement, Property 16: Database-specific constraint translation
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateValidTags();
        std::string columnName = "translation_test_column";
        
        try {
            // Generate column definitions for all dialects
            std::string sqliteColumn = detail::SchemaGenerator<SqliteTag>::generateColumnDefinition<int>(columnName, tags);
            std::string mysqlColumn = detail::SchemaGenerator<MysqlTag>::generateColumnDefinition<int>(columnName, tags);
            std::string postgresColumn = detail::SchemaGenerator<PostgresTag>::generateColumnDefinition<int>(columnName, tags);
            
            // Verify database-specific syntax differences
            
            // Auto-increment syntax should be different across databases
            if (tags.auto_increment) {
                if (tags.primary_key) {
                    // SQLite uses AUTOINCREMENT (only with PRIMARY KEY)
                    EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, "AUTOINCREMENT"))
                        << "SQLite should use AUTOINCREMENT: " << sqliteColumn;
                }
                
                // MySQL uses AUTO_INCREMENT
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "AUTO_INCREMENT"))
                    << "MySQL should use AUTO_INCREMENT: " << mysqlColumn;
                
                // PostgreSQL uses SERIAL/BIGSERIAL
                EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "SERIAL"))
                    << "PostgreSQL should use SERIAL: " << postgresColumn;
            }
            
            // Column name quoting should be different
            if (propertyTester.containsIgnoreCase(mysqlColumn, "`")) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, "`" + columnName + "`"))
                    << "MySQL should use backtick quoting: " << mysqlColumn;
            }
            
            if (propertyTester.containsIgnoreCase(postgresColumn, "\"")) {
                EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, "\"" + columnName + "\""))
                    << "PostgreSQL should use double quote quoting: " << postgresColumn;
            }
            
            // Verify that all dialects produce valid, non-empty output
            EXPECT_FALSE(sqliteColumn.empty()) << "SQLite column definition should not be empty";
            EXPECT_FALSE(mysqlColumn.empty()) << "MySQL column definition should not be empty";
            EXPECT_FALSE(postgresColumn.empty()) << "PostgreSQL column definition should not be empty";
            
            // Verify that each contains the column name
            EXPECT_TRUE(propertyTester.containsIgnoreCase(sqliteColumn, columnName))
                << "SQLite column should contain column name: " << sqliteColumn;
            EXPECT_TRUE(propertyTester.containsIgnoreCase(mysqlColumn, columnName))
                << "MySQL column should contain column name: " << mysqlColumn;
            EXPECT_TRUE(propertyTester.containsIgnoreCase(postgresColumn, columnName))
                << "PostgreSQL column should contain column name: " << postgresColumn;
                
        } catch (const std::exception& e) {
            EXPECT_FALSE(tags.isValid<int>()) << "Exception thrown for valid tags: " << e.what();
        }
    }, 100);
}

// **Property 17: Invalid constraint error reporting**
// *For any* invalid constraint combination, the system should provide clear and descriptive error messages
// **Validates: Requirements 4.4**
TEST_F(SchemaGeneratorIntegrationPropertyTestFixture, Property17_InvalidConstraintErrorReporting) {
    // Feature: sql-tags-enhancement, Property 17: Invalid constraint error reporting
    propertyTester.runPropertyTest([&]() {
        auto tags = propertyTester.generateInvalidTags();
        std::string columnName = "invalid_test_column";
        
        // Test that invalid configurations throw exceptions with meaningful messages
        
        // Test SQLite dialect
        try {
            std::string sqliteColumn = detail::SchemaGenerator<SqliteTag>::generateColumnDefinition<int>(columnName, tags);
            
            // If no exception was thrown, the tags should be valid
            EXPECT_TRUE(tags.isValid<int>()) 
                << "No exception thrown but tags appear invalid. Generated: " << sqliteColumn;
                
        } catch (const std::invalid_argument& e) {
            // Verify error message contains useful information
            std::string errorMsg = e.what();
            EXPECT_TRUE(propertyTester.containsIgnoreCase(errorMsg, columnName) ||
                       propertyTester.containsIgnoreCase(errorMsg, "invalid") ||
                       propertyTester.containsIgnoreCase(errorMsg, "configuration"))
                << "Error message should be descriptive: " << errorMsg;
                
            // Verify that the tags are indeed invalid
            EXPECT_FALSE(tags.isValid<int>()) 
                << "Exception thrown but tags appear valid. Error: " << errorMsg;
                
        } catch (const std::exception& e) {
            // Other exceptions are also acceptable for invalid configurations
            std::string errorMsg = e.what();
            EXPECT_FALSE(errorMsg.empty()) << "Error message should not be empty";
        }
        
        // Test MySQL dialect
        try {
            std::string mysqlColumn = detail::SchemaGenerator<MysqlTag>::generateColumnDefinition<int>(columnName, tags);
            
            EXPECT_TRUE(tags.isValid<int>()) 
                << "No exception thrown but tags appear invalid. Generated: " << mysqlColumn;
                
        } catch (const std::invalid_argument& e) {
            std::string errorMsg = e.what();
            EXPECT_TRUE(propertyTester.containsIgnoreCase(errorMsg, columnName) ||
                       propertyTester.containsIgnoreCase(errorMsg, "invalid") ||
                       propertyTester.containsIgnoreCase(errorMsg, "configuration"))
                << "MySQL error message should be descriptive: " << errorMsg;
                
            EXPECT_FALSE(tags.isValid<int>()) 
                << "Exception thrown but tags appear valid. Error: " << errorMsg;
                
        } catch (const std::exception& e) {
            std::string errorMsg = e.what();
            EXPECT_FALSE(errorMsg.empty()) << "MySQL error message should not be empty";
        }
        
        // Test PostgreSQL dialect
        try {
            std::string postgresColumn = detail::SchemaGenerator<PostgresTag>::generateColumnDefinition<int>(columnName, tags);
            
            EXPECT_TRUE(tags.isValid<int>()) 
                << "No exception thrown but tags appear invalid. Generated: " << postgresColumn;
                
        } catch (const std::invalid_argument& e) {
            std::string errorMsg = e.what();
            EXPECT_TRUE(propertyTester.containsIgnoreCase(errorMsg, columnName) ||
                       propertyTester.containsIgnoreCase(errorMsg, "invalid") ||
                       propertyTester.containsIgnoreCase(errorMsg, "configuration"))
                << "PostgreSQL error message should be descriptive: " << errorMsg;
                
            EXPECT_FALSE(tags.isValid<int>()) 
                << "Exception thrown but tags appear valid. Error: " << errorMsg;
                
        } catch (const std::exception& e) {
            std::string errorMsg = e.what();
            EXPECT_FALSE(errorMsg.empty()) << "PostgreSQL error message should not be empty";
        }
    }, 100);
}

// Additional test to verify index statement generation works correctly
TEST_F(SchemaGeneratorIntegrationPropertyTestFixture, IndexStatementGeneration) {
    propertyTester.runPropertyTest([&]() {
        auto columnInfo = propertyTester.generateColumnInfo(3);
        std::string tableName = "test_table";
        
        // Ensure at least one column has index = true
        bool hasIndexColumn = false;
        for (auto& [colName, tags] : columnInfo) {
            if (tags.index) {
                hasIndexColumn = true;
                break;
            }
        }
        
        if (!hasIndexColumn && !columnInfo.empty()) {
            columnInfo[0].second.index = true;
            columnInfo[0].second.primary_key = false; // Avoid primary key which already creates index
            columnInfo[0].second.unique = false; // Avoid unique which already creates index
        }
        
        // Test index statement generation for all dialects
        auto sqliteIndexes = detail::SchemaGenerator<SqliteTag>::generateIndexStatements(tableName, columnInfo);
        auto mysqlIndexes = detail::SchemaGenerator<MysqlTag>::generateIndexStatements(tableName, columnInfo);
        auto postgresIndexes = detail::SchemaGenerator<PostgresTag>::generateIndexStatements(tableName, columnInfo);
        
        // Verify that index statements are generated when needed
        for (const auto& [colName, tags] : columnInfo) {
            if (tags.index && !tags.primary_key && !tags.unique) {
                // Should have at least one index statement for this column
                bool foundSqliteIndex = std::any_of(sqliteIndexes.begin(), sqliteIndexes.end(),
                    [&](const std::string& stmt) { return propertyTester.containsIgnoreCase(stmt, colName); });
                bool foundMysqlIndex = std::any_of(mysqlIndexes.begin(), mysqlIndexes.end(),
                    [&](const std::string& stmt) { return propertyTester.containsIgnoreCase(stmt, colName); });
                bool foundPostgresIndex = std::any_of(postgresIndexes.begin(), postgresIndexes.end(),
                    [&](const std::string& stmt) { return propertyTester.containsIgnoreCase(stmt, colName); });
                
                EXPECT_TRUE(foundSqliteIndex) << "SQLite should generate index for column: " << colName;
                EXPECT_TRUE(foundMysqlIndex) << "MySQL should generate index for column: " << colName;
                EXPECT_TRUE(foundPostgresIndex) << "PostgreSQL should generate index for column: " << colName;
            }
        }
    }, 50);
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}