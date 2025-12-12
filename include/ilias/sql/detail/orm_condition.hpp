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
    virtual ~SqlStatementBinder()                                            = default;
    virtual void bind(int index, SqlStatement<void> &stmt) const             = 0;
    virtual void bind(std::string_view name, SqlStatement<void> &stmt) const = 0;
};

// 具体的绑定器（模板必须在头文件）
template <typename T>
class ValueBinder : public SqlStatementBinder {
public:
    explicit ValueBinder(T value) : mValue(value) {}
    void bind(int index, SqlStatement<void> &stmt) const override { stmt->bind(index, to_sql_pointer(mValue)); }
    void bind(std::string_view name, SqlStatement<void> &stmt) const override {
        stmt->bind(name, to_sql_pointer(mValue));
    }

private:
    T mValue;
};

// 具体的绑定器（模板必须在头文件）
template <typename T, typename... Ts>
    requires std::is_constructible_v<T, Ts...>
class ObjBinder : public SqlStatementBinder {
public:
    explicit ObjBinder(Ts... arg) : mValue(arg...) {}
    void bind([[maybe_unused]] int index, SqlStatement<void> &stmt) const override {
        stmt.bind(std::apply([](auto &&...args) { return T(std::forward<decltype(args)>(args)...); }, mValue));
    }
    void bind([[maybe_unused]] std::string_view name, SqlStatement<void> &stmt) const override {
        stmt.bind(std::apply([](auto &&...args) { return T(std::forward<decltype(args)>(args)...); }, mValue));
    }

private:
    std::tuple<Ts...> mValue;
};

class NamedBinder : public SqlStatementBinder {
public:
    explicit NamedBinder(const std::string &name, std::shared_ptr<SqlStatementBinder> binder)
        : mbinder(binder), mName(name) {}
    void bind([[maybe_unused]] int index, SqlStatement<void> &stmt) const override { mbinder->bind(mName, stmt); }
    void bind([[maybe_unused]] std::string_view name, SqlStatement<void> &stmt) const override {
        mbinder->bind(mName, stmt);
    }

private:
    std::shared_ptr<SqlStatementBinder> mbinder;
    std::string                         mName;
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

struct SqlAssignment {
    std::string                                      sql;
    std::vector<std::shared_ptr<SqlStatementBinder>> binders;
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

    template <typename T>
        requires ISqlValue<std::decay_t<T>> || ISqlValueView<std::decay_t<T>>
    SqlAssignment operator=(T &&value) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<ValueBinder<std::decay_t<T>>>(std::forward<T>(value)));
        return SqlAssignment {.sql = mName + " = ?", .binders = std::move(binders)};
    }

    // template <typename T>
    //     requires (!ISqlValue<std::decay_t<T>> && !ISqlValueView<std::decay_t<T>> && requires(T t) { t.sql(); })
    // SqlAssignment operator=(const T& value) const {
    //     std::vector<std::shared_ptr<SqlStatementBinder>> binders;
    //     if constexpr (requires { value.getBinders(); }) {
    //          auto otherBinders = value.getBinders();
    //          binders.insert(binders.end(), otherBinders.begin(), otherBinders.end());
    //     }
    //     return SqlAssignment {.sql = mName + " = " + value.sql(), .binders = std::move(binders)};
    // }

protected:
    std::string mName;
};

template <typename T>
class TypedColumn : public SqlVariable {
public:
    using Type = T;
    explicit TypedColumn(std::string name) : SqlVariable(std::move(name)) {}
    using SqlVariable::operator=;
};

} // namespace detail

// 用户字面量声明
detail::SqlVariable operator""_sql(const char *str, size_t len);

ILIAS_SQL_NS_END