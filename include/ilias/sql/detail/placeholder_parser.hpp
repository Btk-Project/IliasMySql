#pragma once

#include "ilias/sql/global/global.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>

ILIAS_SQL_NS_BEGIN
namespace detail {

enum class SqlPlaceholderDialect {
    Generic,
    MySql,
    Postgres,
};

enum class SqlPlaceholderKind {
    Positional,
    Named,
};

enum class SqlPlaceholderRewriteStyle {
    QuestionMark,
    PostgresNumbered,
};

struct SqlPlaceholder {
    SqlPlaceholderKind kind;
    std::size_t        position;
    std::size_t        length;
    std::size_t        ordinal;
    std::string_view   name;
};

struct SqlPlaceholderRewriteResult {
    std::string                                  sql;
    std::unordered_map<std::string, std::size_t> named_param_indices;
    std::size_t                                  parameter_count = 0;
};

constexpr auto sql_is_identifier_start(char c) -> bool {
    return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) || c == '_';
}

constexpr auto sql_is_identifier_body(char c) -> bool {
    return sql_is_identifier_start(c) || (c >= '0' && c <= '9');
}

constexpr auto sql_supports_dollar_quote(SqlPlaceholderDialect dialect) -> bool {
    return dialect == SqlPlaceholderDialect::Generic || dialect == SqlPlaceholderDialect::Postgres;
}

template <typename Fn>
constexpr auto for_each_sql_placeholder(std::string_view sql, SqlPlaceholderDialect dialect, Fn &&fn) -> std::size_t {
    enum class State {
        Normal,
        InString,
        InLineComment,
        InBlockComment,
        InDollarQuote,
    };

    State       state         = State::Normal;
    char        quote_char    = 0;
    std::size_t dollar_start  = 0;
    std::size_t dollar_length = 0;
    std::size_t ordinal       = 0;

    for (std::size_t i = 0; i < sql.size(); ++i) {
        const char c = sql[i];

        switch (state) {
            case State::Normal:
                if (c == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
                    ++i;
                    state = State::InLineComment;
                    continue;
                }
                if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
                    ++i;
                    state = State::InBlockComment;
                    continue;
                }
                if (sql_supports_dollar_quote(dialect) && c == '$') {
                    std::size_t j = i + 1;
                    while (j < sql.size() && sql_is_identifier_body(sql[j])) {
                        ++j;
                    }
                    if (j < sql.size() && sql[j] == '$') {
                        dollar_start  = i;
                        dollar_length = j - i + 1;
                        i             = j;
                        state         = State::InDollarQuote;
                        continue;
                    }
                }
                if (c == '\'' || c == '"' || c == '`') {
                    quote_char = c;
                    state      = State::InString;
                    continue;
                }
                if (c == '?') {
                    fn(SqlPlaceholder {
                        .kind     = SqlPlaceholderKind::Positional,
                        .position = i,
                        .length   = 1,
                        .ordinal  = ordinal++,
                        .name     = {},
                    });
                    continue;
                }
                if (c == ':') {
                    if (i + 1 < sql.size() && sql[i + 1] == ':') {
                        ++i;
                        continue;
                    }

                    const std::size_t start = i + 1;
                    if (start < sql.size() && sql_is_identifier_start(sql[start])) {
                        std::size_t end = start + 1;
                        while (end < sql.size() && sql_is_identifier_body(sql[end])) {
                            ++end;
                        }
                        fn(SqlPlaceholder {
                            .kind     = SqlPlaceholderKind::Named,
                            .position = i,
                            .length   = end - i,
                            .ordinal  = ordinal++,
                            .name     = sql.substr(start, end - start),
                        });
                        i = end - 1;
                    }
                }
                break;

            case State::InString:
                if (c == quote_char) {
                    if (i + 1 < sql.size() && sql[i + 1] == quote_char) {
                        ++i;
                    }
                    else {
                        state = State::Normal;
                    }
                }
                break;

            case State::InLineComment:
                if (c == '\n') {
                    state = State::Normal;
                }
                break;

            case State::InBlockComment:
                if (c == '*' && i + 1 < sql.size() && sql[i + 1] == '/') {
                    ++i;
                    state = State::Normal;
                }
                break;

            case State::InDollarQuote:
                if (c == '$' && i + 1 >= dollar_length) {
                    const auto candidate = sql.substr(i + 1 - dollar_length, dollar_length);
                    const auto tag       = sql.substr(dollar_start, dollar_length);
                    if (candidate == tag) {
                        state         = State::Normal;
                        dollar_start  = 0;
                        dollar_length = 0;
                    }
                }
                break;
        }
    }

    return ordinal;
}

constexpr auto count_sql_placeholders(std::string_view sql,
                                       SqlPlaceholderDialect dialect = SqlPlaceholderDialect::Generic) -> std::size_t {
    return for_each_sql_placeholder(sql, dialect, [](const SqlPlaceholder &) {});
}

template <std::size_t N>
consteval auto get_sql_placeholder_names(std::string_view   sql,
                                         SqlPlaceholderDialect dialect = SqlPlaceholderDialect::Generic)
    -> std::array<std::string_view, N> {
    std::array<std::string_view, N> names {};
    std::size_t                     index = 0;
    for_each_sql_placeholder(sql, dialect, [&](const SqlPlaceholder &placeholder) {
        if (index < N) {
            names[index++] = placeholder.kind == SqlPlaceholderKind::Named ? placeholder.name : std::string_view("?");
        }
    });
    return names;
}

inline auto rewrite_sql_placeholders(std::string_view sql, SqlPlaceholderDialect dialect,
                                     SqlPlaceholderRewriteStyle style) -> SqlPlaceholderRewriteResult {
    SqlPlaceholderRewriteResult result;
    result.sql.reserve(sql.size());

    std::size_t last = 0;
    result.parameter_count = for_each_sql_placeholder(sql, dialect, [&](const SqlPlaceholder &placeholder) {
        result.sql.append(sql.substr(last, placeholder.position - last));

        if (placeholder.kind == SqlPlaceholderKind::Named) {
            result.named_param_indices[std::string(placeholder.name)] = placeholder.ordinal;
        }

        switch (style) {
            case SqlPlaceholderRewriteStyle::QuestionMark:
                result.sql.push_back('?');
                break;

            case SqlPlaceholderRewriteStyle::PostgresNumbered:
                result.sql.push_back('$');
                result.sql.append(std::to_string(placeholder.ordinal + 1));
                break;
        }

        last = placeholder.position + placeholder.length;
    });

    result.sql.append(sql.substr(last));
    return result;
}

} // namespace detail
ILIAS_SQL_NS_END
