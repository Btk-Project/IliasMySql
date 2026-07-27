#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <ilias/platform.hpp>
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sql_orm/orm_form.hpp"
#include "sqlite_test_runtime.hpp"

ILIAS_SQL_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;
NEKO_USE_NAMESPACE

struct KeywordRecord {
    int         id         = 0;
    std::string from_value = "";
};

struct MergeRecord {
    int                        id = 0;
    std::optional<std::string> details;
    int                        metadata_level = 0;
};

template <>
struct NEKO_NAMESPACE::Meta<KeywordRecord> {
    static constexpr auto value =
        Object("id", make_tags<SqlTags::createPrimaryKeyTags(false)>(&KeywordRecord::id), "from",
               make_tags<rename_tag<"from">, SqlTags {.not_null = true}>(&KeywordRecord::from_value));
};

template <>
struct NEKO_NAMESPACE::Meta<MergeRecord> {
    static constexpr auto value =
        Object("id", make_tags<SqlTags::createPrimaryKeyTags(false)>(&MergeRecord::id), "details",
               &MergeRecord::details, "metadata_level",
               make_tags<SqlTags {.not_null = true}>(&MergeRecord::metadata_level));
};

struct DuplicateColumnRecord {
    int id    = 0;
    int other = 0;

    NEKO_SERIALIZER(make_tags<SqlTags::createPrimaryKeyTags(false)>(id), (make_tags<rename_tag<"id">>(other)))
};

struct RenamedKeywordRecord {
    int         id         = 0;
    std::string from_value = "";
    NEKO_SERIALIZER(make_tags<SqlTags::createPrimaryKeyTags(false)>(id),
                    (make_tags<rename_tag<"from">, SqlTags {.not_null = true, .index = true}>(from_value)))
};

struct UnsupportedTransientState {
    std::string value = "local-default";
};

struct IgnoredFieldRecord {
    int                       id = 0;
    UnsupportedTransientState transient;
    std::string               name;

    NEKO_SERIALIZER(make_tags<SqlTags::createPrimaryKeyTags(false)>(id),
                    (make_tags<serialization_ignore_tag>(transient)), (make_tags<rename_tag<"display_name">>(name)))
};

static_assert(ILIAS_SQL_COMPLETE_NAMESPACE::detail::reflectedSqlFieldCount<IgnoredFieldRecord>() == 2);
static_assert(ILIAS_SQL_COMPLETE_NAMESPACE::detail::reflectedSqlFieldNames<IgnoredFieldRecord>()[1] == "display_name");
[[maybe_unused]] constexpr SqlStructCheck<IgnoredFieldRecord> ignoredFieldSql {
    "INSERT INTO ignored_records VALUES (:id, :display_name)"};

struct PartialReflectionRecord {
    int         id     = 0;
    int         hidden = 0;
    std::string name   = "";
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<PartialReflectionRecord, void> {
    constexpr static auto value =
        Object("id", make_tags<SqlTags::createPrimaryKeyTags(false)>(&PartialReflectionRecord::id), "name",
               make_tags<SqlTags {.not_null = true}>(&PartialReflectionRecord::name));
};
NEKO_END_NAMESPACE

TEST(SqlIdentifierQuoting, DialectQuotesAndValidatesIdentifiers) {
    EXPECT_TRUE(Dialect<SqliteTag>::validate_identifier("select"));
    EXPECT_TRUE(Dialect<MysqlTag>::validate_identifier("_value1"));
    EXPECT_FALSE(Dialect<PostgresTag>::validate_identifier(""));
    EXPECT_FALSE(Dialect<PostgresTag>::validate_identifier("1value"));
    EXPECT_FALSE(Dialect<PostgresTag>::validate_identifier("bad name"));

    EXPECT_EQ(Dialect<SqliteTag>::quote_identifier("select"), "\"select\"");
    EXPECT_EQ(Dialect<MysqlTag>::quote_identifier("select"), "`select`");
    EXPECT_EQ(Dialect<PostgresTag>::quote_identifier("select"), "\"select\"");
    EXPECT_THROW((void)Dialect<SqliteTag>::quote_identifier("bad name"), std::invalid_argument);
}

TEST(SqlIdentifierQuoting, IndexStatementsQuoteTableIndexAndColumnNames) {
    SqlTags index_tags;
    index_tags.index = true;

    std::vector<std::pair<std::string, SqlTags>> columns {{"from", index_tags}};

    const auto expectIndexSemantics = [&columns]<typename Tag>(std::string_view quotedIndex,
                                                               std::string_view quotedTable,
                                                               std::string_view quotedColumn) {
        const auto statements = Dialect<Tag>::generate_index_statements("select", columns);
        ASSERT_EQ(statements.size(), 1U);
        EXPECT_NE(statements.front().find("CREATE INDEX"), std::string::npos);
        EXPECT_NE(statements.front().find(quotedIndex), std::string::npos);
        EXPECT_NE(statements.front().find(quotedTable), std::string::npos);
        EXPECT_NE(statements.front().find(quotedColumn), std::string::npos);
    };

    expectIndexSemantics.template operator()<SqliteTag>("\"idx_select_from\"", "\"select\"", "\"from\"");
    expectIndexSemantics.template operator()<MysqlTag>("`idx_select_from`", "`select`", "`from`");
    expectIndexSemantics.template operator()<PostgresTag>("\"idx_select_from\"", "\"select\"", "\"from\"");
}

static auto test_reserved_identifier_roundtrip() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto form_ret = co_await Form<KeywordRecord, SqliteTag>::create_if_not_exists(db, "select");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(form_ret.value());

