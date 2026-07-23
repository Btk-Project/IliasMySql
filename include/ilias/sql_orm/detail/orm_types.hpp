#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/detail/reflection_metadata.hpp"
#include "ilias/sql_orm/detail/orm_traits.hpp"

#include <nekoproto/global/reflection_tags.hpp>
#include <nekoproto/global/string_literal.hpp>

#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

ILIAS_SQL_NS_BEGIN

// 标签定义
struct ILIAS_SQL_API SqlTags {
    // 核心约束
    bool primary_key    = false; // 主键约束
    bool not_null       = false; // 非空约束
    bool unique         = false; // 唯一约束
    bool auto_increment = false; // 自增约束
    bool index          = false; // 声明该列需要被索引

    // 数据类型修饰符
    bool unsigned_type = false; // 无符号类型 (适用于整数)
    int  length        = 0;     // 字符串长度

    // 常用逻辑标记 (用于自动化生成时间戳)
    bool created_at = false; // 标记为创建时间字段
    bool updated_at = false; // 标记为更新时间字段

    /**
     * @brief Create SqlTags configuration for a primary key field
     *
     * Helper method for common primary key configuration.
     * @param autoIncrement Whether the primary key should auto-increment
     * @return SqlTags configured for primary key usage
     */
    static constexpr decltype(auto) createPrimaryKeyTags(bool autoIncrement = false) {
        return SqlTags {.primary_key = true, .not_null = true, .unique = true, .auto_increment = autoIncrement};
    }
    /**
     * @brief Create SqlTags configuration for a unique indexed field
     *
     * Helper method for fields that need unique constraint and indexing.
     * @param length String length for VARCHAR fields (0 for TEXT)
     * @return SqlTags configured for unique indexed field
     */
    static constexpr decltype(auto) createUniqueIndexTags(int length = 0) {
        return SqlTags {.not_null = true, .unique = true, .index = true, .length = length};
    }
    /**
     * @brief Create SqlTags configuration for a timestamp field
     *
     * Helper method for automatic timestamp fields.
     * @param isCreatedAt True for created_at behavior, false for updated_at
     * @return SqlTags configured for timestamp automation
     */
    static constexpr decltype(auto) createTimestampTags(bool isCreatedAt = true) {
        return SqlTags {.not_null = true, .created_at = isCreatedAt, .updated_at = !isCreatedAt};
    }
    /**
     * @brief Create SqlTags configuration for a string field with length constraint
     *
     * Helper method for VARCHAR fields with specific length.
     * @param length Maximum string length
     * @param required Whether the field is required (not null)
     * @param indexed Whether the field should be indexed
     * @return SqlTags configured for string field
     */
    static constexpr decltype(auto) createStringTags(int length, bool required = true, bool indexed = false) {
        return SqlTags {.not_null = required, .index = indexed, .length = length};
    }
    /**
     * @brief Create SqlTags configuration for a numeric field
     *
     * Helper method for numeric fields with optional unsigned constraint.
     * @param required Whether the field is required (not null)
     * @param isUnsigned Whether the field should be unsigned
     * @return SqlTags configured for numeric field
     */
    static constexpr decltype(auto) createNumericTags(bool required = true, bool isUnsigned = false) {
        return SqlTags {.not_null = required, .unsigned_type = isUnsigned};
    }

