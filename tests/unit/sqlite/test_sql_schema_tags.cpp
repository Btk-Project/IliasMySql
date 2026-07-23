#include <gtest/gtest.h>
#include <sqlite3.h>

#include <memory>
#include <string>
#include <string_view>

#include "ilias/sql_orm/detail/schema_generator.hpp"

ILIAS_SQL_USE_NAMESPACE;

namespace {

struct TagParent {
    int id = 0;
};

struct TagChild {
    int         id           = 0;
    int         parent_id    = 0;
    int         singleton_id = 1;
    std::string status;
    std::string code;
};

struct CustomPositiveTag {
    constexpr static bool             not_null             = true;
    constexpr static std::string_view sql_check_expression = "value > 0";
};

struct CustomClauseRecord {
    int value = 0;
};

struct CompositeSchemaRecord {
    int         owner_id  = 0;
    int         sequence  = 0;
    std::string stable_key;
    int64_t     updated_at = 0;
};

struct NullableCompositeKeyRecord {
    int owner_id = 0;
    int sequence = 0;
};

} // namespace

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<TagParent, void> {
    constexpr static auto value =
        Object("id", make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags::createPrimaryKeyTags()>(&TagParent::id));
};

template <>
struct Meta<TagChild, void> {
    using SqlReferenceAction = ILIAS_SQL_COMPLETE_NAMESPACE::SqlReferenceAction;

    constexpr static auto value = Object(
        "id",
        make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags::createPrimaryKeyTags(),
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"CHECK (id > 0)">>(&TagChild::id),
        "parent_id",
        make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true},
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_references<"tag_parents", "id", SqlReferenceAction::Cascade,
                                                               SqlReferenceAction::Restrict>>(&TagChild::parent_id),
        "singleton_id",
        make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true},
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_default<"1">,
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_check<"singleton_id = 1">>(&TagChild::singleton_id),
        "status",
        make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true},
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_default<"'pending'">>(&TagChild::status),
        "code", make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::sql_collate<"NOCASE">>(&TagChild::code));
};

template <>
struct Meta<CustomClauseRecord, void> {
    using SqlCustomPosition = ILIAS_SQL_COMPLETE_NAMESPACE::SqlCustomPosition;

    constexpr static auto value = Object(
        "value",
        make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"GLOBAL_AFTER", "", SqlCustomPosition::AfterType>,
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"SQLITE_AFTER", "sqlite", SqlCustomPosition::AfterType>,
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"MYSQL_AFTER", "mysql", SqlCustomPosition::AfterType>,
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"GLOBAL_TAIL">,
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"SQLITE_TAIL", "sqlite">,
                  ILIAS_SQL_COMPLETE_NAMESPACE::sql_custom<"MYSQL_TAIL", "mysql">>(&CustomClauseRecord::value));
};

template <>
struct Meta<CompositeSchemaRecord, void> {
    constexpr static auto value = Object(
        "owner_id", make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true}>(
                        &CompositeSchemaRecord::owner_id),
        "sequence", make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true}>(
                        &CompositeSchemaRecord::sequence),
        "stable_key", make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true}>(
                          &CompositeSchemaRecord::stable_key),
        "updated_at", make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true}>(
                         &CompositeSchemaRecord::updated_at));
};

template <>
struct Meta<NullableCompositeKeyRecord, void> {
    constexpr static auto value =
        Object("owner_id",
               make_tags<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.not_null = true}>(
                   &NullableCompositeKeyRecord::owner_id),
               "sequence", &NullableCompositeKeyRecord::sequence);
};
NEKO_END_NAMESPACE

ILIAS_SQL_NS_BEGIN
template <>
struct SqlTableMeta<CompositeSchemaRecord> {
    constexpr static auto value =
        sql_table(sql_primary_key<&CompositeSchemaRecord::owner_id, &CompositeSchemaRecord::sequence>,
                  sql_unique<&CompositeSchemaRecord::owner_id, &CompositeSchemaRecord::stable_key>,
                  sql_table_check<"sequence >= 0">,
                  sql_index<"idx_composite_updated", sql_desc<&CompositeSchemaRecord::updated_at>,
                            sql_asc<&CompositeSchemaRecord::owner_id>>);
};

template <>
struct SqlTableMeta<NullableCompositeKeyRecord> {
    constexpr static auto value =
        sql_table(sql_primary_key<&NullableCompositeKeyRecord::owner_id,
                                  &NullableCompositeKeyRecord::sequence>);
};
ILIAS_SQL_NS_END

