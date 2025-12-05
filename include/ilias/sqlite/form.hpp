#pragma once

#include "ilias/sqlite/global.hpp"
#include "ilias/sql/global/global.hpp"
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include <nekoproto/serialization/types/struct_unwrap.hpp>

ILIAS_SQLITE_NS_BEGIN
using namespace ILIAS_SQL_NAMESPACE;

namespace detail {

// 工具：字符串拼接
inline std::string join(const std::vector<std::string> &vec, const std::string &sep, const std::string &prefix = "",
                        const std::string &suffix = "") {
    std::string res;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0)
            res += sep;
        res += prefix + vec[i] + suffix;
    }
    return res;
}

// ==========================================
// EDSL: SQL 条件构建系统
// ==========================================

class SqlStatementBinder {
public:
    virtual ~SqlStatementBinder()                                = default;
    virtual void bind(int index, SqlStatement<void> &stmt) const = 0;
};

// 具体的绑定器，持有值的副本，确保生命周期安全
template <typename T>
class ValueBinder : public SqlStatementBinder {
public:
    explicit ValueBinder(T value) : mValue(std::move(value)) {}
    void bind(int index, SqlStatement<void> &stmt) const override { stmt->bind(index, mValue); }

private:
    T mValue;
};

class SqlCondition {
public:
    SqlCondition() = default;
    // 构造函数：SQL 片段 + 绑定器列表
    SqlCondition(std::string sql, std::vector<std::shared_ptr<SqlStatementBinder>> binders)
        : mSql(std::move(sql)), mBinders(std::move(binders)) {}

    // 逻辑与
    SqlCondition operator&&(const SqlCondition &other) const {
        if (mSql.empty())
            return other;
        if (other.mSql.empty())
            return *this;

        std::vector<std::shared_ptr<SqlStatementBinder>> newBinders = mBinders;
        newBinders.insert(newBinders.end(), other.mBinders.begin(), other.mBinders.end());
        return SqlCondition("(" + mSql + " AND " + other.mSql + ")", std::move(newBinders));
    }

    // 逻辑或
    SqlCondition operator||(const SqlCondition &other) const {
        if (mSql.empty())
            return other;
        if (other.mSql.empty())
            return *this;

        std::vector<std::shared_ptr<SqlStatementBinder>> newBinders = mBinders;
        newBinders.insert(newBinders.end(), other.mBinders.begin(), other.mBinders.end());
        return SqlCondition("(" + mSql + " OR " + other.mSql + ")", std::move(newBinders));
    }

    // 逻辑非
    SqlCondition operator!() const {
        if (mSql.empty())
            return *this;
        return SqlCondition("NOT (" + mSql + ")", mBinders);
    }

    const std::string &sql() const { return mSql; }

    // 执行绑定
    void bindTo(SqlStatement<void> &stmt) const {
        int index = 1;
        for (const auto &binder : mBinders) {
            binder->bind(index++, stmt);
        }
    }

    bool empty() const { return mSql.empty(); }

private:
    std::string                                      mSql;
    std::vector<std::shared_ptr<SqlStatementBinder>> mBinders;
};

class SqlVariable {
public:
    explicit SqlVariable(std::string_view name) : mName(name) {}

    // 生成比较条件的通用模板
    template <typename T>
    SqlCondition compare(const std::string &op, T &&value) const {
        std::vector<std::shared_ptr<SqlStatementBinder>> binders;
        // 关键点：按值捕获（Decay copy），防止引用悬挂
        binders.push_back(std::make_shared<ValueBinder<std::decay_t<T>>>(std::forward<T>(value)));
        return SqlCondition(mName + " " + op + " ?", std::move(binders));
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

    // 支持 like
    template <typename T>
    SqlCondition like(T &&v) {
        return compare("LIKE", std::forward<T>(v));
    }

private:
    std::string mName;
};

} // namespace detail

// 用户字面量
inline detail::SqlVariable operator""_sql(const char *str, size_t len) {
    return detail::SqlVariable(std::string_view(str, len));
}

