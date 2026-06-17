#include <gtest/gtest.h>

#include "ilias/sql/detail/placeholder_parser.hpp"
#include "ilias/sql/detail/type_traits.hpp"

ILIAS_SQL_USE_NAMESPACE

static_assert(count_sql_params("SELECT ':id?', \"?\", `:x`, /* ? */ -- :x\n"
                               "       $tag$:no ?$tag$, :id, ?") == 2);

static constexpr auto kParamNames = get_sql_param_names<3>("SELECT :id, ?, :name_2");
static_assert(kParamNames[0] == "id");
static_assert(kParamNames[1] == "?");
static_assert(kParamNames[2] == "name_2");

TEST(SqlPlaceholderParser, MySqlRewriteKeepsQuestionMarks) {
    auto parsed = detail::rewrite_sql_placeholders(
        "SELECT ':skip?', `:column`, :id, ? /* :ignored ? */ -- :ignored\n",
        detail::SqlPlaceholderDialect::MySql, detail::SqlPlaceholderRewriteStyle::QuestionMark);

    EXPECT_EQ(parsed.sql, "SELECT ':skip?', `:column`, ?, ? /* :ignored ? */ -- :ignored\n");
    EXPECT_EQ(parsed.parameter_count, 2U);
    ASSERT_EQ(parsed.named_param_indices.size(), 1U);
    EXPECT_EQ(parsed.named_param_indices.at("id"), 0U);
}

TEST(SqlPlaceholderParser, PostgresRewriteNumbersParams) {
    auto parsed = detail::rewrite_sql_placeholders(
        "SELECT $tag$:skip ?$tag$ AS body, :id::integer AS id, ? AS value -- :ignored ?\n",
        detail::SqlPlaceholderDialect::Postgres, detail::SqlPlaceholderRewriteStyle::PostgresNumbered);

    EXPECT_EQ(parsed.sql, "SELECT $tag$:skip ?$tag$ AS body, $1::integer AS id, $2 AS value -- :ignored ?\n");
    EXPECT_EQ(parsed.parameter_count, 2U);
    ASSERT_EQ(parsed.named_param_indices.size(), 1U);
    EXPECT_EQ(parsed.named_param_indices.at("id"), 0U);
}

TEST(SqlPlaceholderParser, NamedParameterStartsWithIdentifierStart) {
    auto parsed = detail::rewrite_sql_placeholders("SELECT :1 AS literal_colon, :_ok, :name2",
                                                   detail::SqlPlaceholderDialect::Generic,
                                                   detail::SqlPlaceholderRewriteStyle::QuestionMark);

    EXPECT_EQ(parsed.sql, "SELECT :1 AS literal_colon, ?, ?");
    EXPECT_EQ(parsed.parameter_count, 2U);
    ASSERT_EQ(parsed.named_param_indices.size(), 2U);
    EXPECT_EQ(parsed.named_param_indices.at("_ok"), 0U);
    EXPECT_EQ(parsed.named_param_indices.at("name2"), 1U);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
