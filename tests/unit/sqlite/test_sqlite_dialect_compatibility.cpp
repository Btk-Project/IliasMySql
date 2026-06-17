#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Test fixture for SQLite dialect compatibility tests
class SQLiteDialectCompatibilityTest : public ::testing::Test {
protected:
    // Helper to check if string contains substring (case insensitive)
    bool containsIgnoreCase(const std::string& str, const std::string& substr) {
        std::string strLower = str;
        std::string substrLower = substr;
        std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
        std::transform(substrLower.begin(), substrLower.end(), substrLower.begin(), ::tolower);
        return strLower.find(substrLower) != std::string::npos;
    }
    
    // Helper to validate SQLite syntax
    bool isValidSQLiteSyntax(const std::string& sql) {
        // Check for SQLite-specific constraints
        if (containsIgnoreCase(sql, "AUTO_INCREMENT")) {
            return false; // SQLite uses AUTOINCREMENT
        }
        
        if (containsIgnoreCase(sql, "UNSIGNED")) {
            return false; // SQLite doesn't support UNSIGNED
        }
        
        if (containsIgnoreCase(sql, "ON UPDATE")) {
            return false; // SQLite doesn't support ON UPDATE CURRENT_TIMESTAMP
        }
        
        if (containsIgnoreCase(sql, "SERIAL")) {
            return false; // SQLite doesn't use SERIAL
        }
        
        return true;
    }
};

// Test SQLite dialect detection
TEST_F(SQLiteDialectCompatibilityTest, DialectDetection) {
    EXPECT_TRUE(Dialect<SqliteTag>::check("sqlite"));
    EXPECT_FALSE(Dialect<SqliteTag>::check("SQLite")); // Case-sensitive
    EXPECT_FALSE(Dialect<SqliteTag>::check("SQLITE")); // Case-sensitive
    
    // Should not match other databases
    EXPECT_FALSE(Dialect<SqliteTag>::check("mysql"));
    EXPECT_FALSE(Dialect<SqliteTag>::check("postgresql"));
    EXPECT_FALSE(Dialect<SqliteTag>::check("mariadb"));
}

// Test SQLite type mappings
TEST_F(SQLiteDialectCompatibilityTest, TypeMappings) {
    SqlTags tags;
    
    // Test boolean type (maps to INTEGER in SQLite)
    std::string boolType = Dialect<SqliteTag>::type_name<bool>(tags);
    EXPECT_EQ(boolType, "INTEGER");
    
    // Test integer types (all map to INTEGER in SQLite)
    std::string intType = Dialect<SqliteTag>::type_name<int>(tags);
    EXPECT_EQ(intType, "INTEGER");
    
    std::string bigintType = Dialect<SqliteTag>::type_name<int64_t>(tags);
    EXPECT_EQ(bigintType, "INTEGER");
    
    // Test floating point types
    std::string floatType = Dialect<SqliteTag>::type_name<float>(tags);
    EXPECT_EQ(floatType, "REAL");
    
    std::string doubleType = Dialect<SqliteTag>::type_name<double>(tags);
    EXPECT_EQ(doubleType, "REAL");
    
    // Test string types (all map to TEXT in SQLite)
    std::string textType = Dialect<SqliteTag>::type_name<std::string>(tags);
    EXPECT_EQ(textType, "TEXT");
    
    // SQLite doesn't use VARCHAR, even with length specified
    tags.length = 100;
    std::string lengthType = Dialect<SqliteTag>::type_name<std::string>(tags);
    EXPECT_EQ(lengthType, "TEXT");
    
    // Test blob type
    std::string blobType = Dialect<SqliteTag>::type_name<SqlBlob>(tags);
    EXPECT_EQ(blobType, "BLOB");
    
    // Test date type (maps to TEXT in SQLite)
    std::string dateType = Dialect<SqliteTag>::type_name<SqlDate>(tags);
    EXPECT_EQ(dateType, "TEXT");
}

