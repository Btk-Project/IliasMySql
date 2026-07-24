#include <gtest/gtest.h>
#include <vector>
#include <string>
#include <algorithm>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

ILIAS_SQL_USE_NAMESPACE;

// Test fixture for MySQL dialect compatibility tests
class MySQLDialectCompatibilityTest : public ::testing::Test {
protected:
    // Helper to check if string contains substring (case insensitive)
    bool containsIgnoreCase(std::string_view str, std::string_view substr) {
        std::string strLower {str};
        std::string substrLower {substr};
        std::transform(strLower.begin(), strLower.end(), strLower.begin(), ::tolower);
        std::transform(substrLower.begin(), substrLower.end(), substrLower.begin(), ::tolower);
        return strLower.find(substrLower) != std::string::npos;
    }

    // Helper to validate MySQL syntax
    bool isValidMySQLSyntax(std::string_view sql) {
        // Check for proper identifier quoting
        if (sql.find("\"") != std::string::npos && sql.find("`") == std::string::npos) {
            return false; // MySQL prefers backticks
        }

        // Check for MySQL-specific keywords
        if (containsIgnoreCase(sql, "AUTOINCREMENT")) {
            return false; // MySQL uses AUTO_INCREMENT
        }

        if (containsIgnoreCase(sql, "SERIAL")) {
            return false; // MySQL doesn't use SERIAL
        }

        return true;
    }
};

