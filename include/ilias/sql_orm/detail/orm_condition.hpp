#pragma once

#include <vector>
#include <string>
#include <memory>
#include <string_view>
#include "ilias/sql/sqlstatement.hpp"
#include <nekoproto/serialization/types/struct_unwrap.hpp>
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_traits.hpp"

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
    void bind(int index, SqlStatement<void> &stmt) const override { SqlBinder<T>::bind(*stmt, index, mValue); }
    void bind(std::string_view name, SqlStatement<void> &stmt) const override {
        SqlBinder<T>::bind(*stmt, name, mValue);
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
    auto binds() const { return std::vector {mbinder}; }

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
        using RawType = std::decay_t<T>;
        SqlBinder<RawType>::bind(*stmt, index, mValueFunc());
    }
    void bind([[maybe_unused]] std::string_view name, SqlStatement<void> &stmt) const override {
        using RawType = std::decay_t<T>;
        SqlBinder<RawType>::bind(*stmt, name, mValueFunc());
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
        SqlBinder<T>::bind(*stmt, index, mValue);
    }
    void bind([[maybe_unused]] std::string_view name, SqlStatement<void> &stmt) const override {
        mValue = mValueFunc();
        SqlBinder<T>::bind(*stmt, name, mValue);
    }

private:
    mutable T          mValue;
    std::function<T()> mValueFunc;
};

class ILIAS_SQL_API SqlCondition {
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
    auto               binds() const { return mBinders; }

private:
    std::string                                      mSql;
    std::vector<std::shared_ptr<SqlStatementBinder>> mBinders;
};

struct SqlAssignment {
    auto binds() const { return std::vector {binders}; }

    std::string                                      sql;
    std::vector<std::shared_ptr<SqlStatementBinder>> binders;
};

class ILIAS_SQL_API SqlVariable {
public:
    explicit SqlVariable(std::string_view name);

    template <typename T>
        requires(SqlBindable<T> && (!HasSqlMethod<T>) && (!std::is_invocable_v<T>)) || std::is_null_pointer_v<T>
    SqlCondition compare(const std::string &op, T &&value) const {
        if (is_sql_null(value)) {
            if (op == "=")
                return SqlCondition(mName + " IS NULL", {});
            if (op == "!=")
                return SqlCondition(mName + " IS NOT NULL", {});
        }
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        using StorageT = StorageType_t<T>;
        ILIAS_TRACE("ilias-sql", "compare {} by {}", op, StorageSelector<T>::debug());
        binders.push_back(std::make_shared<ValueBinder<StorageT>>(std::forward<T>(value)));
        return SqlCondition(mName + " " + op + " ?", std::move(binders));
    }

