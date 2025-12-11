#pragma once

#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include "ilias/sql/dialect.hpp"
#include "ilias/sql/detail/console_table.hpp"

#include "ilias/sql/detail/orm_types.hpp"
#include "ilias/sql/detail/orm_condition.hpp"
#include "ilias/sql/detail/orm_builder.hpp"

ILIAS_SQL_NS_BEGIN

template <typename T, typename BackendTag>
class TableAlias;

template <typename T, typename BackendTag = SqliteTag> // 默认可以是 SQLite
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form final {
public:
    using type    = T;
    using Dialect = Dialect<BackendTag>;

    static auto create(SqlDatabase &db, const std::string &tableName) -> IoTask<Form> {
        T                        obj;
        std::vector<std::string> colDefs;
        std::vector<std::string> colNames;
        std::string              pkName;

        std::vector<SqlTags>          tableHeaderTags;
        std::map<std::ptrdiff_t, int> tableHeaderIndex;

        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const SqlTags &tags) {
            std::string typeStr = std::string(Dialect::template type_name<decltype(field)>());
            std::string colDef  = std::string(name) + " " + typeStr;
            tableHeaderTags.emplace_back(tags);
            tableHeaderIndex[(char *)&field - (char *)&obj] = colDefs.size();

            if (tags.primary_key) {
                colDef += " " + std::string(Dialect::primary_key());
                pkName = name;
            }
            if (tags.auto_increment)
                colDef += " " + std::string(Dialect::auto_increment());
            if (tags.unique)
                colDef += " UNIQUE";
            if (tags.not_null)
                colDef += " NOT NULL";

            colDefs.push_back(colDef);
            colNames.emplace_back(name);
        });

        std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (" + detail::join_strs(colDefs, ", ") + ")";
        auto        ret = co_await db.execute(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        Form form(db, tableName);
        form.mTableHeaderNames = std::move(colNames);
        form.mPrimaryKey       = std::move(pkName);
        form.mTableHeaderTags  = std::move(tableHeaderTags);
        form.mTableHeaderIndex = std::move(tableHeaderIndex);
        ILIAS_TRACE("ilias-sql", "Created table {}, columns: {}, primary key: {}", tableName,
                    detail::join_strs(form.mTableHeaderNames, ", "), form.mPrimaryKey);
        co_return form;
    }

    template <typename U>
        requires(std::is_same_v<std::decay_t<U>, T>)
    auto insert(U &&value) -> IoTask<size_t> {
        auto sql = "INSERT INTO " + mTableName + " (" + detail::join_strs(mTableHeaderNames, ", ") + ") VALUES (" +
                   detail::join_strs(mTableHeaderNames, ", ", ":") + ")";

        auto ret = co_await mDb.prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        if (auto bind_ret = ret->bind(std::forward<T>(value)); !bind_ret) {
            co_return Unexpected(bind_ret.error());
        }
        co_return co_await ret->execute();
    }

    template <typename... Args>
        requires(std::is_constructible_v<T, Args...> && sizeof...(Args) > 0)
    auto insert(Args &&...args) -> IoTask<size_t> {
        co_return co_await insert(T(std::forward<Args>(args)...));
    }

    auto update(T value) -> IoTask<size_t> {
        if (mPrimaryKey.empty()) {
            ILIAS_ERROR("ilias-sql", "Cannot update table {} without primary key", mTableName);
            co_return Unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        std::vector<std::string> setClauses;
        for (const auto &name : mTableHeaderNames) {
            if (name != mPrimaryKey) {
                setClauses.push_back(name + " = :" + name);
            }
        }

        std::string sql = "UPDATE " + mTableName + " SET " + detail::join_strs(setClauses, ", ") + " WHERE " +
                          mPrimaryKey + " = :" + mPrimaryKey;

        auto ret = co_await mDb.prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        bool pkFound = false;
        NEKO_NAMESPACE::Reflect<T>::forEach(value, [&](const auto &field, std::string_view name, const SqlTags &) {
            if (name != mPrimaryKey)
                (*ret)->bind(name, field);
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

    auto remove(T value) -> IoTask<size_t> {
        if (mPrimaryKey.empty()) {
            co_return Unexpected(std::make_error_code(std::errc::invalid_argument));
        }

        std::string sql = "DELETE FROM " + mTableName + " WHERE " + mPrimaryKey + " = ?";
        auto        ret = co_await mDb.prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        NEKO_NAMESPACE::Reflect<T>::forEach(value, [&](const auto &field, std::string_view name, const SqlTags &) {
            if (name == mPrimaryKey)
                (*ret)->bind(1, field);
        });

        co_return co_await ret->execute();
    }

    template <typename... Ts>
    auto select(detail::TypedColumn<Ts>... args) const -> detail::ProjectedSelectBuilder<Ts...> {
        return detail::ProjectedSelectBuilder<Ts...>(mDb, mTableName, args...);
    }

    auto select(const std::string &columns = "") const -> detail::SelectBuilder {
        if (columns.empty())
            return detail::SelectBuilder(mDb, mTableName);
        return detail::SelectBuilder(mDb, mTableName).select(columns);
    }

    auto count() const -> detail::SelectBuilder { return detail::SelectBuilder(mDb, mTableName).count(); }

    template <typename TableT>
    auto join(TableT &other, const std::string &type = "INNER") {
        return detail::JoinedSelectBuilder<Form, TableT>(*this, other, type);
    }
    template <typename TableT>
    auto leftJoin(TableT &other) {
        return join(other, "LEFT");
    }
    template <typename TableT>
    auto rightJoin(TableT &other) {
        return join(other, "RIGHT");
    }

    const std::vector<std::string>      &getColumnNames() const { return mTableHeaderNames; }
    const std::vector<SqlTags>          &getColumnTags() const { return mTableHeaderTags; }
    const std::map<std::ptrdiff_t, int> &getColumnIndex() const { return mTableHeaderIndex; }
    const std::string                   &getPrimaryKey() const { return mPrimaryKey; }
    const std::string                   &getTableName() const { return mTableName; }
    const std::string                   &tableRef() const { return mTableName; }
    const std::string                   &getAlias() const { return mTableName; }
    SqlDatabase                         &db() { return mDb; }
    const SqlDatabase                   &db() const { return mDb; }

    template <typename M>
    auto getColumnIndex(M T::*memberPtr) const -> int {
        T             *tmp  = nullptr;
        std::ptrdiff_t ptr  = (char *)&(tmp->*memberPtr) - (char *)tmp;
        auto           item = mTableHeaderIndex.find(ptr);
        if (item == mTableHeaderIndex.end())
            return -1;
        return item->second;
    }

    template <typename M>
    auto getColumnName(M T::*memberPtr) const -> IoResult<std::string> {
        auto index = getColumnIndex(memberPtr);
        if (index == -1)
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
        return mTableHeaderNames.at(index);
    }

    template <typename M>
    auto getColumnTag(M T::*memberPtr) const -> IoResult<SqlTags> {
        auto index = getColumnIndex(memberPtr);
        if (index == -1)
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
        return mTableHeaderTags.at(index);
    }

    template <typename M>
    auto sql(M T::*memberPtr) const {
        return detail::TypedColumn<std::decay_t<M>>(getColumnName(memberPtr).value());
    }
    template <typename M>
    auto col(M T::*memberPtr) const {
        return detail::TypedColumn<std::decay_t<M>>(mTableName + "." + getColumnName(memberPtr).value());
    }

    auto as(const std::string &alias);

    auto print() -> IoTask<void> {
        auto ret = co_await select().query();
        if (!ret) {
            ILIAS_ERROR("ilias-sql", "Print failed: {}", ret.error().message());
            co_return {};
        }
        detail::ConsoleTable table(mTableHeaderNames);
        SqlResult<T>         res = std::move(ret.value());
        ilias_for_await([[maybe_unused]] auto obj, res.range()) {
            std::vector<std::string> rowStrings;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, auto...) {
                using FieldType = std::decay_t<decltype(field)>;
                if constexpr (std::is_same_v<FieldType, std::vector<char>> || std::is_same_v<FieldType, SqlBlob>) {
                    rowStrings.push_back("(BLOB " + std::to_string(field.size()) + " bytes)");
                }
                else {
                    rowStrings.push_back(detail::to_string_view(field));
                }
            });
            table.addRow(rowStrings);
        }
        table.print();
        co_return {};
    }

private:
    Form(SqlDatabase &db, const std::string &tableName) : mDb(db), mTableName(tableName) {}

    SqlDatabase                  &mDb;
    std::string                   mTableName;
    std::vector<std::string>      mTableHeaderNames;
    std::vector<SqlTags>          mTableHeaderTags;
    std::map<std::ptrdiff_t, int> mTableHeaderIndex;
    std::string                   mPrimaryKey;
};

template <typename T, typename BackendTag>
class TableAlias {
public:
    using type    = T;
    using Dialect = Dialect<BackendTag>;

    TableAlias(const std::string &alias, Form<T, BackendTag> &form) : mAlias(alias), mForm(form) {}

    template <typename M>
    auto col(M T::*memberPtr) const {
        std::string rawColName = mForm.getColumnName(memberPtr).value();
        return detail::TypedColumn<std::decay_t<M>>(mAlias + "." + rawColName);
    }
    std::string                          tableRef() const { return mForm.getTableName() + " AS " + mAlias; }
    const std::string                   &getAlias() const { return mAlias; }
    const std::string                   &getTableName() const { return mForm.getTableName(); }
    const std::vector<std::string>      &getColumnNames() const { return mForm.getColumnNames(); }
    const std::vector<SqlTags>          &getColumnTags() const { return mForm.getColumnTags(); }
    const std::map<std::ptrdiff_t, int> &getColumnIndex() const { return mForm.getColumnIndex(); }
    const std::string                   &getPrimaryKey() const { return mForm.getPrimaryKey(); }

    template <typename M>
    auto sql(M T::*memberPtr) const {
        return detail::TypedColumn<std::decay_t<M>>(mAlias + "." + mForm.getColumnName(memberPtr).value());
    }
    template <typename M>
    auto getColumnIndex(M T::*memberPtr) const {
        return mForm.getColumnIndex(memberPtr);
    }
    template <typename M>
    auto getColumnTag(M T::*memberPtr) const {
        return mForm.getColumnTag(memberPtr);
    }
    template <typename M>
    auto getColumnName(M T::*memberPtr) const {
        return mForm.getColumnName(memberPtr);
    }

    auto as(const std::string &alias) { return TableAlias(alias, mForm); }
    auto print() -> IoTask<void> { return mForm.print(); }

    template <typename U>
        requires(std::is_same_v<std::decay_t<U>, T>)
    auto insert(U &&value) -> IoTask<size_t> {
        return mForm.insert(std::forward<U>(value));
    }

    template <typename... Args>
        requires(std::is_constructible_v<T, Args...> && sizeof...(Args) > 0)
    auto insert(Args &&...args) -> IoTask<size_t> {
        return mForm.insert(std::forward<Args>(args)...);
    }

    auto update(T value) -> IoTask<size_t> { return mForm.update(std::move(value)); }
    auto remove(T value) -> IoTask<size_t> { return mForm.remove(std::move(value)); }

    template <typename TableT>
    auto join(TableT &other, const std::string &type = "INNER") {
        return detail::JoinedSelectBuilder<TableAlias, TableT>(*this, other, type);
    }
    template <typename TableT>
    auto leftJoin(TableT &other) {
        return join(other, "LEFT");
    }
    template <typename TableT>
    auto rightJoin(TableT &other) {
        return join(other, "RIGHT");
    }

    SqlDatabase       &db() { return mForm.db(); }
    const SqlDatabase &db() const { return mForm.db(); }

private:
    std::string          mAlias;
    Form<T, BackendTag> &mForm;
};

template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
auto Form<T, BackendTag>::as(const std::string &alias) {
    TableAlias<T, BackendTag> wrapper(alias, *this);
    return wrapper;
}

ILIAS_SQL_NS_END