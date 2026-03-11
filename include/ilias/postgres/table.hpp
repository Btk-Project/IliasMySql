#pragma once

#include "ilias/postgres/global.hpp"

#include <vector>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

ILIAS_POSTGRES_NS_BEGIN
template <typename... Args>
class ColumnarTable {
public:
    ColumnarTable(Args... default_value) { mColumnDefaultValues = std::make_tuple(default_value...); }
    void set_column_names(std::array<std::string, sizeof...(Args)> names) { mColumnNames = std::move(names); }
    template <typename T>
    bool append_to_column(const std::string &name, const T &value) {
        return apply_by_name(name, [this, &value](auto &col) {
            using ColValueType = typename std::decay_t<decltype(col)>::value_type;
            // 编译期校验：传入的值类型 T 是否能隐式转换为该列的底层类型
            if constexpr (std::is_convertible_v<T, ColValueType>) {
                col.push_back(value);
                expand_to(0);
                return true;
            }
            else {
                return false;
            }
        });
    }
    template <size_t I, typename T>
    void append_to_column(const T &value) {
        std::get<I>(mColumns).push_back(value);
        expand_to(0);
    }
    template <typename T>
    void append_to_column(const std::string &name, const std::vector<T> &values) {
        return apply_by_name(name, [this, &values](auto &col) {
            using ColValueType = typename std::decay_t<decltype(col)>::value_type;
            // 编译期校验：传入的值类型 T 是否能隐式转换为该列的底层类型
            if constexpr (std::is_convertible_v<T, ColValueType>) {
                col.insert(col.end(), values.begin(), values.end());
                expand_to(0);
                return true;
            }
            else {
                return false;
            }
        });
    }

    template <size_t I, typename T>
    void append_to_column(const std::vector<T> &values) {
        std::get<I>(mColumns).insert(std::get<I>(mColumns).end(), values.begin(), values.end());
        expand_to(0);
    }

    template <typename... Ts>
    void push_back(const Ts &...values) {
        static_assert(sizeof...(values) == sizeof...(Args), "Number of arguments does not match number of columns");
        auto vt = std::tie(values...);
        [&vt, this]<size_t... Is>(std::index_sequence<Is...>) {
            ((std::get<Is>(mColumns).push_back(std::get<Is>(vt))), ...);
        }(std::make_index_sequence<sizeof...(Args)>());
    }

    template <typename T>
    void set_column(const std::string &name, const std::vector<T> &values) {
        return apply_by_name(name, [this, &values](auto &col) {
            using ColValueType = typename std::decay_t<decltype(col)>::value_type;
            // 编译期校验：传入的值类型 T 是否能隐式转换为该列的底层类型
            if constexpr (std::is_convertible_v<T, ColValueType>) {
                col.assign(values.begin(), values.end());
                expand_to(0);
                return true;
            }
            else {
                return false;
            }
        });
    }

    template <size_t I, typename T>
    void set_column(const std::vector<T> &values) {
        std::get<I>(mColumns).assign(values.begin(), values.end());
        expand_to(0);
    }

    template <typename T>
    void set_column(const std::string &name, const T &value) {
        return apply_by_name(name, [this, &value](auto &col) {
            using ColValueType = typename std::decay_t<decltype(col)>::value_type;
            // 编译期校验：传入的值类型 T 是否能隐式转换为该列的底层类型
            if constexpr (std::is_convertible_v<T, ColValueType>) {
                col.assign(col.size(), value);
                return true;
            }
            else {
                return false;
            }
        });
    }

    template <size_t I, typename T>
    void set_column(const T &value) {
        std::get<I>(mColumns).assign(std::get<I>(mColumns).size(), value);
    }

    template <size_t I>
    decltype(auto) get_column() const {
        return std::get<I>(mColumns);
    }
    template <size_t I>
    decltype(auto) get_column() {
        return std::get<I>(mColumns);
    }

    template <size_t I>
    decltype(auto) get_value(size_t i) const {
        return std::get<I>(mColumns)[i];
    }

    size_t column_size() {
        if constexpr (sizeof...(Args) > 0) {
            return std::get<0>(mColumns).size();
        }
        else {
            return 0;
        }
    }

    size_t row_size() { return sizeof...(Args); }

    void clear() {
        std::apply([&](auto &...cols) { (cols.clear(), ...); }, mColumns);
    }

    void resize(size_t size) {
        [this, size]<size_t... I>(std::index_sequence<I...>) {
            (std::get<I>(mColumns).resize(size, std::get<I>(mColumnDefaultValues)), ...);
        }(std::make_index_sequence<sizeof...(Args)>());
    }

    void reserve(size_t size) {
        [this, size]<size_t... I>(std::index_sequence<I...>) {
            (std::get<I>(mColumns).reserve(size), ...);
        }(std::make_index_sequence<sizeof...(Args)>());
    }

    decltype(auto) operator[](size_t i) {
        return std::apply([i](auto &...cols) { return std::tie(cols.at(i)...); }, mColumns);
    }

private:
    std::tuple<std::vector<Args>...>         mColumns;
    std::array<std::string, sizeof...(Args)> mColumnNames;
    std::tuple<Args...>                      mColumnDefaultValues;

    // 【核心黑魔法】：按列名查找并执行对应操作。
    // 使用 std::apply 配合 C++17 折叠表达式，在编译期生成遍历 tuple 的代码。
    template <typename Func>
    bool apply_by_name(const std::string &name, Func &&func) {
        bool found = false;
        std::apply(
            [&](auto &&...cols) {
                size_t index = 0;
                (
                    [&](auto &&col) {
                        if (index < mColumnNames.size() && mColumnNames[index] == name) {
                            found = func(col); // 找到列名后，将该列喂给 lambda 函数
                        }
                        index++;
                    }(cols),
                    ...);
            },
            mColumns);
        return found;
    }

    void expand_to(size_t size) {
        auto msize = [this]<size_t... Is>(std::index_sequence<Is...>) {
            std::max({std::get<Is>(mColumns).size()...});
        }(std::make_index_sequence<sizeof...(Args)>());
        msize = std::max(msize, size);
        [this, msize]<size_t... Is>(std::index_sequence<Is...>) {
            (std::get<Is>(mColumns).resize(msize, std::get<Is>(mColumnDefaultValues)), ...);
        }(std::make_index_sequence<sizeof...(Args)>());
    }
};
ILIAS_POSTGRES_NS_END