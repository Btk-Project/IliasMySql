#pragma once

#include "ilias/sql/global/global.hpp"

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
    size_t count = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        // 处理 ? 占位符
        if (sql[i] == '?') {
            count++;
            continue;
        }
        
        // 处理 :name 占位符
        if (sql[i] == ':') {
            if (i + 1 >= sql.size()) {
                break;
            }
            // 处理双冒号转义 :: (PostgreSQL type cast etc.)
            if (sql[i + 1] == ':') {
                i++;
                continue;
            }

            char next = sql[i + 1];
            // 检查是否为合法的标识符开始
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_') {
                count++;
                // 跳过标识符剩余部分
                while (i + 1 < sql.size()) {
                    char c = sql[i + 1];
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                        i++;
                    }
                    else {
                        break;
                    }
                }
            }
        }
    }
    return count;
}

/**
 * @brief 编译期从SQL中提取所有参数名称
 * 对于 :name，返回 "name"
 * 对于 ?，返回 "?"
 */
template <size_t N>
consteval auto get_sql_param_names(std::string_view sql) -> std::array<std::string_view, N> {
    std::array<std::string_view, N> names {};
    size_t                          current_index = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        // 处理 ? 占位符
        if (sql[i] == '?') {
            if (current_index < N) {
                // 将 "?" 作为一个特殊的名称存入
                names[current_index++] = sql.substr(i, 1);
            }
            continue;
        }

        // 处理 :name 占位符
        if (sql[i] == ':') {
            if (i + 1 >= sql.size()) {
                break;
            }
            if (sql[i + 1] == ':') {
                i++;
                continue;
            }

            char next = sql[i + 1];
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_') {
                const size_t start = i + 1;
                size_t       end   = start;
                while (end < sql.size()) {
                    char c = sql[end];
                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
                        end++;
                    }
                    else {
                        break;
                    }
                }
                if (current_index < N) {
                    names[current_index++] = sql.substr(start, end - start);
                }
                i = end - 1; // 更新外层循环的索引
            }
        }
    }
    return names;
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