    template <typename T>
        requires std::is_invocable_v<T>
    SqlCondition compare(const std::string &op, T &&value) const {
        using ResultT = std::invoke_result_t<T>;
        static_assert(SqlBindable<ResultT>, "Lambda return type must be bindable to SQL");
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<LambdaBinder<ResultT>>(std::forward<T>(value)));
        return SqlCondition(mName + " " + op + " ?", std::move(binders));
    }

    template <typename T>
        requires HasSqlMethod<T>
    SqlCondition compare(const std::string &op, T &&value) const {
        return SqlCondition(mName + " " + op + " " + value.sql(), {});
    }

    template <typename T>
    auto operator<(T &&v) const {
        return compare("<", std::forward<T>(v));
    }
    template <typename T>
    auto operator<=(T &&v) const {
        return compare("<=", std::forward<T>(v));
    }
    template <typename T>
    auto operator>(T &&v) const {
        return compare(">", std::forward<T>(v));
    }
    template <typename T>
    auto operator>=(T &&v) const {
        return compare(">=", std::forward<T>(v));
    }
    template <typename T>
    auto operator==(T &&v) const {
        return compare("=", std::forward<T>(v));
    }
    template <typename T>
    auto operator!=(T &&v) const {
        return compare("!=", std::forward<T>(v));
    }

    std::string sql() const { return mName; }

    template <typename T>
    SqlCondition like(T &&v) const {
        return compare("LIKE", std::forward<T>(v));
    }

    template <typename T>
        requires SqlBindable<T> && (!HasSqlMethod<T>) && (!std::is_invocable_v<T>)
    SqlAssignment operator=(T &&value) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<ValueBinder<StorageType_t<T>>>(std::forward<T>(value)));
        return SqlAssignment {.sql = mName + " = :" + mName, .binders = std::move(binders)};
    }

    template <typename T>
        requires std::is_invocable_v<T>
    SqlAssignment operator=(T &&u) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        using ResultT = std::invoke_result_t<T>;
        static_assert(SqlBindable<ResultT>, "Lambda return type must be bindable to SQL");
        binders.push_back(std::make_shared<LambdaBinder<ResultT>>(std::forward<T>(u)));
        return SqlAssignment {.sql = mName + " = :" + mName, .binders = std::move(binders)};
    }

    template <typename T>
        requires HasSqlMethod<T>
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
    static constexpr bool IsValidOperand =
        HasSqlMethod<U> || (SqlBindable<U> && IsCompatible<T, U>) || std::is_invocable_v<U>;

    template <typename U>
        requires IsValidOperand<U>
    auto operator<(U &&v) const {
        return SqlVariable::compare("<", std::forward<U>(v));
    }
    template <typename U>
        requires IsValidOperand<U>
    auto operator<=(U &&v) const {
        return SqlVariable::compare("<=", std::forward<U>(v));
    }
    template <typename U>
        requires IsValidOperand<U>
    auto operator>(U &&v) const {
        return SqlVariable::compare(">", std::forward<U>(v));
    }
    template <typename U>
        requires IsValidOperand<U>
    auto operator>=(U &&v) const {
        return SqlVariable::compare(">=", std::forward<U>(v));
    }
    template <typename U>
        requires IsValidOperand<U>
    auto operator==(U &&v) const {
        return SqlVariable::compare("=", std::forward<U>(v));
    }
    auto operator==(std::nullptr_t /*unused */) const { return SqlVariable::compare("=", std::nullptr_t {}); }
    template <typename U>
        requires IsValidOperand<U>
    auto operator!=(U &&v) const {
        return SqlVariable::compare("!=", std::forward<U>(v));
    }

    std::string sql() const { return mName; }

    template <typename U>
        requires(IsCompatible<T, U> && SqlBindable<U>) || std::is_same_v<std::decay_t<U>, std::string> ||
                std::is_convertible_v<std::decay_t<U>, std::string_view>
    SqlCondition like(U &&v) const {
        return SqlVariable::compare("LIKE", std::forward<U>(v));
    }

    template <typename U>
    SqlAssignment operator=(U &&value) const {
        if constexpr (ISqlValue<std::decay_t<U>> || ISqlValueView<std::decay_t<U>>) {
            static_assert(std::is_same_v<std::decay_t<U>, std::decay_t<typename OptionalLikeType<T>::type>>,
                          "raw value type mismatch");
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

    // 1. has_value() - 检查可选字段是否有值
    template <typename U = T>
        requires std::is_same_v<U, std::optional<typename U::value_type>>
    SqlCondition has_value() const {
        return SqlCondition(this->sql() + " IS NOT NULL", {});
    }

    // 2. is_null() - 检查字段是否为 NULL
    SqlCondition is_null() const { return SqlCondition(this->sql() + " IS NULL", {}); }

    // 3. is_not_null() - 检查字段是否不为 NULL
    SqlCondition is_not_null() const { return SqlCondition(this->sql() + " IS NOT NULL", {}); }

    // 4. in() - IN 操作符，支持多个值
    template <typename U>
        requires IsValidOperand<U>
    SqlCondition in(std::initializer_list<U> values) const {
        return in(std::vector<U>(values));
    }

    template <typename U>
        requires IsValidOperand<U>
    SqlCondition in(const std::vector<U> &values) const {
        if (values.empty()) {
            return SqlCondition("1 = 0", {}); // 永远为假
        }

        std::string                                      sql = this->sql() + " IN (";
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;

        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0)
                sql += ", ";
            sql += "?";
            binders.push_back(std::make_shared<ValueBinder<StorageType_t<U>>>(values[i]));
        }
        sql += ")";

        return SqlCondition(std::move(sql), std::move(binders));
    }

    // 5. not_in() - NOT IN 操作符
    template <typename U>
        requires IsValidOperand<U>
    SqlCondition not_in(std::initializer_list<U> values) const {
        return not_in(std::vector<U>(values));
    }

    template <typename U>
        requires IsValidOperand<U>
    SqlCondition not_in(const std::vector<U> &values) const {
        if (values.empty()) {
            return SqlCondition("1 = 1", {}); // 永远为真
        }

        std::string                                      sql = this->sql() + " NOT IN (";
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;

        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0)
                sql += ", ";
            sql += "?";
            binders.push_back(std::make_shared<ValueBinder<StorageType_t<U>>>(values[i]));
        }
        sql += ")";

        return SqlCondition(std::move(sql), std::move(binders));
    }

    // 6. between() - BETWEEN 操作符
    template <typename U>
        requires IsValidOperand<U>
    SqlCondition between(U &&min_val, U &&max_val) const {
        std::string                                      sql = this->sql() + " BETWEEN ? AND ?";
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<ValueBinder<StorageType_t<U>>>(std::forward<U>(min_val)));
        binders.push_back(std::make_shared<ValueBinder<StorageType_t<U>>>(std::forward<U>(max_val)));
        return SqlCondition(std::move(sql), std::move(binders));
    }

    // 7. not_between() - NOT BETWEEN 操作符
    template <typename U>
        requires IsValidOperand<U>
    SqlCondition not_between(U &&min_val, U &&max_val) const {
        std::string                                      sql = this->sql() + " NOT BETWEEN ? AND ?";
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<ValueBinder<StorageType_t<U>>>(std::forward<U>(min_val)));
        binders.push_back(std::make_shared<ValueBinder<StorageType_t<U>>>(std::forward<U>(max_val)));
        return SqlCondition(std::move(sql), std::move(binders));
    }

    // 8. starts_with() - 字符串开头匹配
    template <typename U>
        requires std::is_convertible_v<U, std::string>
    SqlCondition starts_with(U &&prefix) const {
        std::string pattern = std::string(prefix) + "%";
        return like(std::move(pattern));
    }

    // 9. ends_with() - 字符串结尾匹配
    template <typename U>
        requires std::is_convertible_v<U, std::string>
    SqlCondition ends_with(U &&suffix) const {
        std::string pattern = "%" + std::string(suffix);
        return like(std::move(pattern));
    }

    // 10. contains() - 字符串包含匹配
    template <typename U>
        requires std::is_convertible_v<U, std::string>
    SqlCondition contains(U &&substring) const {
        std::string pattern = "%" + std::string(substring) + "%";
        return like(std::move(pattern));
    }

    // 11. ilike() - 大小写不敏感的 LIKE (PostgreSQL)
    template <typename U>
        requires std::is_convertible_v<U, std::string>
    SqlCondition ilike(U &&pattern) const {
        std::string                                      sql = this->sql() + " ILIKE ?";
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<ValueBinder<std::string>>(std::string(pattern)));
        return SqlCondition(std::move(sql), std::move(binders));
    }

    // 12. regexp() - 正则表达式匹配
    template <typename U>
        requires std::is_convertible_v<U, std::string>
    SqlCondition regexp(U &&pattern) const {
        std::string                                      sql = this->sql() + " REGEXP ?";
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<ValueBinder<std::string>>(std::string(pattern)));
        return SqlCondition(std::move(sql), std::move(binders));
    }
};

