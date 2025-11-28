#pragma once

#include "global.hpp"

#include <type_traits>
#include <nekoproto/serialization/reflection.hpp>

ILIAS_SQL_NS_BEGIN

constexpr size_t count_sql_params(std::string_view sql) {
    size_t count = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        if (sql[i] == ':') {
            // 检查边界
            if (i + 1 >= sql.size()) {
                // 结尾是 :，这通常是非法 SQL，但作为占位符算作 1 个
                count++;
                break;
            }

            // 处理 '::' (PostgreSQL cast)，跳过
            if (sql[i + 1] == ':') {
                i++; // 跳过下一个字符
                continue;
            }

            // 只有当 : 后面跟着字母或下划线时，才认为是参数
            char next = sql[i + 1];
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_') {
                count++;

                // 跳过参数名的剩余部分，避免参数名里有奇怪字符导致误判
                // (简化处理：一直跳到非标识符字符)
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

template <typename... Args>
struct SqlCheck {
    std::string_view sql;

    // consteval 构造函数：只有在编译期能执行成功才能通过编译
    consteval SqlCheck(const char *s) : sql(s) {
        size_t param_count = count_sql_params(sql);
        size_t args_count  = sizeof...(Args);

        if (param_count != args_count) {
            // 这里的报错会在编译输出中显示
            // 遗憾的是 C++20 throw 在 consteval 中还不完全被所有编译器完美支持显示文本
            // 但会导致编译失败。
            // 也可以使用 if consteval 分支做 compile_error
            throw "Error: SQL placeholder count does not match argument count!";
        }
    }
};

template <typename U>
struct SqlStructCheck {
    std::string_view sql;

    consteval SqlStructCheck(const char *s) : sql(s) {
        size_t param_count = count_sql_params(sql);

        // 【注意】这里需要获取 U 的成员数量。
        // 如果你有反射库 (如 PFR)，可以用 pfr::structure_to_tuple(U{}).size()
        // 这里为了演示，我假设 U 有一个静态成员 constexpr size_t field_count
        // 如果没有，这行代码会编译报错，提示你需要适配你的反射机制。
        // 在你的代码中，可能是 NEKO_NAMESPACE::detail::meta_info<U>::size 等

        // 伪代码适配你的环境:
        constexpr size_t member_count = get_member_count<U>();

        if (param_count != member_count) {
            throw "Error: SQL placeholder count does not match Struct member count!";
        }
    }

    // 辅助 helper，你需要根据你的库替换这个实现
    template <typename T>
    static consteval size_t get_member_count() {
        if constexpr (NEKO_NAMESPACE::detail::is_std_tuple_v<T>) {
            return std::tuple_size_v<T>;
        }
        else {
            return NEKO_NAMESPACE::Reflect<T>::size();
        }
    }
};

ILIAS_SQL_NS_END
