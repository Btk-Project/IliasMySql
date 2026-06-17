#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/detail/placeholder_parser.hpp"

#include <string_view>
#include <array>
#include <algorithm> // For std::sort, std::equal
#include <stdexcept> // For a better exception type
#include <type_traits>

// 假设这是你的反射库头文件
#include <nekoproto/serialization/reflection.hpp>

ILIAS_SQL_NS_BEGIN

/**
 * @brief 编译期计算SQL中的参数数量（支持 :name 和 ?）
 */
constexpr size_t count_sql_params(std::string_view sql) {
    return detail::count_sql_placeholders(sql);
}

/**
 * @brief 编译期从SQL中提取所有参数名称
 * 对于 :name，返回 "name"
 * 对于 ?，返回 "?"
 */
template <size_t N>
consteval auto get_sql_param_names(std::string_view sql) -> std::array<std::string_view, N> {
    return detail::get_sql_placeholder_names<N>(sql);
}

struct SQL_Placeholder_Count_Mismatch : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
struct SQL_Placeholder_Name_Mismatch : public std::runtime_error {
    using std::runtime_error::runtime_error;
};

template <typename U>
struct SqlStructCheck {
    std::string_view sql;

    /**
     * @brief 这是一个“立即函数”构造函数。
     *        参考 std::format 的 basic_format_string 实现。
     */
    template <typename T>
        requires std::convertible_to<const T &, std::string_view>
    consteval SqlStructCheck(const T &s) : sql(s) {
        std::string_view sql_view     = sql;
        size_t           param_count  = count_sql_params(sql_view);
        constexpr size_t member_count = NEKO_NAMESPACE::Reflect<U>::size();
        if (param_count != member_count) {
            throw SQL_Placeholder_Count_Mismatch("Error: SQL placeholder count does not match struct member count!");
        }
        if constexpr (member_count > 0) {
            auto sql_names    = get_sql_param_names<member_count>(sql_view);
            auto member_names = NEKO_NAMESPACE::Reflect<U>::names();

            std::ranges::sort(sql_names);
            std::ranges::sort(member_names);

            if (!std::ranges::equal(sql_names, member_names)) {
                throw SQL_Placeholder_Name_Mismatch("Error: SQL placeholder names do not match struct member names!");
            }
        }
    }
};

/**
 * @brief (不变) 用于可变参数的SQL检查器，只检查数量。
 *        适用于非结构体绑定的场景。
 */
template <typename T>
    requires NEKO_NAMESPACE::detail::is_std_tuple_v<T>
struct SqlCheck {
    template <typename U>
    SqlCheck(SqlStructCheck<U> sqls) : sql(sqls.sql) {
        size_t param_count  = count_sql_params(sql);
        size_t member_count = NEKO_NAMESPACE::Reflect<U>::size();
        if (param_count != member_count) {
            throw SQL_Placeholder_Count_Mismatch("Error: SQL placeholder count does not match struct member count!");
        }
    }
    consteval SqlCheck(const char *s) : sql(s) {
        size_t param_count = count_sql_params(sql);
        size_t args_count  = std::tuple_size_v<T>;
        if (param_count != args_count) {
            throw SQL_Placeholder_Count_Mismatch("Error: SQL placeholder count does not match argument count!");
        }
    }
    std::string_view sql;
};

template <typename T>
struct StorageSelector {
    using type = std::remove_reference_t<T>;
    static constexpr std::string_view debug() {
        return "copy";
    }
};

template <typename T>
struct StorageSelector<T&> {
    using type = T&;
    static constexpr std::string_view debug() {
        return "reference";
    }
};

template <typename T>
using StorageType_t = typename StorageSelector<T>::type;

ILIAS_SQL_NS_END