// Test SQLite constraint generation
TEST_F(SQLiteDialectCompatibilityTest, ConstraintGeneration) {
    // Test primary key constraint
    SqlTags primaryKeyTags;
    primaryKeyTags.primary_key = true;
    
    std::string primaryKeyColumn = Dialect<SqliteTag>::generate_column_definition<int>("id", primaryKeyTags);
    EXPECT_TRUE(containsIgnoreCase(primaryKeyColumn, "PRIMARY KEY"));
    EXPECT_TRUE(containsIgnoreCase(primaryKeyColumn, "id")); // No backticks in SQLite
    EXPECT_FALSE(containsIgnoreCase(primaryKeyColumn, "`")); // Should not have backticks
    EXPECT_TRUE(isValidSQLiteSyntax(primaryKeyColumn));
    
    // Test not null constraint
    SqlTags notNullTags;
    notNullTags.not_null = true;
    
    std::string notNullColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("name", notNullTags);
    EXPECT_TRUE(containsIgnoreCase(notNullColumn, "NOT NULL"));
    EXPECT_TRUE(isValidSQLiteSyntax(notNullColumn));
    
    // Test unique constraint
    SqlTags uniqueTags;
    uniqueTags.unique = true;
    
    std::string uniqueColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("email", uniqueTags);
    EXPECT_TRUE(containsIgnoreCase(uniqueColumn, "UNIQUE"));
    EXPECT_FALSE(containsIgnoreCase(uniqueColumn, "UNIQUE KEY")); // SQLite uses just UNIQUE
    EXPECT_TRUE(isValidSQLiteSyntax(uniqueColumn));
    
    // Test auto increment constraint (only works with PRIMARY KEY in SQLite)
    SqlTags autoIncrementTags;
    autoIncrementTags.auto_increment = true;
    autoIncrementTags.primary_key = true; // Required for AUTOINCREMENT in SQLite
    
    std::string autoIncrementColumn = Dialect<SqliteTag>::generate_column_definition<int>("id", autoIncrementTags);
    EXPECT_TRUE(containsIgnoreCase(autoIncrementColumn, "AUTOINCREMENT"));
    EXPECT_TRUE(containsIgnoreCase(autoIncrementColumn, "PRIMARY KEY"));
    EXPECT_TRUE(isValidSQLiteSyntax(autoIncrementColumn));
    
    // Test auto increment without primary key (should not include AUTOINCREMENT)
    SqlTags autoIncrementNoPKTags;
    autoIncrementNoPKTags.auto_increment = true;
    autoIncrementNoPKTags.primary_key = false;
    
    std::string autoIncrementNoPKColumn = Dialect<SqliteTag>::generate_column_definition<int>("id", autoIncrementNoPKTags);
    EXPECT_FALSE(containsIgnoreCase(autoIncrementNoPKColumn, "AUTOINCREMENT"));
    EXPECT_TRUE(isValidSQLiteSyntax(autoIncrementNoPKColumn));
}

// Test SQLite timestamp behavior
TEST_F(SQLiteDialectCompatibilityTest, TimestampBehavior) {
    // Test created_at timestamp
    SqlTags createdAtTags;
    createdAtTags.created_at = true;
    
    std::string createdAtColumn = Dialect<SqliteTag>::generate_column_definition<SqlDate>("created_at", createdAtTags);
    EXPECT_TRUE(containsIgnoreCase(createdAtColumn, "DEFAULT CURRENT_TIMESTAMP"));
    EXPECT_TRUE(isValidSQLiteSyntax(createdAtColumn));
    
    // Test updated_at timestamp (SQLite doesn't support ON UPDATE in column definition)
    SqlTags updatedAtTags;
    updatedAtTags.updated_at = true;
    
    std::string updatedAtColumn = Dialect<SqliteTag>::generate_column_definition<SqlDate>("updated_at", updatedAtTags);
    EXPECT_FALSE(containsIgnoreCase(updatedAtColumn, "ON UPDATE"));
    EXPECT_TRUE(isValidSQLiteSyntax(updatedAtColumn));
    
    // Test both created_at and updated_at (only created_at should appear in column definition)
    SqlTags bothTimestampTags;
    bothTimestampTags.created_at = true;
    bothTimestampTags.updated_at = true;
    
    std::string bothTimestampColumn = Dialect<SqliteTag>::generate_column_definition<SqlDate>("timestamp_col", bothTimestampTags);
    EXPECT_TRUE(containsIgnoreCase(bothTimestampColumn, "DEFAULT CURRENT_TIMESTAMP"));
    EXPECT_FALSE(containsIgnoreCase(bothTimestampColumn, "ON UPDATE"));
    EXPECT_TRUE(isValidSQLiteSyntax(bothTimestampColumn));
}

// Test SQLite doesn't support unsigned modifier
TEST_F(SQLiteDialectCompatibilityTest, UnsignedModifierNotSupported) {
    SqlTags unsignedTags;
    unsignedTags.unsigned_type = true;
    
    // Test with integer type
    std::string intColumn = Dialect<SqliteTag>::generate_column_definition<int>("count", unsignedTags);
    EXPECT_FALSE(containsIgnoreCase(intColumn, "UNSIGNED"));
    EXPECT_TRUE(containsIgnoreCase(intColumn, "INTEGER"));
    EXPECT_TRUE(isValidSQLiteSyntax(intColumn));
    
    // Test with string type
    std::string stringColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("name", unsignedTags);
    EXPECT_FALSE(containsIgnoreCase(stringColumn, "UNSIGNED"));
    EXPECT_TRUE(containsIgnoreCase(stringColumn, "TEXT"));
    EXPECT_TRUE(isValidSQLiteSyntax(stringColumn));
}