    // 验证方法
    template <typename T>
    std::vector<std::string> getValidationErrors() const {
        std::vector<std::string> errors;
        // 1. 长度检查
        if (length < 0) {
            errors.push_back("Length cannot be negative");
        }

        // 2. 主键逻辑
        // 2. 索引与约束的逻辑一致性
        // 如果设置了 auto_increment，通常要求该列必须是主键或有唯一索引
        // (虽然部分数据库允许非唯一索引自增，但在ORM层严加限制有助于移植性)
        if (auto_increment && !primary_key && !unique && !index) {
            errors.push_back("Auto-increment field should be a Primary Key or have a Unique/Index constraint");
        }

        // 3. 时间戳行为冲突检查
        // 虽然理论上一个字段可以既是 created_at 又是 updated_at，但这通常是设计错误
        if (created_at && updated_at) {
            errors.push_back("Field cannot be both 'created_at' and 'updated_at' (ambiguous behavior)");
        }

        // 3. 自增逻辑
        if (auto_increment) {
            // 自增列通常必须是主键或唯一键
            if (!primary_key && !unique && !index) {
                // MySQL 强制要求 auto_increment 字段必须是 key
                // SQLite 也是如此 (Primary Key)
                errors.push_back("Auto-increment field must be defined as a primary key or unique index");
            }
        }

        using RawType = detail::strip_wrapper_t<T>;

        // 1. 自增约束检查
        if (auto_increment) {
            if constexpr (!std::is_integral_v<RawType>) {
                errors.push_back("Auto-increment is only allowed for integral types");
            }
            if constexpr (std::is_same_v<RawType, bool>) {
                errors.push_back("Auto-increment cannot be used with boolean");
            }
        }

        // 2. 无符号检查
        if (unsigned_type) {
            if constexpr (!std::is_integral_v<RawType> && !std::is_floating_point_v<RawType>) {
                errors.push_back("Unsigned attribute is only allowed for numeric types");
            }
            if constexpr (std::is_same_v<RawType, bool>) {
                errors.push_back("Unsigned attribute cannot be used with boolean");
            }
        }

        // 3. 长度属性检查 (Length)
        if (length > 0) {
            constexpr bool is_string_type = std::is_same_v<RawType, std::string> ||
                                            std::is_same_v<RawType, std::string_view> ||
                                            std::is_same_v<RawType, const char *>;

            if constexpr (!is_string_type) {
                errors.push_back("Length attribute is only allowed for string types");
            }
        }

        // 4. 时间戳字段检查
        if (created_at || updated_at) {
            // 允许的时间类型：SqlDate (std::chrono::system_clock::time_point)
            // 或者可能是 string (ISO8601) 或 int (timestamp)
            constexpr bool is_time_compatible = std::is_same_v<RawType, SqlDate> ||
                                                std::is_same_v<RawType, std::string> ||
                                                (std::is_integral_v<RawType> && sizeof(RawType) >= sizeof(int32_t));
            if constexpr (!is_time_compatible) {
                errors.push_back("Timestamp behavior (created_at/updated_at) requires a compatible date/time type");
            }
        }

        return errors;
    }
    template <typename T>
    bool isValid() const {
        return getValidationErrors<T>().empty();
    }
    // 约束组合辅助方法
    bool hasTimestampBehavior() const;
    bool requiresIndex() const;

    template <typename T, auto tags>
    constexpr static bool constexpr_check() {
        using RawType = detail::strip_wrapper_t<T>;
        if constexpr (tags.primary_key) {
            static_assert(tags.not_null, "Primary key fields must be marked as 'not_null'");
            static_assert(tags.unique, "Primary key fields must be marked as 'unique'");
        }

        if constexpr (tags.auto_increment) {
            static_assert(tags.primary_key || tags.unique || tags.index,
                          "Auto-increment field must be a Primary Key or have a Unique/Index constraint");
        }

        if constexpr (tags.auto_increment) {
            static_assert(std::is_integral_v<RawType>,
                          "Auto-increment is only allowed for integral types (e.g., int, long)");
        }

        if constexpr (tags.unsigned_type) {
            static_assert(std::is_integral_v<RawType> || std::is_floating_point_v<RawType>,
                          "Unsigned attribute is only allowed for numeric types");
            static_assert(!std::is_same_v<RawType, bool>, "Unsigned attribute cannot be used with boolean type");
        }

        if constexpr (tags.length > 0) {
            constexpr bool is_string_type = std::is_same_v<RawType, std::string> ||
                                            std::is_same_v<RawType, std::string_view> ||
                                            std::is_same_v<RawType, const char *>;
            static_assert(is_string_type, "Length attribute is only allowed for string types");
        }

        if constexpr (tags.created_at && tags.updated_at) {
            static_assert(!tags.created_at || !tags.updated_at,
                          "Field cannot be marked as both 'created_at' and 'updated_at'");
        }

        if constexpr (tags.created_at || tags.updated_at) {
            constexpr bool is_time_compatible =
                std::is_same_v<RawType, SqlDate> ||                                  // 自定义日期时间类型
                std::is_same_v<RawType, std::string> ||                              // ISO 8601 字符串
                (std::is_integral_v<RawType> && sizeof(RawType) >= sizeof(int32_t)); // Unix 时间戳

            static_assert(is_time_compatible, "Timestamp behavior (created_at/updated_at) requires a compatible type "
                                              "(e.g., SqlDate, integer, or string)");
        }

        return true;
    }
};