// Test MySQL dialect detection
TEST_F(MySQLDialectCompatibilityTest, DialectDetection) {
    EXPECT_TRUE(Dialect<MysqlTag>::check("mysql"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MySQL"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MYSQL"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("mariadb"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MariaDB"));
    EXPECT_TRUE(Dialect<MysqlTag>::check("MARIADB"));

    // Should not match other databases
    EXPECT_FALSE(Dialect<MysqlTag>::check("sqlite"));
    EXPECT_FALSE(Dialect<MysqlTag>::check("postgresql"));
    EXPECT_FALSE(Dialect<MysqlTag>::check("postgres"));
}

// Test MySQL type mappings
TEST_F(MySQLDialectCompatibilityTest, TypeMappings) {
    SqlTags tags;

    // Test boolean type
    std::string boolType = Dialect<MysqlTag>::type_name<bool>(tags);
    EXPECT_EQ(boolType, "TINYINT(1)");

    // Test integer types
    std::string intType = Dialect<MysqlTag>::type_name<int>(tags);
    EXPECT_EQ(intType, "INT");

    std::string bigintType = Dialect<MysqlTag>::type_name<int64_t>(tags);
    EXPECT_EQ(bigintType, "BIGINT");

    // Test floating point types
    std::string floatType = Dialect<MysqlTag>::type_name<float>(tags);
    EXPECT_EQ(floatType, "FLOAT");

    std::string doubleType = Dialect<MysqlTag>::type_name<double>(tags);
    EXPECT_EQ(doubleType, "DOUBLE");

    // Test string types
    std::string textType = Dialect<MysqlTag>::type_name<std::string>(tags);
    EXPECT_EQ(textType, "TEXT");

    tags.length             = 100;
    std::string varcharType = Dialect<MysqlTag>::type_name<std::string>(tags);
    EXPECT_EQ(varcharType, "VARCHAR(100)");

    // Test blob type
    std::string blobType = Dialect<MysqlTag>::type_name<SqlBlob>(tags);
    EXPECT_EQ(blobType, "BLOB");

    // Test date type
    std::string dateType = Dialect<MysqlTag>::type_name<SqlDate>(tags);
    EXPECT_EQ(dateType, "DATETIME");
}

// Test MySQL unsigned type modifier
TEST_F(MySQLDialectCompatibilityTest, UnsignedTypeModifier) {
    SqlTags tags;
    tags.unsigned_type = true;

    // Test with integer types
    std::string intType = Dialect<MysqlTag>::type_name<int>(tags);
    EXPECT_EQ(intType, "INT UNSIGNED");

    std::string bigintType = Dialect<MysqlTag>::type_name<int64_t>(tags);
    EXPECT_EQ(bigintType, "BIGINT UNSIGNED");

    // Test that unsigned doesn't affect non-numeric types
    std::string stringType = Dialect<MysqlTag>::type_name<std::string>(tags);
    EXPECT_EQ(stringType, "TEXT");
    EXPECT_FALSE(containsIgnoreCase(stringType, "UNSIGNED"));
}

// Test MySQL constraint generation
TEST_F(MySQLDialectCompatibilityTest, ConstraintGeneration) {
    // Test primary key constraint
    SqlTags primaryKeyTags;
    primaryKeyTags.primary_key = true;

    std::string primaryKeyColumn = Dialect<MysqlTag>::generate_column_definition<int>("id", primaryKeyTags);
    EXPECT_TRUE(containsIgnoreCase(primaryKeyColumn, "PRIMARY KEY"));
    EXPECT_TRUE(containsIgnoreCase(primaryKeyColumn, "`id`"));
    EXPECT_TRUE(isValidMySQLSyntax(primaryKeyColumn));

    // Test not null constraint
    SqlTags notNullTags;
    notNullTags.not_null = true;

    std::string notNullColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("name", notNullTags);
    EXPECT_TRUE(containsIgnoreCase(notNullColumn, "NOT NULL"));
    EXPECT_TRUE(isValidMySQLSyntax(notNullColumn));

    // Test unique constraint
    SqlTags uniqueTags;
    uniqueTags.unique = true;

    std::string uniqueColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("email", uniqueTags);
    EXPECT_TRUE(containsIgnoreCase(uniqueColumn, "UNIQUE KEY"));
    EXPECT_TRUE(isValidMySQLSyntax(uniqueColumn));

    // Test auto increment constraint
    SqlTags autoIncrementTags;
    autoIncrementTags.auto_increment = true;

    std::string autoIncrementColumn = Dialect<MysqlTag>::generate_column_definition<int>("id", autoIncrementTags);
    EXPECT_TRUE(containsIgnoreCase(autoIncrementColumn, "AUTO_INCREMENT"));
    EXPECT_TRUE(isValidMySQLSyntax(autoIncrementColumn));

    // Test index constraint
    SqlTags indexTags;
    indexTags.index = true;

    std::string indexColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("name", indexTags);
    EXPECT_TRUE(containsIgnoreCase(indexColumn, "KEY"));
    EXPECT_TRUE(isValidMySQLSyntax(indexColumn));
}

// Test MySQL timestamp behavior
TEST_F(MySQLDialectCompatibilityTest, TimestampBehavior) {
    // Test created_at timestamp
    SqlTags createdAtTags;
    createdAtTags.created_at = true;

    std::string createdAtColumn = Dialect<MysqlTag>::generate_column_definition<SqlDate>("created_at", createdAtTags);
    EXPECT_TRUE(containsIgnoreCase(createdAtColumn, "DEFAULT CURRENT_TIMESTAMP"));
    EXPECT_TRUE(isValidMySQLSyntax(createdAtColumn));

    // Test updated_at timestamp
    SqlTags updatedAtTags;
    updatedAtTags.updated_at = true;

    std::string updatedAtColumn = Dialect<MysqlTag>::generate_column_definition<SqlDate>("updated_at", updatedAtTags);
    EXPECT_TRUE(containsIgnoreCase(updatedAtColumn, "ON UPDATE CURRENT_TIMESTAMP"));
    EXPECT_TRUE(isValidMySQLSyntax(updatedAtColumn));

    // Test both created_at and updated_at
    SqlTags bothTimestampTags;
    bothTimestampTags.created_at = true;
    bothTimestampTags.updated_at = true;

    std::string bothTimestampColumn =
        Dialect<MysqlTag>::generate_column_definition<SqlDate>("timestamp_col", bothTimestampTags);
    EXPECT_TRUE(containsIgnoreCase(bothTimestampColumn, "DEFAULT CURRENT_TIMESTAMP"));
    EXPECT_TRUE(containsIgnoreCase(bothTimestampColumn, "ON UPDATE CURRENT_TIMESTAMP"));
    EXPECT_TRUE(isValidMySQLSyntax(bothTimestampColumn));
}

// Test MySQL string length handling
TEST_F(MySQLDialectCompatibilityTest, StringLengthHandling) {
    // Test specific length
    SqlTags lengthTags;
    lengthTags.length = 50;

    std::string lengthColumn = Dialect<MysqlTag>::generate_column_definition<std::string>("name", lengthTags);
    EXPECT_TRUE(containsIgnoreCase(lengthColumn, "VARCHAR(50)"));
    EXPECT_TRUE(isValidMySQLSyntax(lengthColumn));

    // Test zero length with indexing requirement
    SqlTags indexedZeroLengthTags;
    indexedZeroLengthTags.length      = 0;
    indexedZeroLengthTags.primary_key = true;

    std::string indexedZeroLengthColumn =
        Dialect<MysqlTag>::generate_column_definition<std::string>("id", indexedZeroLengthTags);
    EXPECT_TRUE(containsIgnoreCase(indexedZeroLengthColumn, "VARCHAR(255)"));
    EXPECT_TRUE(isValidMySQLSyntax(indexedZeroLengthColumn));

    // Test zero length without indexing requirement
    SqlTags zeroLengthTags;
    zeroLengthTags.length = 0;

    std::string zeroLengthColumn =
        Dialect<MysqlTag>::generate_column_definition<std::string>("description", zeroLengthTags);
    EXPECT_TRUE(containsIgnoreCase(zeroLengthColumn, "TEXT"));
    EXPECT_TRUE(isValidMySQLSyntax(zeroLengthColumn));
}

// Test MySQL constraint combinations
TEST_F(MySQLDialectCompatibilityTest, ConstraintCombinations) {
    // Test primary key + auto increment + not null
    SqlTags combinedTags;
    combinedTags.primary_key    = true;
    combinedTags.auto_increment = true;
    combinedTags.not_null       = true;

    std::string combinedColumn = Dialect<MysqlTag>::generate_column_definition<int>("id", combinedTags);
    EXPECT_TRUE(containsIgnoreCase(combinedColumn, "PRIMARY KEY"));
    EXPECT_TRUE(containsIgnoreCase(combinedColumn, "AUTO_INCREMENT"));
    EXPECT_TRUE(containsIgnoreCase(combinedColumn, "NOT NULL"));
    EXPECT_TRUE(isValidMySQLSyntax(combinedColumn));

    // Test unique + not null (should not have primary key)
    SqlTags uniqueNotNullTags;
    uniqueNotNullTags.unique      = true;
    uniqueNotNullTags.not_null    = true;
    uniqueNotNullTags.primary_key = false;

    std::string uniqueNotNullColumn =
        Dialect<MysqlTag>::generate_column_definition<std::string>("email", uniqueNotNullTags);
    EXPECT_TRUE(containsIgnoreCase(uniqueNotNullColumn, "UNIQUE KEY"));
    EXPECT_TRUE(containsIgnoreCase(uniqueNotNullColumn, "NOT NULL"));
    EXPECT_FALSE(containsIgnoreCase(uniqueNotNullColumn, "PRIMARY KEY"));
    EXPECT_TRUE(isValidMySQLSyntax(uniqueNotNullColumn));

    // Test unsigned + length + not null
    SqlTags unsignedLengthTags;
    unsignedLengthTags.unsigned_type = true;
    unsignedLengthTags.not_null      = true;

    std::string unsignedColumn = Dialect<MysqlTag>::generate_column_definition<int>("count", unsignedLengthTags);
    EXPECT_TRUE(containsIgnoreCase(unsignedColumn, "UNSIGNED"));
    EXPECT_TRUE(containsIgnoreCase(unsignedColumn, "NOT NULL"));
    EXPECT_TRUE(isValidMySQLSyntax(unsignedColumn));
}

// Test MySQL index statement generation
TEST_F(MySQLDialectCompatibilityTest, IndexStatementGeneration) {
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

    auto indexStatements = Dialect<MysqlTag>::generate_index_statements("users", columns);

    // Should only generate index for the 'name' column
    EXPECT_EQ(indexStatements.size(), 1);
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "CREATE INDEX"));
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "`idx_users_name`"));
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "ON `users`"));
    EXPECT_TRUE(containsIgnoreCase(indexStatements[0], "(`name`)"));
}