// Test SQLite index handling (no inline index syntax)
TEST_F(SQLiteDialectCompatibilityTest, IndexHandling) {
    SqlTags indexTags;
    indexTags.index = true;
    
    std::string indexColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("name", indexTags);
    EXPECT_FALSE(containsIgnoreCase(indexColumn, "KEY"));
    EXPECT_FALSE(containsIgnoreCase(indexColumn, "INDEX"));
    EXPECT_TRUE(isValidSQLiteSyntax(indexColumn));
    
    // Index should be created via separate CREATE INDEX statements
    // Test that the column definition itself doesn't include index syntax
    EXPECT_TRUE(containsIgnoreCase(indexColumn, "name"));
    EXPECT_TRUE(containsIgnoreCase(indexColumn, "TEXT"));
}

// Test SQLite constraint combinations
TEST_F(SQLiteDialectCompatibilityTest, ConstraintCombinations) {
    // Test primary key + auto increment + not null
    SqlTags combinedTags;
    combinedTags.primary_key = true;
    combinedTags.auto_increment = true;
    combinedTags.not_null = true;
    
    std::string combinedColumn = Dialect<SqliteTag>::generate_column_definition<int>("id", combinedTags);
    EXPECT_TRUE(containsIgnoreCase(combinedColumn, "PRIMARY KEY"));
    EXPECT_TRUE(containsIgnoreCase(combinedColumn, "AUTOINCREMENT"));
    EXPECT_TRUE(containsIgnoreCase(combinedColumn, "NOT NULL"));
    EXPECT_TRUE(isValidSQLiteSyntax(combinedColumn));
    
    // Test unique + not null (should not have primary key)
    SqlTags uniqueNotNullTags;
    uniqueNotNullTags.unique = true;
    uniqueNotNullTags.not_null = true;
    uniqueNotNullTags.primary_key = false;
    
    std::string uniqueNotNullColumn = Dialect<SqliteTag>::generate_column_definition<std::string>("email", uniqueNotNullTags);
    EXPECT_TRUE(containsIgnoreCase(uniqueNotNullColumn, "UNIQUE"));
    EXPECT_TRUE(containsIgnoreCase(uniqueNotNullColumn, "NOT NULL"));
    EXPECT_FALSE(containsIgnoreCase(uniqueNotNullColumn, "PRIMARY KEY"));
    EXPECT_TRUE(isValidSQLiteSyntax(uniqueNotNullColumn));
    
    // Test created_at + not null
    SqlTags timestampNotNullTags;
    timestampNotNullTags.created_at = true;
    timestampNotNullTags.not_null = true;
    
    std::string timestampColumn = Dialect<SqliteTag>::generate_column_definition<SqlDate>("created_at", timestampNotNullTags);
    EXPECT_TRUE(containsIgnoreCase(timestampColumn, "DEFAULT CURRENT_TIMESTAMP"));
    EXPECT_TRUE(containsIgnoreCase(timestampColumn, "NOT NULL"));
    EXPECT_TRUE(isValidSQLiteSyntax(timestampColumn));
}

// Test SQLite index statement generation
TEST_F(SQLiteDialectCompatibilityTest, IndexStatementGeneration) {
    std::vector<std::pair<std::string, SqlTags>> columns;
    
    // Add column with index flag
    SqlTags indexTags;
    indexTags.index = true;
    columns.emplace_back("name", indexTags);
    
    // Add column with primary key (should not generate separate index)
    SqlTags primaryKeyTags;
    primaryKeyTags.primary_key = true;
    columns.emplace_back("id", primaryKeyTags);
    
    // Add column with unique (should not generate separate index)
    SqlTags uniqueTags;
    uniqueTags.unique = true;
    columns.emplace_back("email", uniqueTags);
    
    auto indexStatements = Dialect<SqliteTag>::generate_index_statements("users", columns);
    
    // Should only generate index for the 'name' column
    for (const auto& statement : indexStatements) {
        ILIAS_INFO("sqlite-test", "Index statement: {}", statement);
    }
    EXPECT_EQ(indexStatements.size(), 1);
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "CREATE INDEX"));
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "\"idx_users_name\""));
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "ON \"users\""));
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "(\"name\")"));
    
    // SQLite doesn't use backticks or quotes for simple identifiers
    EXPECT_FALSE(containsIgnoreCase(indexStatements[0], "`"));
}