/**
 * @brief Referential actions shared by the supported SQL dialects.
 *
 * NoAction is also used as the default value and therefore omits the
 * corresponding ON DELETE / ON UPDATE clause.
 */
enum class SqlReferenceAction {
    NoAction,
    Restrict,
    Cascade,
    SetNull,
    SetDefault,
};

/** @brief Stable insertion points for raw column-definition fragments. */
enum class SqlCustomPosition {
    AfterType,
    Tail,
};

/** @brief Sort direction for one column in a table-level index. */
enum class SqlIndexOrder {
    Asc,
    Desc,
};

namespace sql_tag_detail {

template <NEKO_NAMESPACE::ConstexprString Expression>
struct sql_default_impl {
    static_assert(Expression.size() > 0, "SQL default expression must not be empty");
    constexpr static auto sql_default_expression = Expression.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <NEKO_NAMESPACE::ConstexprString Expression>
struct sql_check_impl {
    static_assert(Expression.size() > 0, "SQL check expression must not be empty");
    constexpr static auto sql_check_expression = Expression.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <NEKO_NAMESPACE::ConstexprString Collation>
struct sql_collate_impl {
    static_assert(Collation.size() > 0, "SQL collation name must not be empty");
    constexpr static auto sql_collation = Collation.view();

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <NEKO_NAMESPACE::ConstexprString Table, NEKO_NAMESPACE::ConstexprString Column, SqlReferenceAction OnDelete,
          SqlReferenceAction OnUpdate>
struct sql_references_impl {
    static_assert(Table.size() > 0, "SQL referenced table must not be empty");
    static_assert(Column.size() > 0, "SQL referenced column must not be empty");

    constexpr static auto sql_reference_table  = Table.view();
    constexpr static auto sql_reference_column = Column.view();
    constexpr static auto sql_on_delete        = OnDelete;
    constexpr static auto sql_on_update        = OnUpdate;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <NEKO_NAMESPACE::ConstexprString Fragment, NEKO_NAMESPACE::ConstexprString Backend, SqlCustomPosition Position>
struct sql_custom_impl {
    static_assert(Fragment.size() > 0, "Custom SQL fragment must not be empty");

    constexpr static auto sql_custom_fragment = Fragment.view();
    constexpr static auto sql_custom_backend  = Backend.view();
    constexpr static auto sql_custom_position = Position;

    template <typename T, auto /*tags*/>
    constexpr static bool constexpr_check() {
        return true;
    }
};

template <auto... Members>
struct sql_table_primary_key_impl {
    static_assert(sizeof...(Members) > 1, "Table primary keys are intended for two or more columns");
    static_assert((std::is_member_object_pointer_v<decltype(Members)> && ...),
                  "Table primary key columns must be member pointers");

    constexpr static bool primary_key = true;

    template <typename Visitor>
    static constexpr void forEachMember(Visitor &&visitor) {
        (visitor.template operator()<Members>(), ...);
    }
};

template <auto... Members>
struct sql_table_unique_impl {
    static_assert(sizeof...(Members) > 1, "Table unique constraints are intended for two or more columns");
    static_assert((std::is_member_object_pointer_v<decltype(Members)> && ...),
                  "Table unique columns must be member pointers");

    constexpr static bool unique = true;