// Test MySQL syntax validation edge cases
TEST_F(MySQLDialectCompatibilityTest, SyntaxValidationEdgeCases) {
    // Test empty tags
    SqlTags     emptyTags;
    std::string emptyColumn = Dialect<MysqlTag>::generate_column_definition<int>("test", emptyTags);
    EXPECT_TRUE(isValidMySQLSyntax(emptyColumn));
    EXPECT_TRUE(containsIgnoreCase(emptyColumn, "`test`"));
    EXPECT_TRUE(containsIgnoreCase(emptyColumn, "INT"));

    // Test all flags enabled
    SqlTags allFlagsTags;
    allFlagsTags.primary_key    = true;
    allFlagsTags.not_null       = true;
    allFlagsTags.unique         = true;
    allFlagsTags.auto_increment = true;
    allFlagsTags.index          = true;
    allFlagsTags.unsigned_type  = true;
    allFlagsTags.created_at     = true;
    allFlagsTags.updated_at     = true;
    allFlagsTags.length         = 100;

    std::string allFlagsColumn = Dialect<MysqlTag>::generate_column_definition<int>("test_all", allFlagsTags);
    EXPECT_TRUE(isValidMySQLSyntax(allFlagsColumn));

    // When primary_key is true, unique should not appear separately
    EXPECT_TRUE(containsIgnoreCase(allFlagsColumn, "PRIMARY KEY"));
    EXPECT_FALSE(containsIgnoreCase(allFlagsColumn, "UNIQUE KEY"));
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}