    auto insert_ret = co_await form.emplace(1, "hello");
    if (!insert_ret) {
        ADD_FAILURE() << insert_ret.error().message();
        co_return {};
    }

    auto query_ret = co_await form.select(form.sql(&KeywordRecord::from_value))
                         .where(form.sql(&KeywordRecord::id) == 1)
                         .orderBy(form.sql(&KeywordRecord::id))
                         .query();
    if (!query_ret) {
        ADD_FAILURE() << query_ret.error().message();
        co_return {};
    }

    std::string value;
    ilias_for_await(auto row, query_ret.value().rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        value = std::get<0>(row.value());
    }
    EXPECT_EQ(value, "hello");

    auto runtime_column_ret =
        co_await form.select("from").where(form.sql(&KeywordRecord::id) == 1).orderBy("id").query();
    if (!runtime_column_ret) {
        ADD_FAILURE() << runtime_column_ret.error().message();
        co_return {};
    }
    std::string runtime_value;
    ilias_for_await(auto row, runtime_column_ret.value().range(runtime_value)) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
    }
    EXPECT_EQ(runtime_value, "hello");
    EXPECT_THROW((void)form.select("count(*)"), std::invalid_argument);

    auto update_ret = co_await form.update()
                          .set(form.sql(&KeywordRecord::from_value) = std::string("updated"))
                          .where(form.sql(&KeywordRecord::id) == 1)
                          .execute();
    if (!update_ret) {
        ADD_FAILURE() << update_ret.error().message();
        co_return {};
    }
    EXPECT_EQ(update_ret.value(), 1U);

    auto verify_ret =
        co_await form.select(form.sql(&KeywordRecord::from_value)).where(form.sql(&KeywordRecord::id) == 1).query();
    if (!verify_ret) {
        ADD_FAILURE() << verify_ret.error().message();
        co_return {};
    }
    ilias_for_await(auto row, verify_ret.value().rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_EQ(std::get<0>(row.value()), "updated");
    }
    co_return {};
}