template <typename T>
class AggregateColumn : public SqlVariable {
public:
    using Type = T;
    explicit AggregateColumn(std::string sql) : SqlVariable(std::move(sql)) {}

    // COUNT
    static AggregateColumn<int> count(const TypedColumn<T> &col) {
        return AggregateColumn<int>("COUNT(" + col.sql() + ")");
    }

    static AggregateColumn<int> count_distinct(const TypedColumn<T> &col) {
        return AggregateColumn<int>("COUNT(DISTINCT " + col.sql() + ")");
    }

    // SUM (仅数值类型)
    template <typename U = T>
        requires std::is_arithmetic_v<U>
    static AggregateColumn<T> sum(const TypedColumn<T> &col) {
        return AggregateColumn<T>("SUM(" + col.sql() + ")");
    }

    // AVG (仅数值类型)
    template <typename U = T>
        requires std::is_arithmetic_v<U>
    static AggregateColumn<double> avg(const TypedColumn<T> &col) {
        return AggregateColumn<double>("AVG(" + col.sql() + ")");
    }

    // MIN/MAX
    static AggregateColumn<T> min(const TypedColumn<T> &col) { return AggregateColumn<T>("MIN(" + col.sql() + ")"); }

    static AggregateColumn<T> max(const TypedColumn<T> &col) { return AggregateColumn<T>("MAX(" + col.sql() + ")"); }
};

