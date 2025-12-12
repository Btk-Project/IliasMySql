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
template <typename T>
class LambdaBinder;

template <typename T>
    requires std::is_lvalue_reference_v<T>
class LambdaBinder<T> : public SqlStatementBinder {
public:
    explicit LambdaBinder(std::function<T()> valueFunc) : mValueFunc(valueFunc) {}
    void bind([[maybe_unused]] int index, SqlStatement<void> &stmt) const override {
        stmt->bind(index, to_sql_pointer(mValueFunc()));
    }
    void bind([[maybe_unused]] std::string_view name, SqlStatement<void> &stmt) const override {
        stmt->bind(name, to_sql_pointer(mValueFunc()));
    }

private:
    std::function<T()> mValueFunc;
};

template <typename T>
    requires(!std::is_lvalue_reference_v<T>)
class LambdaBinder<T> : public SqlStatementBinder {
public:
    explicit LambdaBinder(std::function<T()> valueFunc) : mValueFunc(valueFunc) {}
    void bind([[maybe_unused]] int index, SqlStatement<void> &stmt) const override {
        mValue = mValueFunc();
        stmt->bind(index, to_sql_pointer(mValue));
    }
    void bind([[maybe_unused]] std::string_view name, SqlStatement<void> &stmt) const override {
        mValue = mValueFunc();
        stmt->bind(name, to_sql_pointer(mValue));
    }

private:
    mutable T          mValue;
    std::function<T()> mValueFunc;
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
        requires ISqlValue<T> || ISqlValueView<T>
    SqlCondition compare(const std::string &op, std::function<T()> &&value) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<LambdaBinder<T>>(std::forward<T>(value)));
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
        binders.push_back(std::make_shared<ValueBinder<StorageType_t<T>>>(std::forward<T>(value)));
        return SqlAssignment {.sql = mName + " = ?", .binders = std::move(binders)};
    }

    template <typename T>
        requires(std::is_invocable_v<T> &&
                 requires(T u) {
                     { u() } -> ISqlValueView<>;
                 })
    SqlAssignment operator=(T &&u) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        using ResultT = std::invoke_result_t<T>;
        binders.push_back(std::make_shared<LambdaBinder<ResultT>>(std::forward<T>(u)));
        return SqlAssignment {.sql = mName + " = ?", .binders = std::move(binders)};
    }

    template <typename T>
        requires(!ISqlValue<std::decay_t<T>> && !ISqlValueView<std::decay_t<T>> && requires(T t) { t.sql(); })
    SqlAssignment operator=(const T &value) const {
        return SqlAssignment {.sql = mName + " = " + value.sql(), .binders = {}};
    }

protected:
    std::string mName;
};

template <typename T>
class TypedColumn : public SqlVariable {
public:
    using Type = T;
    explicit TypedColumn(std::string name) : SqlVariable(std::move(name)) {}
    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    auto operator<(U &&v) {
        return SqlVariable::compare("<", std::forward<U>(v));
    }
    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    auto operator<=(U &&v) {
        return SqlVariable::compare("<=", std::forward<U>(v));
    }
    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    auto operator>(U &&v) {
        return SqlVariable::compare(">", std::forward<U>(v));
    }
    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    auto operator>=(U &&v) {
        return SqlVariable::compare(">=", std::forward<U>(v));
    }
    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    auto operator==(U &&v) {
        return SqlVariable::compare("=", std::forward<U>(v));
    }
    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    auto operator!=(U &&v) {
        return SqlVariable::compare("!=", std::forward<U>(v));
    }

    std::string sql() const { return mName; }

    template <typename U>
        requires std::totally_ordered_with<U, T> || std::is_same_v<std::decay_t<U>, TypedColumn<T>> ||
                 (std::is_function_v<U> &&
                  requires(U u) {
                      { u() } -> std::totally_ordered_with<T>;
                  })
    SqlCondition like(U &&v) {
        return SqlVariable::compare("LIKE", std::forward<U>(v));
    }

    template <typename U>
    SqlAssignment operator=(U &&value) const {
        if constexpr (ISqlValue<std::decay_t<U>> || ISqlValueView<std::decay_t<U>>) {
            static_assert(std::is_same_v<std::decay_t<U>, std::decay_t<T>>, "raw value type mismatch");
        }
        else if constexpr (std::is_invocable_v<U>) {
            using ResultT = std::invoke_result_t<U>;
            static_assert(std::is_same_v<std::decay_t<ResultT>, std::decay_t<T>>, "function result type mismatch");
        }
        else if constexpr (requires(U t) {
                               { t.sql() } -> std::same_as<std::string>;
                           }) {
            // TODO: check string can be converted to T ?
        }
        else {
            static_assert(std::is_same_v<std::decay_t<U>, std::decay_t<T>>, "unknown bind type");
        }
        return SqlVariable::operator=(std::forward<U>(value));
    }
};

} // namespace detail

// 用户字面量声明
detail::SqlVariable operator""_sql(const char *str, size_t len);

ILIAS_SQL_NS_END