static auto test_compatible_form_attachment() -> IoTask<void> {
    auto dbRet = co_await SqlDatabase::open_in_memory();
    if (!dbRet) {
        ADD_FAILURE() << dbRet.error().message();
        co_return {};
    }
    auto db = std::move(*dbRet);

    auto created = co_await db.execute(
        "CREATE TABLE compatible_records("
        "id INTEGER, \"from\" TEXT, extra BLOB, UNIQUE(\"from\"))");
    if (!created) {
        ADD_FAILURE() << created.error().message();
        co_return {};
    }

    auto attached = co_await Form<KeywordRecord, SqliteTag>::attach(
        db, "compatible_records");
    EXPECT_TRUE(attached) << attached.error().message();

    auto opened = co_await Form<KeywordRecord, SqliteTag>::create_if_not_exists(
        db, "compatible_records");
    EXPECT_TRUE(opened) << opened.error().message();

    created = co_await db.execute(
        "CREATE TABLE incomplete_records(id INTEGER, extra TEXT)");
    if (!created) {
        ADD_FAILURE() << created.error().message();
        co_return {};
    }
    auto incomplete = co_await Form<KeywordRecord, SqliteTag>::attach(
        db, "incomplete_records");
    EXPECT_FALSE(incomplete);

    created = co_await db.execute(
        "CREATE TABLE incompatible_records(id INTEGER, \"from\" BLOB)");
    if (!created) {
        ADD_FAILURE() << created.error().message();
        co_return {};
    }
    auto incompatible = co_await Form<KeywordRecord, SqliteTag>::attach(
        db, "incompatible_records");
    EXPECT_FALSE(incompatible);
    co_return {};
}

static auto test_member_pointer_column_errors() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto form_ret = co_await Form<PartialReflectionRecord, SqliteTag>::create_if_not_exists(db, "partial_records");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(form_ret.value());

    auto id_col = form.sql<&PartialReflectionRecord::id>();
    EXPECT_TRUE(id_col.isValid());
    EXPECT_EQ(id_col.sql(), "\"id\"");

    auto valid_query = co_await form.select(form.sql<&PartialReflectionRecord::id>()).query();
    if (!valid_query) {
        ADD_FAILURE() << valid_query.error().message();
        co_return {};
    }

    auto invalid_col = form.sql(&PartialReflectionRecord::hidden);
    EXPECT_FALSE(invalid_col.isValid());

    auto invalid_where = co_await form.select().where(invalid_col == 1).query();
    EXPECT_FALSE(invalid_where.has_value());
    if (!invalid_where) {
        EXPECT_EQ(invalid_where.error(), SqlError::Code::InvalidParameter);
    }

    auto invalid_projection = co_await form.select(form.sql(&PartialReflectionRecord::hidden)).query();
    EXPECT_FALSE(invalid_projection.has_value());
    if (!invalid_projection) {
        EXPECT_EQ(invalid_projection.error(), SqlError::Code::InvalidParameter);
    }
    co_return {};
}

static auto test_identifier_errors() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto invalid_table = co_await Form<KeywordRecord, SqliteTag>::create_if_not_exists(db, "bad table");
    EXPECT_FALSE(invalid_table.has_value());
    if (!invalid_table) {
        EXPECT_EQ(invalid_table.error(), SqlError::Code::InvalidParameter);
    }

    auto duplicate_columns =
        co_await Form<DuplicateColumnRecord, SqliteTag>::create_if_not_exists(db, "duplicate_records");
    EXPECT_FALSE(duplicate_columns.has_value());
    if (!duplicate_columns) {
        EXPECT_EQ(duplicate_columns.error(), SqlError::Code::InvalidParameter);
    }

    auto form_ret = co_await Form<KeywordRecord, SqliteTag>::create_if_not_exists(db, "keyword_records");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(form_ret.value());

    EXPECT_THROW((void)form.as("bad alias"), std::invalid_argument);

    auto duplicate_relation =
        co_await form.join(form).on(form.col(&KeywordRecord::id) == form.col(&KeywordRecord::id)).query();
    EXPECT_FALSE(duplicate_relation.has_value());
    if (!duplicate_relation) {
        EXPECT_EQ(duplicate_relation.error(), SqlError::Code::InvalidParameter);
    }
    co_return {};
}

