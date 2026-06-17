#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include <ilias/platform.hpp>
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql_orm/orm_form.hpp"

ILIAS_SQL_USE_NAMESPACE
using namespace ILIAS_NAMESPACE;
NEKO_USE_NAMESPACE

struct KeywordRecord {
    int         id         = 0;
    std::string from_value = "";
};

struct DuplicateColumnRecord {
    int id    = 0;
    int other = 0;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<KeywordRecord, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags::createPrimaryKeyTags(false)>(&KeywordRecord::id),
        "from", make_tags<SqlTags {.not_null = true}>(&KeywordRecord::from_value));
};

template <>
struct Meta<DuplicateColumnRecord, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags::createPrimaryKeyTags(false)>(&DuplicateColumnRecord::id),
        "id", make_tags<SqlTags {}>(&DuplicateColumnRecord::other));
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

    EXPECT_EQ(Dialect<SqliteTag>::generate_index_statements("select", columns).front(),
              "CREATE INDEX \"idx_select_from\" ON \"select\" (\"from\")");
    EXPECT_EQ(Dialect<MysqlTag>::generate_index_statements("select", columns).front(),
              "CREATE INDEX `idx_select_from` ON `select` (`from`)");
    EXPECT_EQ(Dialect<PostgresTag>::generate_index_statements("select", columns).front(),
              "CREATE INDEX \"idx_select_from\" ON \"select\" (\"from\")");
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

    auto verify_ret = co_await form.select(form.sql(&KeywordRecord::from_value))
                          .where(form.sql(&KeywordRecord::id) == 1)
                          .query();
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
    EXPECT_EQ(invalid_table.error(), SqlError::Code::InvalidParameter);

    auto duplicate_columns =
        co_await Form<DuplicateColumnRecord, SqliteTag>::create_if_not_exists(db, "duplicate_records");
    EXPECT_FALSE(duplicate_columns.has_value());
    EXPECT_EQ(duplicate_columns.error(), SqlError::Code::InvalidParameter);

    auto form_ret = co_await Form<KeywordRecord, SqliteTag>::create_if_not_exists(db, "keyword_records");
    if (!form_ret) {
        ADD_FAILURE() << form_ret.error().message();
        co_return {};
    }
    auto form = std::move(form_ret.value());

    EXPECT_THROW((void)form.as("bad alias"), std::invalid_argument);

    auto duplicate_relation = co_await form.join(form)
                                  .on(form.col(&KeywordRecord::id) == form.col(&KeywordRecord::id))
                                  .query();
    EXPECT_FALSE(duplicate_relation.has_value());
    EXPECT_EQ(duplicate_relation.error(), SqlError::Code::InvalidParameter);
}

TEST(SqlIdentifierQuoting, ReservedIdentifiersWorkThroughOrmDsl) {
    test_reserved_identifier_roundtrip().wait();
}

TEST(SqlIdentifierQuoting, DeterministicIdentifierErrorsAreReportedEarly) {
    test_identifier_errors().wait();
}

int main(int argc, char **argv) {
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