TEST(SqlSchemaTags, PropertyExtractionSupportsCustomTags) {
    constexpr auto tags =
        NEKO_NAMESPACE::TagList<ILIAS_SQL_COMPLETE_NAMESPACE::SqlTags {.unique = true}, CustomPositiveTag {}> {};
    const auto metadata = ILIAS_SQL_COMPLETE_NAMESPACE::detail::extractSqlColumnMetadata(tags);

    EXPECT_TRUE(metadata.tags.not_null);
    EXPECT_TRUE(metadata.tags.unique);
    EXPECT_EQ(metadata.check_expression, "value > 0");
}

TEST(SqlSchemaTags, GeneratesCommonColumnMetadataForAllDialects) {
    using ILIAS_SQL_COMPLETE_NAMESPACE::detail::SchemaGenerator;

    auto sqlite   = SchemaGenerator<SqliteTag>::generateTableSchema<TagChild>("tag_children");
    auto mysql    = SchemaGenerator<MysqlTag>::generateTableSchema<TagChild>("tag_children");
    auto postgres = SchemaGenerator<PostgresTag>::generateTableSchema<TagChild>("tag_children");

    ASSERT_TRUE(sqlite);
    ASSERT_TRUE(mysql);
    ASSERT_TRUE(postgres);
    ASSERT_EQ(sqlite->tableConstraints.size(), 1);
    ASSERT_EQ(mysql->tableConstraints.size(), 1);
    ASSERT_EQ(postgres->tableConstraints.size(), 1);

    EXPECT_NE(sqlite->createTableSql.find("DEFAULT 1"), std::string::npos);
    EXPECT_NE(sqlite->createTableSql.find("DEFAULT 'pending'"), std::string::npos);
    EXPECT_NE(sqlite->createTableSql.find("CHECK (singleton_id = 1)"), std::string::npos);
    EXPECT_NE(sqlite->createTableSql.find("COLLATE \"NOCASE\""), std::string::npos);
    EXPECT_NE(sqlite->createTableSql.find("FOREIGN KEY (\"parent_id\") REFERENCES \"tag_parents\" (\"id\") "
                                          "ON DELETE CASCADE ON UPDATE RESTRICT"),
              std::string::npos);

    EXPECT_NE(mysql->createTableSql.find("COLLATE `NOCASE`"), std::string::npos);
    EXPECT_NE(mysql->createTableSql.find("FOREIGN KEY (`parent_id`) REFERENCES `tag_parents` (`id`) "
                                         "ON DELETE CASCADE ON UPDATE RESTRICT"),
              std::string::npos);

    EXPECT_NE(postgres->createTableSql.find("COLLATE \"NOCASE\""), std::string::npos);
    EXPECT_NE(postgres->createTableSql.find("FOREIGN KEY (\"parent_id\") REFERENCES \"tag_parents\" (\"id\") "
                                            "ON DELETE CASCADE ON UPDATE RESTRICT"),
              std::string::npos);
}

TEST(SqlSchemaTags, FiltersAndOrdersRepeatableCustomClausesByBackend) {
    using ILIAS_SQL_COMPLETE_NAMESPACE::detail::SchemaGenerator;

    auto sqlite   = SchemaGenerator<SqliteTag>::generateTableSchema<CustomClauseRecord>("custom_clauses");
    auto mysql    = SchemaGenerator<MysqlTag>::generateTableSchema<CustomClauseRecord>("custom_clauses");
    auto postgres = SchemaGenerator<PostgresTag>::generateTableSchema<CustomClauseRecord>("custom_clauses");

    ASSERT_TRUE(sqlite);
    ASSERT_TRUE(mysql);
    ASSERT_TRUE(postgres);
    ASSERT_EQ(sqlite->columnDefinitions.size(), 1);
    ASSERT_EQ(mysql->columnDefinitions.size(), 1);
    ASSERT_EQ(postgres->columnDefinitions.size(), 1);

    EXPECT_EQ(sqlite->columnDefinitions.front(), "\"value\" INTEGER GLOBAL_AFTER SQLITE_AFTER GLOBAL_TAIL SQLITE_TAIL");
    EXPECT_EQ(mysql->columnDefinitions.front(), "`value` INT GLOBAL_AFTER MYSQL_AFTER GLOBAL_TAIL MYSQL_TAIL");
    EXPECT_EQ(postgres->columnDefinitions.front(), "\"value\" INTEGER GLOBAL_AFTER GLOBAL_TAIL");
}