static auto test_rename_tag_identifier_roundtrip() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto form_ret = co_await Form<RenamedKeywordRecord, SqliteTag>::create_if_not_exists(db, "rename_records");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(form_ret.value());

    EXPECT_EQ(form.getColumnNames().size(), 2U);
    if (form.getColumnNames().size() != 2U) {
        co_return {};
    }
    EXPECT_EQ(form.getColumnNames()[1], "from");

    auto schema = form.createTableSchema();
    EXPECT_TRUE(schema.has_value());
    if (!schema) {
        co_return {};
    }
    EXPECT_NE(schema.value().find("\"from\""), std::string::npos);
    EXPECT_EQ(schema.value().find("from_value"), std::string::npos);

    auto if_not_exists_schema = form.createTableSchema(true);
    EXPECT_TRUE(if_not_exists_schema.has_value());
    if (!if_not_exists_schema) {
        co_return {};
    }
    EXPECT_NE(if_not_exists_schema.value().find("CREATE TABLE IF NOT EXISTS \"rename_records\""), std::string::npos);

    auto generated_schema =
        ILIAS_SQL_COMPLETE_NAMESPACE::detail::SchemaGenerator<SqliteTag>::generateTableSchema<RenamedKeywordRecord>(
            "rename_records", true);
    EXPECT_TRUE(generated_schema.has_value());
    if (!generated_schema) {
        co_return {};
    }
    EXPECT_EQ(generated_schema->createTableSql, if_not_exists_schema.value());
    EXPECT_EQ(generated_schema->indexStatements.size(), 1U);
    if (generated_schema->indexStatements.size() != 1U) {
        co_return {};
    }
    EXPECT_NE(generated_schema->indexStatements.front().find("\"idx_rename_records_from\""), std::string::npos);
    EXPECT_NE(generated_schema->indexStatements.front().find("\"rename_records\""), std::string::npos);
    EXPECT_NE(generated_schema->indexStatements.front().find("\"from\""), std::string::npos);
    auto complete_schema = form.completeSchema();
    EXPECT_EQ(complete_schema.size(), 2U);
    if (complete_schema.size() != 2U) {
        co_return {};
    }
    EXPECT_EQ(complete_schema.front(), schema.value());
    EXPECT_EQ(complete_schema.back(), generated_schema->indexStatements.front());

    auto index_statements = form.indexStatementsSchema();
    EXPECT_EQ(index_statements.size(), 1U);
    if (index_statements.size() != 1U) {
        co_return {};
    }
    EXPECT_EQ(index_statements, generated_schema->indexStatements);

    auto insert_ret = co_await form.emplace(1, "hello");
    if (!insert_ret) {
        ADD_FAILURE() << insert_ret.error().message();
        co_return {};
    }

    auto projected_ret = co_await form.select(form.sql(&RenamedKeywordRecord::from_value))
                             .where(form.sql(&RenamedKeywordRecord::id) == 1)
                             .query();
    if (!projected_ret) {
        ADD_FAILURE() << projected_ret.error().message();
        co_return {};
    }
    ilias_for_await(auto row, projected_ret.value().rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_EQ(std::get<0>(row.value()), "hello");
    }

    auto full_ret = co_await form.select().where(form.sql(&RenamedKeywordRecord::id) == 1).query();
    if (!full_ret) {
        ADD_FAILURE() << full_ret.error().message();
        co_return {};
    }
    ilias_for_await(auto row, full_ret.value().rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_EQ(row.value().id, 1);
        EXPECT_EQ(row.value().from_value, "hello");
    }
    co_return {};
}