    template <typename Visitor>
    static constexpr void forEachMember(Visitor &&visitor) {
        (visitor.template operator()<Members>(), ...);
    }
};

template <NEKO_NAMESPACE::ConstexprString Expression>
struct sql_table_check_impl {
    static_assert(Expression.size() > 0, "Table CHECK expression must not be empty");
    constexpr static auto expression = Expression.view();
    constexpr static bool check      = true;
};

template <auto Member, SqlIndexOrder Order>
struct sql_index_column_impl {
    static_assert(std::is_member_object_pointer_v<decltype(Member)>, "Index columns must be member pointers");
    constexpr static auto          member = Member;
    constexpr static SqlIndexOrder order  = Order;
};

template <NEKO_NAMESPACE::ConstexprString Name, bool Unique, auto... Columns>
struct sql_table_index_impl {
    static_assert(Name.size() > 0, "SQL index name must not be empty");
    static_assert(sizeof...(Columns) > 0, "SQL indexes require at least one column");

    constexpr static auto name   = Name.view();
    constexpr static bool unique = Unique;
    constexpr static bool index  = true;

    template <typename Visitor>
    static constexpr void forEachColumn(Visitor &&visitor) {
        (visitor.template operator()<std::remove_cvref_t<decltype(Columns)>>(), ...);
    }
};

} // namespace sql_tag_detail

/**
 * @brief Use a trusted SQL expression as the column default.
 *
 * The expression is emitted verbatim after DEFAULT. String literals therefore
 * need SQL quoting, for example sql_default<"'pending'">.
 */
template <NEKO_NAMESPACE::ConstexprString Expression>
inline constexpr auto sql_default = sql_tag_detail::sql_default_impl<Expression> {};

/** @brief Add a column CHECK constraint. The generator supplies CHECK (...). */
template <NEKO_NAMESPACE::ConstexprString Expression>
inline constexpr auto sql_check = sql_tag_detail::sql_check_impl<Expression> {};

/** @brief Set the column collation using a validated SQL identifier path. */
template <NEKO_NAMESPACE::ConstexprString Collation>
inline constexpr auto sql_collate = sql_tag_detail::sql_collate_impl<Collation> {};

/**
 * @brief Add a single-column foreign key constraint.
 *
 * It is emitted as a table constraint so MySQL enforces it as well. Composite
 * foreign keys still belong to explicit table-level schema metadata.
 */
template <NEKO_NAMESPACE::ConstexprString Table, NEKO_NAMESPACE::ConstexprString Column,
          SqlReferenceAction OnDelete = SqlReferenceAction::NoAction,
          SqlReferenceAction OnUpdate = SqlReferenceAction::NoAction>
inline constexpr auto sql_references = sql_tag_detail::sql_references_impl<Table, Column, OnDelete, OnUpdate> {};

/**
 * @brief Append a trusted raw fragment to a generated column definition.
 *
 * An empty backend applies to all dialects. Supported backend selectors are
 * sqlite, mysql/mariadb and postgres/postgresql/pg. Matching fragments remain
 * repeatable and are emitted in declaration order.
 */
template <NEKO_NAMESPACE::ConstexprString Fragment, NEKO_NAMESPACE::ConstexprString Backend = "",
          SqlCustomPosition Position = SqlCustomPosition::Tail>
inline constexpr auto sql_custom = sql_tag_detail::sql_custom_impl<Fragment, Backend, Position> {};

/**
 * @brief Add a composite primary key to SqlTableMeta<T>::value.
 *
 * Single-column primary keys should continue to use SqlTags::primary_key.
 */
template <auto... Members>
inline constexpr auto sql_primary_key = sql_tag_detail::sql_table_primary_key_impl<Members...> {};

/** @brief Add a composite UNIQUE constraint to SqlTableMeta<T>::value. */
template <auto... Members>
inline constexpr auto sql_unique = sql_tag_detail::sql_table_unique_impl<Members...> {};

/** @brief Add a trusted table-level CHECK expression to SqlTableMeta<T>::value. */
template <NEKO_NAMESPACE::ConstexprString Expression>
inline constexpr auto sql_table_check = sql_tag_detail::sql_table_check_impl<Expression> {};

/** @brief Describe an ascending or descending column in a table-level index. */
template <auto Member>
inline constexpr auto sql_asc = sql_tag_detail::sql_index_column_impl<Member, SqlIndexOrder::Asc> {};

template <auto Member>
inline constexpr auto sql_desc = sql_tag_detail::sql_index_column_impl<Member, SqlIndexOrder::Desc> {};

/** @brief Add a named, possibly composite index to SqlTableMeta<T>::value. */
template <NEKO_NAMESPACE::ConstexprString Name, auto... Columns>
inline constexpr auto sql_index = sql_tag_detail::sql_table_index_impl<Name, false, Columns...> {};

template <NEKO_NAMESPACE::ConstexprString Name, auto... Columns>
inline constexpr auto sql_unique_index = sql_tag_detail::sql_table_index_impl<Name, true, Columns...> {};

/**
 * @brief Collect table-level constraints and indexes.
 *
 * Specialize SqlTableMeta<T> and assign `sql_table(...)` to its `value`.
 */
template <typename... Items>
consteval auto sql_table(Items... items) {
    return std::tuple<Items...> {items...};
}

template <typename T>
struct SqlTableMeta {
    constexpr static auto value = std::tuple {};
};

namespace tag_properties {
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, primary_key, primary_key)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, not_null, not_null)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, unique, unique)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, auto_increment, auto_increment)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, index, index)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, unsigned_type, unsigned_type)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(int, length, length)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, created_at, created_at)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(bool, updated_at, updated_at)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, sql_default_expression, sql_default_expression)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, sql_check_expression, sql_check_expression)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, sql_collation, sql_collation)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, sql_reference_table, sql_reference_table)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(std::string_view, sql_reference_column, sql_reference_column)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(SqlReferenceAction, sql_on_delete, sql_on_delete)
NEKO_DETAIL_DEFINE_TAG_PROPERTY(SqlReferenceAction, sql_on_update, sql_on_update)
} // namespace tag_properties

