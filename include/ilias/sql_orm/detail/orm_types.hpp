#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql_orm/detail/orm_traits.hpp"

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
} // namespace detail

ILIAS_SQL_NS_END