static auto test_ignore_tag_roundtrip() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto form_ret = co_await Form<IgnoredFieldRecord, SqliteTag>::create_if_not_exists(db, "ignored_records");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(form_ret.value());

    EXPECT_EQ(form.getColumnNames().size(), 2U);
    if (form.getColumnNames().size() != 2U) {
        co_return {};
    }
    EXPECT_EQ(form.getColumnNames()[0], "id");
    EXPECT_EQ(form.getColumnNames()[1], "display_name");
    EXPECT_FALSE(form.getColumnName(&IgnoredFieldRecord::transient).has_value());
    EXPECT_FALSE(form.col(&IgnoredFieldRecord::transient).isValid());
    auto renamed_column = form.sql<&IgnoredFieldRecord::name>();
    EXPECT_TRUE(renamed_column.isValid());
    EXPECT_EQ(renamed_column.sql(), "\"display_name\"");

    auto schema = form.createTableSchema();
    if (!schema) {
        ADD_FAILURE() << schema.error().message();
        co_return {};
    }
    EXPECT_NE(schema->find("\"display_name\""), std::string::npos);
    EXPECT_EQ(schema->find("transient"), std::string::npos);

    auto first_insert = co_await form.emplace(1, UnsupportedTransientState {"never persisted"}, "Alice");
    if (!first_insert) {
        ADD_FAILURE() << first_insert.error().message();
        co_return {};
    }

    auto second_insert =
        co_await form.insert().set(IgnoredFieldRecord {2, UnsupportedTransientState {"also ignored"}, "Bob"}).execute();
    if (!second_insert) {
        ADD_FAILURE() << second_insert.error().message();
        co_return {};
    }

    auto rows_ret = co_await form.select().orderBy(form.sql(&IgnoredFieldRecord::id)).query();
    if (!rows_ret) {
        ADD_FAILURE() << rows_ret.error().message();
        co_return {};
    }
    std::vector<IgnoredFieldRecord> rows;
    ilias_for_await(auto row, rows_ret->rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        rows.push_back(std::move(row.value()));
    }
    EXPECT_EQ(rows.size(), 2U);
    if (rows.size() != 2U) {
        co_return {};
    }
    EXPECT_EQ(rows[0].name, "Alice");
    EXPECT_EQ(rows[1].name, "Bob");
    EXPECT_EQ(rows[0].transient.value, "local-default");
    EXPECT_EQ(rows[1].transient.value, "local-default");

    std::ostringstream printed;
    co_await form.print(50, printed);
    EXPECT_EQ(printed.str().find("transient"), std::string::npos);

    auto alias = form.as("ignored_alias");
    auto join_ret =
        co_await form.join(alias).on(form.col(&IgnoredFieldRecord::id) == alias.col(&IgnoredFieldRecord::id)).query();
    if (!join_ret) {
        ADD_FAILURE() << join_ret.error().message();
        co_return {};
    }
    std::size_t joined_rows = 0;
    ilias_for_await(auto row, join_ret->rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_EQ(std::get<0>(row.value()).transient.value, "local-default");
        EXPECT_EQ(std::get<1>(row.value()).transient.value, "local-default");
        ++joined_rows;
    }
    EXPECT_EQ(joined_rows, 2U);
    co_return {};
}

static auto test_transaction_bound_builders() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto created = co_await Form<KeywordRecord, SqliteTag>::create_if_not_exists(
        db, "transaction_records");
    if (!created) {
        ADD_FAILURE() << created.error().message();
        co_return {};
    }

    auto tx_ret = co_await db.transaction();
    if (!tx_ret) {
        ADD_FAILURE() << tx_ret.error().message();
        co_return {};
    }
    auto tx = std::move(tx_ret.value());

    auto bound =
        Form<KeywordRecord, SqliteTag>::bind(tx, "transaction_records");
    if (!bound) {
        ADD_FAILURE() << bound.error().message();
        co_return {};
    }

    auto inserted =
        co_await bound->insert().set(KeywordRecord {1, "created"}).execute();
    if (!inserted) {
        ADD_FAILURE() << inserted.error().message();
        co_return {};
    }
    auto updated =
        co_await bound->update()
            .set(bound->sql(&KeywordRecord::from_value) =
                     std::string("updated"))
            .where(bound->sql(&KeywordRecord::id) == 1)
            .execute();
    if (!updated) {
        ADD_FAILURE() << updated.error().message();
        co_return {};
    }
    auto upserted =
        co_await bound->upsert()
            .values(bound->sql(&KeywordRecord::id) = 1,
                    bound->sql(&KeywordRecord::from_value) =
                        std::string("upserted"))
            .onConflict(bound->sql(&KeywordRecord::id))
            .updateExcluded(bound->sql(&KeywordRecord::from_value))
            .execute();
    if (!upserted) {
        ADD_FAILURE() << upserted.error().message();
        co_return {};
    }
    auto selected =
        co_await bound->select()
            .where(bound->sql(&KeywordRecord::id) == 1)
            .query();
    if (!selected) {
        ADD_FAILURE() << selected.error().message();
        co_return {};
    }
    ilias_for_await(auto row, selected->rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_EQ(row->from_value, "upserted");
    }

    auto committed = co_await tx.commit();
    if (!committed) {
        ADD_FAILURE() << committed.error().message();
    }
    co_return {};
}