// 标签定义
struct SqlTags {
    bool unique         = false;
    bool not_null       = false;
    bool primary_key    = false;
    bool auto_increment = false;
    bool default_value  = false;
};

// 前置声明
template <typename T>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form;

// ==========================================
// Select Builder (完全重构)
// ==========================================
// 不再使用模板参数 N 计数，改用 Condition 对象自包含数据
class SelectBuilder {
public:
    SelectBuilder(SqlDatabase &db, std::string tableName) : mDb(db), mTableName(std::move(tableName)) {}

    // 选择特定列
    SelectBuilder &select(const std::string &columns) {
        if (mSelectColumns == "*")
            mSelectColumns = columns;
        else
            mSelectColumns += ", " + columns;
        return *this;
    }

    SelectBuilder &count() {
        mSelectColumns = "COUNT(*)";
        return *this;
    }

    // WHERE 子句
    SelectBuilder &where(const detail::SqlCondition &cond) {
        // 如果多次调用 where，默认用 AND 连接
        mWhereCondition = mWhereCondition && cond;
        return *this;
    }

    // 排序
    SelectBuilder &orderBy(const std::string &column, bool desc = false) {
        mOrderBy = " ORDER BY " + column + (desc ? " DESC" : " ASC");
        return *this;
    }

    // 分页
    SelectBuilder &limit(int limit) {
        mLimit = " LIMIT " + std::to_string(limit);
        return *this;
    }

    SelectBuilder &offset(int offset) {
        mOffset = " OFFSET " + std::to_string(offset);
        return *this;
    }

    // 执行查询
    IoTask<SqlResult<void>> execute() {
        std::string sql = "SELECT " + mSelectColumns + " FROM " + mTableName;

        if (!mWhereCondition.empty()) {
            sql += " WHERE " + mWhereCondition.sql();
        }

        sql += mOrderBy + mLimit + mOffset;

        auto ret = co_await mDb.prepare(sql);
        if (!ret) {
            co_return Unexpected(ret.error());
        }

        // 绑定值（值存储在 Condition 中）
        mWhereCondition.bindTo(ret.value());

        co_return co_await ret->query();
    }

    // 如果需要映射回对象列表的辅助函数
    template <typename T>
    IoTask<std::vector<T>> toVector() {
        // 需要 SqlResult 支持迭代器转换，此处略
        co_return std::vector<T> {};
    }

private:
    SqlDatabase         &mDb;
    std::string          mTableName;
    std::string          mSelectColumns = "*";
    detail::SqlCondition mWhereCondition;
    std::string          mOrderBy;
    std::string          mLimit;
    std::string          mOffset;
};