TEST(SqlSchemaTags, GeneratesCompositeTableConstraintsAndIndexesForAllDialects) {
    using ILIAS_SQL_COMPLETE_NAMESPACE::detail::SchemaGenerator;

    auto sqlite = SchemaGenerator<SqliteTag>::generateTableSchema<CompositeSchemaRecord>("composite_records");
    auto mysql = SchemaGenerator<MysqlTag>::generateTableSchema<CompositeSchemaRecord>("composite_records");
    auto postgres = SchemaGenerator<PostgresTag>::generateTableSchema<CompositeSchemaRecord>("composite_records");

    ASSERT_TRUE(sqlite);
    ASSERT_TRUE(mysql);
    ASSERT_TRUE(postgres);

    EXPECT_NE(sqlite->createTableSql.find("PRIMARY KEY (\"owner_id\", \"sequence\")"), std::string::npos);
    EXPECT_NE(sqlite->createTableSql.find("UNIQUE (\"owner_id\", \"stable_key\")"), std::string::npos);
    EXPECT_NE(sqlite->createTableSql.find("CHECK (sequence >= 0)"), std::string::npos);
    ASSERT_EQ(sqlite->indexStatements.size(), 1);
    EXPECT_EQ(sqlite->indexStatements.front(),
              "CREATE INDEX \"idx_composite_updated\" ON \"composite_records\" "
              "(\"updated_at\" DESC, \"owner_id\" ASC)");

    EXPECT_NE(mysql->createTableSql.find("PRIMARY KEY (`owner_id`, `sequence`)"), std::string::npos);
    ASSERT_EQ(mysql->indexStatements.size(), 1);
    EXPECT_EQ(mysql->indexStatements.front(),
              "CREATE INDEX `idx_composite_updated` ON `composite_records` "
              "(`updated_at` DESC, `owner_id` ASC)");

    EXPECT_NE(postgres->createTableSql.find("PRIMARY KEY (\"owner_id\", \"sequence\")"), std::string::npos);
    ASSERT_EQ(postgres->indexStatements.size(), 1);
    EXPECT_EQ(postgres->indexStatements.front(),
              "CREATE INDEX \"idx_composite_updated\" ON \"composite_records\" "
              "(\"updated_at\" DESC, \"owner_id\" ASC)");
}

TEST(SqlSchemaTags, GeneratesPortableUpsertDialectFragments) {
    const std::vector<std::string> conflicts {"\"provider_key\"", "\"external_id\""};
    const std::vector<std::string> assignments {"\"fetched_at\" = excluded.\"fetched_at\""};
    EXPECT_EQ(Dialect<SqliteTag>::generate_upsert_clause(conflicts, assignments, false),
              " ON CONFLICT (\"provider_key\", \"external_id\") DO UPDATE SET "
              "\"fetched_at\" = excluded.\"fetched_at\"");
    EXPECT_EQ(Dialect<PostgresTag>::generate_upsert_clause(conflicts, assignments, false),
              " ON CONFLICT (\"provider_key\", \"external_id\") DO UPDATE SET "
              "\"fetched_at\" = excluded.\"fetched_at\"");

    const std::vector<std::string> mysqlConflicts {"`provider_key`", "`external_id`"};
    const std::vector<std::string> mysqlAssignments {"`fetched_at` = VALUES(`fetched_at`)"};
    EXPECT_EQ(Dialect<MysqlTag>::generate_upsert_clause(mysqlConflicts, mysqlAssignments, false),
              " ON DUPLICATE KEY UPDATE `fetched_at` = VALUES(`fetched_at`)");

    EXPECT_EQ(Dialect<SqliteTag>::greatest_value("\"metadata_level\"", "excluded.\"metadata_level\""),
              "MAX(\"metadata_level\", excluded.\"metadata_level\")");
    EXPECT_EQ(Dialect<PostgresTag>::greatest_value("\"metadata_level\"", "excluded.\"metadata_level\""),
              "GREATEST(\"metadata_level\", excluded.\"metadata_level\")");
}