static auto test_upsert_merge_policies() -> IoTask<void> {
    auto db_ret = co_await SqlDatabase::open_in_memory();
    if (!db_ret) {
        ADD_FAILURE() << db_ret.error().message();
        co_return {};
    }
    auto db = std::move(db_ret.value());

    auto form_ret =
        co_await Form<MergeRecord, SqliteTag>::create_if_not_exists(db, "merge_records");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(*form_ret);

    auto initial =
        co_await form.upsert()
            .values(form.sql(&MergeRecord::id) = 1,
                    form.sql(&MergeRecord::details) =
                        std::optional<std::string> {"full"},
                    form.sql(&MergeRecord::metadata_level) = 1)
            .onConflict(form.sql(&MergeRecord::id))
            .updateExcluded(form.sql(&MergeRecord::details),
                            form.sql(&MergeRecord::metadata_level))
            .execute();
    if (!initial) {
        ADD_FAILURE() << initial.error().message();
        co_return {};
    }

    auto summary =
        co_await form.upsert()
            .values(form.sql(&MergeRecord::id) = 1,
                    form.sql(&MergeRecord::details) =
                        std::optional<std::string> {},
                    form.sql(&MergeRecord::metadata_level) = 0)
            .onConflict(form.sql(&MergeRecord::id))
            .updateCoalesced(form.sql(&MergeRecord::details))
            .updateGreatest(form.sql(&MergeRecord::metadata_level))
            .execute();
    if (!summary) {
        ADD_FAILURE() << summary.error().message();
        co_return {};
    }

    auto mergedUpdate =
        co_await form.update()
            .set(form.assignCoalesced(
                     form.sql(&MergeRecord::details),
                     std::optional<std::string> {}),
                 form.assignGreatest(
                     form.sql(&MergeRecord::metadata_level), 0))
            .where(form.sql(&MergeRecord::id) == 1)
            .execute();
    if (!mergedUpdate) {
        ADD_FAILURE() << mergedUpdate.error().message();
        co_return {};
    }

    auto partialInsert =
        co_await form.insert()
            .set(form.sql(&MergeRecord::id) = 2,
                 form.sql(&MergeRecord::metadata_level) = 7)
            .execute();
    if (!partialInsert) {
        ADD_FAILURE() << partialInsert.error().message();
        co_return {};
    }

    auto selected =
        co_await form.select().where(form.sql(&MergeRecord::id) == 1).query();
    if (!selected) {
        ADD_FAILURE() << selected.error().message();
        co_return {};
    }
    ilias_for_await(auto row, selected->rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_EQ(row->details, std::optional<std::string> {"full"});
        EXPECT_EQ(row->metadata_level, 1);
    }

    auto partial =
        co_await form.select().where(form.sql(&MergeRecord::id) == 2).query();
    if (!partial) {
        ADD_FAILURE() << partial.error().message();
        co_return {};
    }
    ilias_for_await(auto row, partial->rangeResult()) {
        if (!row) {
            ADD_FAILURE() << row.error().message();
            co_return {};
        }
        EXPECT_FALSE(row->details.has_value());
        EXPECT_EQ(row->metadata_level, 7);
    }
    co_return {};
}

TEST(SqlIdentifierQuoting, ReservedIdentifiersWorkThroughOrmDsl) {
    test_reserved_identifier_roundtrip().wait();
}

TEST(SqlIdentifierQuoting, DeterministicIdentifierErrorsAreReportedEarly) {
    test_identifier_errors().wait();
}

TEST(SqlIdentifierQuoting, MemberPointerColumnErrorsFlowThroughIoResult) {
    test_member_pointer_column_errors().wait();
}

TEST(SqlIdentifierQuoting, RenameTagAppliesToSqlIdentifiers) {
    test_rename_tag_identifier_roundtrip().wait();
}

TEST(SqlIdentifierQuoting, IgnoreTagExcludesNonPersistentFields) {
    test_ignore_tag_roundtrip().wait();
}

TEST(SqlIdentifierQuoting, BuildersCanBindToExistingTransaction) {
    test_transaction_bound_builders().wait();
}

TEST(SqlIdentifierQuoting, OrmSupportsPartialInsertAndMergePolicies) {
    test_upsert_merge_policies().wait();
}

TEST(SqlIdentifierQuoting, AttachRequiresOnlyUsableMappedColumns) {
    test_compatible_form_attachment().wait();
}

int main(int argc, char **argv) {
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ::testing::InitGoogleTest(&argc, argv);
    return sqlite_test::runAllTests();
}