// ==========================================
// Form Class
// ==========================================
template <typename T>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form final {
public:
    static auto create(SqlDatabase &db, const std::string &tableName) -> IoTask<Form> {
        T                        obj;
        std::vector<std::string> colDefs;
        std::vector<std::string> colNames;
        std::string              pkName;

        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const SqlTags &tags) {
            std::string typeStr;
            using FieldType = std::decay_t<decltype(field)>;

            if constexpr (std::is_integral_v<FieldType>) {
                // 处理 int64 等
                typeStr = "INTEGER";
            }
            else if constexpr (std::is_floating_point_v<FieldType>) {
                typeStr = "REAL";
            }
            else if constexpr (std::is_same_v<FieldType, std::string> || std::is_same_v<FieldType, SqlDate>) {
                typeStr = "TEXT";
            }
            else if constexpr (std::is_same_v<FieldType, SqlBlob>) {
                typeStr = "BLOB";
            }
            else {
                // 默认回退
                typeStr = "TEXT";
            }

            std::string colDef = std::string(name) + " " + typeStr;

            if (tags.primary_key) {
                colDef += " PRIMARY KEY";
                pkName = name;
            }
            if (tags.auto_increment)
                colDef += " AUTOINCREMENT";
            if (tags.unique)
                colDef += " UNIQUE";
            if (tags.not_null)
                colDef += " NOT NULL";
            // default value logic needs careful handling of value to string conversion

            colDefs.push_back(colDef);
            colNames.emplace_back(name);
        });

        std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (" + detail::join(colDefs, ", ") + ")";
        auto        ret = co_await db.execute(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        Form form(db, tableName);
        form.mTableHeaderNames = std::move(colNames);
        form.mPrimaryKey       = std::move(pkName);
        co_return form;
    }

    auto insert(T value) -> IoTask<size_t> {

        auto sql = "INSERT INTO " + mTableName + " (" + detail::join(mTableHeaderNames, ", ") + ") VALUES (" +
                   detail::join(mTableHeaderNames, ", ", ":") + ")";

        auto ret = co_await mDb.prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        // 绑定整个结构体
        if (auto bind_ret = ret->bind(std::move(value)); !bind_ret) {
            co_return Unexpected(bind_ret.error());
        }
        co_return co_await ret->execute();
    }

    template <typename... Args>
        requires(std::is_constructible_v<T, Args...> && sizeof...(Args) > 0)
    auto insert(Args &&...args) -> IoTask<size_t> {
        co_return co_await insert(T(std::forward<Args>(args)...));
    }

    // --------------------------------------------------------
    // Update: 自动寻找 Primary Key 进行更新
    // --------------------------------------------------------
    auto update(T value) -> IoTask<size_t> {
        if (mPrimaryKey.empty()) {
            // 如果没有主键，无法自动 update，或者需要抛错
            ILIAS_ERROR("ilias-sqlite", "Cannot update table {} without primary key", mTableName);
            co_return Unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        std::vector<std::string> setClauses;
        for (const auto &name : mTableHeaderNames) {
            if (name != mPrimaryKey) {
                setClauses.push_back(name + " = :" + name);
            }
        }

        std::string sql = "UPDATE " + mTableName + " SET " + detail::join(setClauses, ", ") + " WHERE " + mPrimaryKey +
                          " = :" + mPrimaryKey;

        auto ret = co_await mDb.prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        bool pkFound = false;
        NEKO_NAMESPACE::Reflect<T>::forEach(value, [&](const auto &field, std::string_view name, const SqlTags &) {
            if (name != mPrimaryKey) {
                (*ret)->bind(name, field);
            }
        });
        NEKO_NAMESPACE::Reflect<T>::forEach(value, [&](const auto &field, std::string_view name, const SqlTags &) {
            if (name == mPrimaryKey) {
                (*ret)->bind(name, field);
                pkFound = true;
            }
        });

        if (!pkFound)
            co_return Unexpected(std::make_error_code(std::errc::invalid_argument));

        co_return co_await ret->execute();
    }

    // --------------------------------------------------------
    // Remove
    // --------------------------------------------------------
    auto remove(T value) -> IoTask<size_t> {
        if (mPrimaryKey.empty()) {
            co_return Unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        std::string sql = "DELETE FROM " + mTableName + " WHERE " + mPrimaryKey + " = ?";
        auto        ret = co_await mDb.prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        NEKO_NAMESPACE::Reflect<T>::forEach(value, [&](const auto &field, std::string_view name, const SqlTags &) {
            if (name == mPrimaryKey) {
                (*ret)->bind(1, field);
            }
        });

        co_return co_await ret->execute();
    }

    // --------------------------------------------------------
    // Select 入口
    // --------------------------------------------------------
    auto select(const std::string &columns) -> SelectBuilder { return SelectBuilder(mDb, mTableName).select(columns); }

    auto name() -> std::string & { return mTableName; }

private:
    Form(SqlDatabase &db, const std::string &tableName) : mDb(db), mTableName(tableName) {}

    SqlDatabase             &mDb;
    std::string              mTableName;
    std::vector<std::string> mTableHeaderNames;
    std::string              mPrimaryKey; // 缓存主键名
};

ILIAS_SQLITE_NS_END