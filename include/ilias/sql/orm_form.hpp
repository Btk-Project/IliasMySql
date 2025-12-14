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
    using type           = T;
    using BackendDialect = Dialect<BackendTag>;

    static auto create(SqlDatabase &db, const std::string &tableName) -> IoTask<Form> {
        if (!BackendDialect::check(db->sqlname())) {
            ILIAS_ERROR("ilias-sql", "Dialect {} is not supported", db->sqlname());
            co_return Unexpected(SqlError::DialectNotSupported);
        }
        T                        obj;
        std::vector<std::string> colDefs;

        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const SqlTags &tags) {
            std::string typeStr = std::string(BackendDialect::template type_name<decltype(field)>());
            std::string colDef  = std::string(name) + " " + typeStr;
            if (tags.primary_key) {
                colDef += " " + std::string(BackendDialect::primary_key());
            }
            if (tags.auto_increment)
                colDef += " " + std::string(BackendDialect::auto_increment());
            if (tags.unique)
                colDef += " UNIQUE";
            if (tags.not_null)
                colDef += " NOT NULL";
            colDefs.push_back(colDef);
        });

        std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (" + detail::join_strs(colDefs, ", ") + ")";
        auto        ret = co_await db.execute(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        Form form(db, tableName);
        ILIAS_TRACE("ilias-sql", "Created table {}, columns: {}, primary key: {}", tableName,
                    detail::join_strs(form.getColumnNames(), ", "), form.getPrimaryKey());
        co_return form;
    }

    static auto getColumnNames() noexcept -> const std::vector<std::string> & { return mTableHeaderNames; }
    static auto getColumnTags() noexcept -> const std::vector<SqlTags> & { return mTableHeaderTags; }
    static auto getColumnIndex() noexcept -> const std::map<std::ptrdiff_t, int> & { return mTableHeaderIndex; }
    static auto getPrimaryKey() noexcept -> const std::string & { return mPrimaryKey; }

    auto getTableName() const -> const std::string & { return mTableName; }
    auto tableRef() const -> const std::string & { return mTableName; }
    auto getAlias() const -> const std::string & { return mTableName; }
    auto db() -> SqlDatabase & { return mDb; }
    auto db() const -> SqlDatabase & { return mDb; }

    auto as(const std::string &alias);

private:
    Form(SqlDatabase &db, const std::string &tableName) : mDb(db), mTableName(tableName) {}

    SqlDatabase                         &mDb;
    std::string                          mTableName;
    static std::vector<std::string>      mTableHeaderNames;
    static std::vector<SqlTags>          mTableHeaderTags;
    static std::map<std::ptrdiff_t, int> mTableHeaderIndex;
    static std::string                   mPrimaryKey;
};

template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::vector<std::string> Form<T, BackendTag>::mTableHeaderNames = []() {
    auto names = NEKO_NAMESPACE::Reflect<T>::names();
    return std::vector<std::string>(names.begin(), names.end());
}();

template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::vector<SqlTags> Form<T, BackendTag>::mTableHeaderTags = []() {
    std::vector<SqlTags> tags_array;
    tags_array.resize(NEKO_NAMESPACE::Reflect<T>::value_count);
    auto tags = NEKO_NAMESPACE::Reflect<T>::value_tags; // this is a tuple, may be has other tags in the field
    [&tags, &tags_array]<std::size_t... I>(std::index_sequence<I...>) {
        ((tags_array[I] = std::get<I>(tags)), ...);
    }(std::make_index_sequence<NEKO_NAMESPACE::Reflect<T>::value_count>());
    return tags_array;
}();

template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::map<std::ptrdiff_t, int> Form<T, BackendTag>::mTableHeaderIndex = []() {
    T                             obj;
    std::map<std::ptrdiff_t, int> indexMap;
    NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field) {
        auto field_ptr      = (char *)(&field) - (char *)(&obj);
        indexMap[field_ptr] = static_cast<int>(indexMap.size());
    });
    return indexMap;
}();

template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::string Form<T, BackendTag>::mPrimaryKey = []() {
    T           obj;
    std::string ret;
    NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto & /*field*/, std::string_view name, const SqlTags &tags) {
        if (tags.primary_key) {
            ret = std::string(name);
        }
    });
    return ret;
}();

template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class TableAlias final : public TableOperations<TableAlias<T, BackendTag>, T, BackendTag> {
    friend class TableOperations<TableAlias<T, BackendTag>, T, BackendTag>;

public:
    using type           = T;
    using BackendDialect = Dialect<BackendTag>;

    TableAlias(const std::string &alias, Form<T, BackendTag> &form) : mAlias(alias), mForm(form) {}

    template <typename M>
    auto col(M T::*memberPtr) const {
        std::string rawColName = mForm.getColumnName(memberPtr).value();
        return detail::TypedColumn<std::decay_t<M>>(mAlias + "." + rawColName);
    }
    auto tableRef() const -> std::string { return mForm.getTableName() + " AS " + mAlias; }
    auto getAlias() const -> const std::string & { return mAlias; }
    auto getTableName() const -> const std::string & { return mForm.getTableName(); }
    auto db() -> SqlDatabase & { return mForm.db(); }
    auto db() const -> const SqlDatabase & { return mForm.db(); }

    static decltype(auto) getColumnTags() noexcept { return Form<T, BackendDialect>::getColumnTags(); }
    static decltype(auto) getColumnNames() noexcept { return Form<T, BackendDialect>::getColumnNames(); }
    static decltype(auto) getColumnIndex() noexcept { return Form<T, BackendDialect>::getColumnIndex(); }
    static decltype(auto) getPrimaryKey() noexcept { return Form<T, BackendDialect>::getPrimaryKey(); }

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