/**
 * @brief Normalized SQL metadata for one reflected column.
 *
 * Core behavioral tags remain grouped in SqlTags. DDL fragments and
 * relationship metadata stay independent so new properties can be added
 * without continuously growing SqlTags.
 */
struct SqlCustomClause {
    std::string_view  fragment {};
    std::string_view  backend {};
    SqlCustomPosition position = SqlCustomPosition::Tail;
};

struct SqlColumnMetadata {
    SqlTags                      tags {};
    std::string_view             default_expression {};
    std::string_view             check_expression {};
    std::string_view             collation {};
    std::string_view             reference_table {};
    std::string_view             reference_column {};
    SqlReferenceAction           on_delete = SqlReferenceAction::NoAction;
    SqlReferenceAction           on_update = SqlReferenceAction::NoAction;
    std::vector<SqlCustomClause> custom_clauses {};

    [[nodiscard]] constexpr bool hasDefault() const noexcept { return !default_expression.empty(); }
    [[nodiscard]] constexpr bool hasCheck() const noexcept { return !check_expression.empty(); }
    [[nodiscard]] constexpr bool hasCollation() const noexcept { return !collation.empty(); }
    [[nodiscard]] constexpr bool hasReference() const noexcept {
        return !reference_table.empty() || !reference_column.empty();
    }