// Test SQLite syntax validation edge cases
TEST_F(SQLiteDialectCompatibilityTest, SyntaxValidationEdgeCases) {
    // Test empty tags
    SqlTags emptyTags;
    std::string emptyColumn = Dialect<SqliteTag>::generate_column_definition<int>("test", emptyTags);
    EXPECT_TRUE(isValidSQLiteSyntax(emptyColumn));
    EXPECT_TRUE(containsIgnoreCase(emptyColumn, "test"));
    EXPECT_TRUE(containsIgnoreCase(emptyColumn, "INTEGER"));
    EXPECT_FALSE(containsIgnoreCase(emptyColumn, "`")); // No backticks
    
    // Test all supported flags enabled
    SqlTags allSupportedTags;
    allSupportedTags.primary_key = true;
    allSupportedTags.not_null = true;
    allSupportedTags.unique = true;
    allSupportedTags.auto_increment = true;
    allSupportedTags.created_at = true;
    // Note: not setting unsigned_type, updated_at, or index as they're not fully supported
    
    std::string allSupportedColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_all", allSupportedTags);
    EXPECT_TRUE(isValidSQLiteSyntax(allSupportedColumn));
    
    // When primary_key is true, unique should not appear separately
    EXPECT_TRUE(containsIgnoreCase(allSupportedColumn, "PRIMARY KEY"));
    EXPECT_FALSE(containsIgnoreCase(allSupportedColumn, "UNIQUE"));
    
    // Test unsupported combinations gracefully handled
    SqlTags unsupportedTags;
    unsupportedTags.unsigned_type = true;
    unsupportedTags.updated_at = true;
    unsupportedTags.index = true;
    
    std::string unsupportedColumn = Dialect<SqliteTag>::generate_column_definition<int>("test_unsupported", unsupportedTags);
    EXPECT_TRUE(isValidSQLiteSyntax(unsupportedColumn));
    EXPECT_FALSE(containsIgnoreCase(unsupportedColumn, "UNSIGNED"));
    EXPECT_FALSE(containsIgnoreCase(unsupportedColumn, "ON UPDATE"));
    EXPECT_FALSE(containsIgnoreCase(unsupportedColumn, "KEY"));
}

// Test SQLite type system simplicity
TEST_F(SQLiteDialectCompatibilityTest, TypeSystemSimplicity) {
    SqlTags tags;
    
    // All integer types should map to INTEGER
    EXPECT_EQ(Dialect<SqliteTag>::type_name<int8_t>(tags), "INTEGER");
    EXPECT_EQ(Dialect<SqliteTag>::type_name<int16_t>(tags), "INTEGER");
    EXPECT_EQ(Dialect<SqliteTag>::type_name<int32_t>(tags), "INTEGER");
    EXPECT_EQ(Dialect<SqliteTag>::type_name<int64_t>(tags), "INTEGER");
    EXPECT_EQ(Dialect<SqliteTag>::type_name<bool>(tags), "INTEGER");
    
    // All floating point types should map to REAL
    EXPECT_EQ(Dialect<SqliteTag>::type_name<float>(tags), "REAL");
    EXPECT_EQ(Dialect<SqliteTag>::type_name<double>(tags), "REAL");
    
    // All string-like types should map to TEXT
    EXPECT_EQ(Dialect<SqliteTag>::type_name<std::string>(tags), "TEXT");
    EXPECT_EQ(Dialect<SqliteTag>::type_name<const char*>(tags), "TEXT");
    
    // Date should map to TEXT (SQLite stores dates as text)
    EXPECT_EQ(Dialect<SqliteTag>::type_name<SqlDate>(tags), "TEXT");
    
    // Blob should map to BLOB
    EXPECT_EQ(Dialect<SqliteTag>::type_name<SqlBlob>(tags), "BLOB");
}

// Test SQLite length handling (length is ignored)
TEST_F(SQLiteDialectCompatibilityTest, LengthHandling) {
    SqlTags tags;
    
    // Test various lengths - all should result in TEXT
    tags.length = 50;
    EXPECT_EQ(Dialect<SqliteTag>::type_name<std::string>(tags), "TEXT");
    
    tags.length = 255;
    EXPECT_EQ(Dialect<SqliteTag>::type_name<std::string>(tags), "TEXT");
    
    tags.length = 1000;
    EXPECT_EQ(Dialect<SqliteTag>::type_name<std::string>(tags), "TEXT");
    
    tags.length = 0;
    EXPECT_EQ(Dialect<SqliteTag>::type_name<std::string>(tags), "TEXT");
    
    // Even with indexing requirements, SQLite still uses TEXT
    tags.length = 0;
    tags.primary_key = true;
    EXPECT_EQ(Dialect<SqliteTag>::type_name<std::string>(tags), "TEXT");
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}