// 14. 便利的聚合函数
template <typename T>
auto count(const TypedColumn<T> &col) {
    return AggregateColumn<int>::count(col);
}

template <typename T>
auto count_distinct(const TypedColumn<T> &col) {
    return AggregateColumn<int>::count_distinct(col);
}

template <typename T>
    requires std::is_arithmetic_v<T>
auto sum(const TypedColumn<T> &col) {
    return AggregateColumn<T>::sum(col);
}

template <typename T>
    requires std::is_arithmetic_v<T>
auto avg(const TypedColumn<T> &col) {
    return AggregateColumn<double>::avg(col);
}

template <typename T>
auto min(const TypedColumn<T> &col) {
    return AggregateColumn<T>::min(col);
}

template <typename T>
auto max(const TypedColumn<T> &col) {
    return AggregateColumn<T>::max(col);
}

// 15. 数学函数支持 (仅数值类型)
template <typename T>
    requires std::is_arithmetic_v<T>
class MathColumn : public SqlVariable {
public:
    using Type = T;
    explicit MathColumn(std::string sql) : SqlVariable(std::move(sql)) {}

    // ABS
    static MathColumn<T> abs(const TypedColumn<T> &col) { return MathColumn<T>("ABS(" + col.sql() + ")"); }

    // ROUND (仅浮点类型)
    template <typename U = T>
        requires std::is_floating_point_v<U>
    static MathColumn<T> round(const TypedColumn<T> &col, int precision = 0) {
        return MathColumn<T>("ROUND(" + col.sql() + ", " + std::to_string(precision) + ")");
    }

    // CEIL/FLOOR (仅浮点类型)
    template <typename U = T>
        requires std::is_floating_point_v<U>
    static MathColumn<T> ceil(const TypedColumn<T> &col) {
        return MathColumn<T>("CEIL(" + col.sql() + ")");
    }

    template <typename U = T>
        requires std::is_floating_point_v<U>
    static MathColumn<T> floor(const TypedColumn<T> &col) {
        return MathColumn<T>("FLOOR(" + col.sql() + ")");
    }
};

// 便利的数学函数
template <typename T>
    requires std::is_arithmetic_v<T>
auto abs(const TypedColumn<T> &col) {
    return MathColumn<T>::abs(col);
}

template <typename T>
    requires std::is_floating_point_v<T>
auto round(const TypedColumn<T> &col, int precision = 0) {
    return MathColumn<T>::round(col, precision);
}

template <typename T>
    requires std::is_floating_point_v<T>
auto ceil(const TypedColumn<T> &col) {
    return MathColumn<T>::ceil(col);
}

template <typename T>
    requires std::is_floating_point_v<T>
auto floor(const TypedColumn<T> &col) {
    return MathColumn<T>::floor(col);
}

} // namespace detail

// 用户字面量声明
ILIAS_SQL_API detail::SqlVariable operator""_sql(const char *str, size_t len);

ILIAS_SQL_NS_END