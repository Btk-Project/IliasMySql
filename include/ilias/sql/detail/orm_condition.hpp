#pragma once

#include <vector>
#include <string>
#include <memory>
#include <string_view>
#include "ilias/sql/sqlstatement.hpp"
#include <nekoproto/serialization/types/struct_unwrap.hpp>
#include "ilias/sql/detail/orm_types.hpp"

ILIAS_SQL_NS_BEGIN
namespace detail {

// 绑定器基类
class SqlStatementBinder {
public:
    virtual ~SqlStatementBinder()                                = default;
    virtual void bind(int index, SqlStatement<void> &stmt) const = 0;
};

// 具体的绑定器（模板必须在头文件）
template <typename T>
class ValueBinder : public SqlStatementBinder {
public:
    explicit ValueBinder(T value) : mValue(value) {}
    void bind(int index, SqlStatement<void> &stmt) const override { stmt->bind(index, mValue); }

private:
    T mValue;
};

class SqlCondition {
public:
    SqlCondition() = default;
    SqlCondition(std::string sql, std::vector<std::shared_ptr<SqlStatementBinder>> binders);

    // 逻辑运算符（实现在 cpp）
    SqlCondition operator&&(const SqlCondition &other) const;
    SqlCondition operator||(const SqlCondition &other) const;
    SqlCondition operator!() const;

    const std::string &sql() const;
    int                bindTo(SqlStatement<void> &stmt, int startIndex = 1) const;
    bool               empty() const;

private:
    std::string                                      mSql;
    std::vector<std::shared_ptr<SqlStatementBinder>> mBinders;
};

class SqlVariable {
public:
    explicit SqlVariable(std::string_view name);

    template <typename T>
        requires ISqlValue<T> || ISqlValueView<T>
    SqlCondition compare(const std::string &op, T &&value) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        using StorageT = StorageType_t<T>;
        binders.push_back(std::make_shared<ValueBinder<StorageT>>(std::forward<T>(value)));
        return SqlCondition(mName + " " + op + " ?", std::move(binders));
    }

    template <typename T>
        requires(!ISqlValue<T> && !ISqlValueView<T> && requires(T &&v) { v.sql(); })
    SqlCondition compare(const std::string &op, T &&value) const {
        return SqlCondition(mName + " " + op + " " + value.sql(), {});
    }

    template <typename T>
    auto operator<(T &&v) {
        return compare("<", std::forward<T>(v));
    }
    template <typename T>
    auto operator<=(T &&v) {
        return compare("<=", std::forward<T>(v));
    }
    template <typename T>
    auto operator>(T &&v) {
        return compare(">", std::forward<T>(v));
    }
    template <typename T>
    auto operator>=(T &&v) {
        return compare(">=", std::forward<T>(v));
    }
    template <typename T>
    auto operator==(T &&v) {
        return compare("=", std::forward<T>(v));
    }
    template <typename T>
    auto operator!=(T &&v) {
        return compare("!=", std::forward<T>(v));
    }

    std::string sql() const { return mName; }

    template <typename T>
    SqlCondition like(T &&v) {
        return compare("LIKE", std::forward<T>(v));
    }

protected:
    std::string mName;
};

template <typename T>
class TypedColumn : public SqlVariable {
public:
    using Type = T;
    TypedColumn(std::string name) : SqlVariable(std::move(name)) {}
};

} // namespace detail

// 用户字面量声明
detail::SqlVariable operator""_sql(const char *str, size_t len);

ILIAS_SQL_NS_END