    template <typename T>
    auto getValidationErrors() const -> std::vector<std::string> {
        auto errors = tags.template getValidationErrors<T>();

        if (hasDefault() && tags.created_at) {
            errors.emplace_back("Explicit SQL default conflicts with created_at's automatic CURRENT_TIMESTAMP default");
        }
        if (hasDefault() && tags.auto_increment) {
            errors.emplace_back("Explicit SQL default conflicts with auto-increment behavior");
        }
        if (reference_table.empty() != reference_column.empty()) {
            errors.emplace_back("SQL reference requires both a table and a column");
        }
        if (!hasReference() &&
            (on_delete != SqlReferenceAction::NoAction || on_update != SqlReferenceAction::NoAction)) {
            errors.emplace_back("SQL referential actions require a reference target");
        }
        if ((tags.not_null || tags.primary_key) &&
            (on_delete == SqlReferenceAction::SetNull || on_update == SqlReferenceAction::SetNull)) {
            errors.emplace_back("SET NULL referential actions require a nullable column");
        }
        for (const auto &clause : custom_clauses) {
            if (clause.fragment.empty()) {
                errors.emplace_back("Custom SQL fragments must not be empty");
            }
        }

        if (hasCollation()) {
            using RawType                 = detail::strip_wrapper_t<T>;
            constexpr bool is_string_type = std::is_same_v<RawType, std::string> ||
                                            std::is_same_v<RawType, std::string_view> ||
                                            std::is_same_v<RawType, const char *>;
            if constexpr (!is_string_type) {
                errors.emplace_back("SQL collation is only supported for string fields");
            }
        }

        return errors;
    }
};

// 前置声明
class SqlDatabase;
template <typename T>
class SqlResult;
template <typename T>
class SqlStatement;

namespace detail {
class SqlCondition;
class SqlStatementBinder;
class SelectBuilder;
template <typename... ResultTypes>
class ProjectedSelectBuilder;
template <typename T>
class TypedColumn;
// 工具函数声明
ILIAS_SQL_API std::string join_strs(const std::vector<std::string> &vec, const std::string &sep,
                                    const std::string &prefix = "", const std::string &suffix = "");

template <typename Tags>
constexpr auto extractSqlTags(const Tags &tags) -> SqlTags {
    SqlTags ret;
#define ILIAS_SQL_EXTRACT_TAG_PROPERTY(Property, Member)                                                               \
    if constexpr (NEKO_NAMESPACE::tag_query::has<tag_properties::Property>(Tags {})) {                                 \
        ret.Member = NEKO_NAMESPACE::tag_query::get<tag_properties::Property>(tags);                                   \
    }
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(primary_key, primary_key)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(not_null, not_null)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(unique, unique)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(auto_increment, auto_increment)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(index, index)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(unsigned_type, unsigned_type)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(length, length)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(created_at, created_at)
    ILIAS_SQL_EXTRACT_TAG_PROPERTY(updated_at, updated_at)
#undef ILIAS_SQL_EXTRACT_TAG_PROPERTY
    return ret;
}

template <typename Tags, typename Visitor>
constexpr void forEachSqlTag(const Tags &tags, Visitor &&visitor) {
    if constexpr (NEKO_NAMESPACE::is_tag_list_v<std::remove_cvref_t<Tags>>) {
        std::apply([&visitor](const auto &...tag) { (visitor(tag), ...); }, tags.tuple());
    }
    else {
        visitor(tags);
    }
}

template <typename Tags>
constexpr auto extractSqlColumnMetadata(const Tags &tags) -> SqlColumnMetadata {
    SqlColumnMetadata ret {.tags = extractSqlTags(tags)};
#define ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(Property, Member)                                                            \
    if constexpr (NEKO_NAMESPACE::tag_query::has<tag_properties::Property>(Tags {})) {                                 \
        ret.Member = NEKO_NAMESPACE::tag_query::get<tag_properties::Property>(tags);                                   \
    }
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_default_expression, default_expression)
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_check_expression, check_expression)
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_collation, collation)
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_reference_table, reference_table)
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_reference_column, reference_column)
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_on_delete, on_delete)
    ILIAS_SQL_EXTRACT_COLUMN_PROPERTY(sql_on_update, on_update)
#undef ILIAS_SQL_EXTRACT_COLUMN_PROPERTY
    forEachSqlTag(tags, [&ret](const auto &tag) {
        if constexpr (requires {
                          tag.sql_custom_fragment;
                          tag.sql_custom_backend;
                          tag.sql_custom_position;
                      }) {
            ret.custom_clauses.push_back({
                .fragment = tag.sql_custom_fragment,
                .backend  = tag.sql_custom_backend,
                .position = tag.sql_custom_position,
            });
        }
    });
    return ret;
}
} // namespace detail

ILIAS_SQL_NS_END
