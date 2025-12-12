#pragma once

#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include "ilias/sql/dialect.hpp"

#include "ilias/sql/detail/orm_types.hpp"
#include "ilias/sql/detail/orm_condition.hpp"
#include "ilias/sql/detail/orm_builder.hpp"
#include "ilias/sql/detail/orm_table_ops.hpp"

ILIAS_SQL_NS_BEGIN

template <typename T, typename Tag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form;
template <typename T, typename Tag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class TableAlias;

template <typename T, typename BackendTag = SqliteTag> // 默认可以是 SQLite
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form final : public TableOperations<Form<T, BackendTag>, T, BackendTag> {
    friend class TableOperations<Form<T, BackendTag>, T, BackendTag>;

public:
    using type    = T;
    using Dialect = Dialect<BackendTag>;

    static auto create(SqlDatabase &db, const std::string &tableName) -> IoTask<Form> {
        if (!Dialect::check(db->sqlname())) {
            ILIAS_ERROR("ilias-sql", "Dialect {} is not supported", db->sqlname());
            co_return Unexpected(SqlError::DialectNotSupported);
        }
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

    auto getColumnNames() const -> const std::vector<std::string> & { return mTableHeaderNames; }
    auto getColumnTags() const -> const std::vector<SqlTags> & { return mTableHeaderTags; }
    auto getColumnIndex() const -> const std::map<std::ptrdiff_t, int> & { return mTableHeaderIndex; }
    auto getPrimaryKey() const -> const std::string & { return mPrimaryKey; }
    auto getTableName() const -> const std::string & { return mTableName; }
    auto tableRef() const -> const std::string & { return mTableName; }
    auto getAlias() const -> const std::string & { return mTableName; }
    auto db() -> SqlDatabase & { return mDb; }
    auto db() const -> SqlDatabase & { return mDb; }

    auto as(const std::string &alias);

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
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class TableAlias final : public TableOperations<TableAlias<T, BackendTag>, T, BackendTag> {
    friend class TableOperations<TableAlias<T, BackendTag>, T, BackendTag>;

public:
    using type    = T;
    using Dialect = Dialect<BackendTag>;

    TableAlias(const std::string &alias, Form<T, BackendTag> &form) : mAlias(alias), mForm(form) {}

    template <typename M>
    auto col(M T::*memberPtr) const {
        std::string rawColName = mForm.getColumnName(memberPtr).value();
        return detail::TypedColumn<std::decay_t<M>>(mAlias + "." + rawColName);
    }
    auto tableRef() const -> std::string { return mForm.getTableName() + " AS " + mAlias; }
    auto getAlias() const -> const std::string & { return mAlias; }
    auto getTableName() const -> const std::string & { return mForm.getTableName(); }
    auto getColumnNames() const -> const std::vector<std::string> & { return mForm.getColumnNames(); }
    auto getColumnTags() const -> const std::vector<SqlTags> & { return mForm.getColumnTags(); }
    auto getColumnIndex() const -> const std::map<std::ptrdiff_t, int> & { return mForm.getColumnIndex(); }
    auto getPrimaryKey() const -> const std::string & { return mForm.getPrimaryKey(); }
    auto db() -> SqlDatabase & { return mForm.db(); }
    auto db() const -> const SqlDatabase & { return mForm.db(); }

    auto as(const std::string &alias) { return TableAlias(alias, mForm); }

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