TEST(SqlSchemaTags, RejectsConflictingOrTypeIncompatibleMetadata) {
    using ILIAS_SQL_COMPLETE_NAMESPACE::detail::SchemaGenerator;

    SqlColumnMetadata explicitTimestampDefault {
        .tags               = SqlTags {.created_at = true},
        .default_expression = "CURRENT_DATE",
    };
    EXPECT_FALSE(explicitTimestampDefault.getValidationErrors<SqlDate>().empty());

    SqlColumnMetadata invalidCollation {.collation = "NOCASE"};
    EXPECT_FALSE(invalidCollation.getValidationErrors<int>().empty());

    SqlColumnMetadata invalidAutoIncrementDefault {
        .tags               = SqlTags::createPrimaryKeyTags(true),
        .default_expression = "1",
    };
    EXPECT_FALSE(invalidAutoIncrementDefault.getValidationErrors<int>().empty());

    SqlColumnMetadata invalidSetNull {
        .tags             = SqlTags {.not_null = true},
        .reference_table  = "parents",
        .reference_column = "id",
        .on_delete        = SqlReferenceAction::SetNull,
    };
    EXPECT_FALSE(invalidSetNull.getValidationErrors<int>().empty());

    SqlColumnMetadata orphanedAction {.on_delete = SqlReferenceAction::Cascade};
    EXPECT_FALSE(orphanedAction.getValidationErrors<int>().empty());

    SqlColumnMetadata emptyCustomClause {
        .custom_clauses = {SqlCustomClause {}},
    };
    EXPECT_FALSE(emptyCustomClause.getValidationErrors<int>().empty());

    EXPECT_FALSE(
        SchemaGenerator<SqliteTag>::generateTableSchema<NullableCompositeKeyRecord>(
            "nullable_composite_key"));
}

TEST(SqlSchemaTags, GeneratedSqlWorksWithSQLite) {
    using ILIAS_SQL_COMPLETE_NAMESPACE::detail::SchemaGenerator;

    auto parentSchema = SchemaGenerator<SqliteTag>::generateTableSchema<TagParent>("tag_parents");
    auto childSchema  = SchemaGenerator<SqliteTag>::generateTableSchema<TagChild>("tag_children");
    ASSERT_TRUE(parentSchema);
    ASSERT_TRUE(childSchema);

    sqlite3 *rawDb = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &rawDb), SQLITE_OK);
    std::unique_ptr<sqlite3, decltype(&sqlite3_close)> db(rawDb, &sqlite3_close);

    auto execute = [&](const std::string &sql) {
        char     *message = nullptr;
        const int rc      = sqlite3_exec(db.get(), sql.c_str(), nullptr, nullptr, &message);
        if (message != nullptr) {
            sqlite3_free(message);
        }
        return rc;
    };

    ASSERT_EQ(execute("PRAGMA foreign_keys = ON"), SQLITE_OK);
    ASSERT_EQ(execute(parentSchema->createTableSql), SQLITE_OK);
    ASSERT_EQ(execute(childSchema->createTableSql), SQLITE_OK);
    ASSERT_EQ(execute("INSERT INTO tag_parents(id) VALUES (7)"), SQLITE_OK);
    ASSERT_EQ(execute("INSERT INTO tag_children(id, parent_id, code) VALUES (1, 7, 'Alpha')"), SQLITE_OK);

    sqlite3_stmt *rawStatement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db.get(), "SELECT singleton_id, status FROM tag_children WHERE code = 'alpha'", -1,
                                 &rawStatement, nullptr),
              SQLITE_OK);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> statement(rawStatement, &sqlite3_finalize);
    ASSERT_EQ(sqlite3_step(statement.get()), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(statement.get(), 0), 1);
    EXPECT_STREQ(reinterpret_cast<const char *>(sqlite3_column_text(statement.get(), 1)), "pending");

    EXPECT_EQ(execute("INSERT INTO tag_children(id, parent_id, singleton_id) VALUES (2, 7, 2)"), SQLITE_CONSTRAINT);
    EXPECT_EQ(execute("INSERT INTO tag_children(id, parent_id) VALUES (3, 99)"), SQLITE_CONSTRAINT);
    EXPECT_EQ(execute("INSERT INTO tag_children(id, parent_id) VALUES (-1, 7)"), SQLITE_CONSTRAINT);

    ASSERT_EQ(execute("DELETE FROM tag_parents WHERE id = 7"), SQLITE_OK);
    sqlite3_stmt *rawCountStatement = nullptr;
    ASSERT_EQ(sqlite3_prepare_v2(db.get(), "SELECT count(*) FROM tag_children", -1, &rawCountStatement, nullptr),
              SQLITE_OK);
    std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)> countStatement(rawCountStatement, &sqlite3_finalize);
    ASSERT_EQ(sqlite3_step(countStatement.get()), SQLITE_ROW);
    EXPECT_EQ(sqlite3_column_int(countStatement.get(), 0), 0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
