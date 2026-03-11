/**
 * @file orm_form.hpp
 * @brief ORM Form class for database table operations
 *
 * This file provides the Form template class for object-relational mapping,
 * allowing C++ structs to be mapped to database tables with type-safe operations.
 */

#pragma once

#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include "ilias/sql_orm/dialect.hpp"
#include <type_traits>

#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_condition.hpp"
#include "ilias/sql_orm/detail/orm_builder.hpp"
#include "ilias/sql_orm/detail/orm_table_ops.hpp"

ILIAS_SQL_NS_BEGIN

/**
 * @brief Forward declaration of Form template
 */
template <typename T, typename Tag, typename DatabaseT = void>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form;

/**
 * @brief Forward declaration of TableAlias template
 */
template <typename T, typename Tag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class TableAlias;

/**
 * @brief ORM Form class for database table operations
 *
 * Provides type-safe database operations for C++ structs,
 * including table creation, querying, inserting, updating, and deleting.
 *
 * @tparam T The struct type to map to the database table
 * @tparam BackendTag The database backend tag (e.g., SqliteTag, MysqlTag)
 * @tparam DatabaseT The database type (void for static operations)
 */
template <typename T, typename BackendTag>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form<T, BackendTag, void> final {
public:
    using BackendDialect = Dialect<BackendTag>;
    /**
     * @brief 强制重建模式：如果表存在，则删除并重新创建。
     * @return IoTask<Form> on success, IoTask<Unexpected<SqlError>> on failure.
     */
    template <typename DatabaseT>
        requires(!std::is_const_v<DatabaseT>)
    static auto create_or_replace(DatabaseT &db, const std::string &tableName)
        -> IoTask<Form<T, BackendTag, DatabaseT>> {
        // 1. 先 Drop
        std::string drop_sql = "DROP TABLE IF EXISTS " + tableName; // 注意为表名添加引号/反引号
        auto        drop_ret = co_await db.execute(drop_sql);
        if (!drop_ret) {
            co_return Unexpected(drop_ret.error());
        }
        // 2. 再 Create
        co_return co_await _create_table_impl(db, tableName, false);
    }

    /**
     * @brief 温柔创建模式：如果表不存在，则创建它。
     */
    template <typename DatabaseT>
        requires(!std::is_const_v<DatabaseT>)
    static auto create_if_not_exists(DatabaseT &db, const std::string &tableName)
        -> IoTask<Form<T, BackendTag, DatabaseT>> {
        co_return co_await _create_table_impl(db, tableName, true);
    }

    /**
     * @brief 附加并校验模式：附加到已存在的表，并严格校验其结构。
     */
    template <typename DatabaseT>
        requires(!std::is_const_v<DatabaseT>)
    static auto attach(DatabaseT &db, const std::string &tableName) -> IoTask<Form<T, BackendTag, DatabaseT>> {
        // 1. 从Dialect获取查询元数据的SQL
        // 注意：SQLite的PRAGMA不支持绑定参数，所以特殊处理
        std::string schema_query_sql;
        if constexpr (std::is_same_v<BackendTag, SqliteTag>) {
            schema_query_sql = BackendDialect::get_schema_query(tableName);
        }
        else {
            schema_query_sql = BackendDialect::get_schema_query();
        }

        // 2. 执行查询
        auto prepare_ret = co_await db.prepare(schema_query_sql);
        if (!prepare_ret) {
            ILIAS_ERROR("ilias-sql", "Attach failed for table '{}': prepare failed.", tableName);
            co_return Unexpected(prepare_ret.error());
        }
        if constexpr (!std::is_same_v<BackendTag, SqliteTag>) {
            prepare_ret->bind(tableName);
        }
        auto result = co_await prepare_ret->query();

        if (!result) {
            // 查询失败，可能是表不存在
            ILIAS_ERROR("ilias-sql", "Attach failed for table '{}': query for schema failed.", tableName);
            co_return Unexpected(result.error());
        }

        // 3. 解析结果
        auto actual_schema = co_await BackendDialect::parse_schema_result(std::move(result.value()));
        if (!actual_schema) {
            ILIAS_ERROR("ilias-sql", "Attach failed for table '{}': parse schema result failed.", tableName);
            co_return Unexpected(actual_schema.error());
        }
        if ((*actual_schema).empty()) {
            ILIAS_ERROR("ilias-sql", "Attach failed: Table '{}' does not exist or has no columns.", tableName);
            co_return Unexpected(SqlError::Code::TableNotFound);
        }

        // 4. 校验Schema
        auto validation = _validate_schema(*actual_schema);
        if (!validation.is_ok()) {
            std::string error_msg =
                "Schema validation failed for table '" + tableName + "': " + detail::join_strs(validation.errors, "; ");
            ILIAS_ERROR("ilias-sql", "{}", error_msg);
            co_return Unexpected(SqlError::SchemaMismatch);
        }

        // 5. 校验通过，创建并返回Form实例
        ILIAS_TRACE("ilias-sql", "Successfully attached to table '{}', schema validated.", tableName);
        co_return Form<T, BackendTag, DatabaseT>(db, tableName);
    }

    template <typename DatabaseT>
    static auto transaction(DatabaseT &db, const std::string &tableName)
        -> IoTask<std::pair<std::unique_ptr<SqlTransaction>, Form<T, BackendTag, SqlTransaction>>> {
        auto ret = co_await db.transaction();
        if (!ret) {
            co_return Unexpected(ret.error());
        }
        auto tx   = std::make_unique<SqlTransaction>(std::move(ret.value()));
        auto form = Form<T, BackendTag, SqlTransaction>(*tx, tableName);
        co_return std::make_pair(std::move(tx), std::move(form));
    }

private:
    // --- 私有辅助函数 ---

    template <typename DatabaseT>
    static auto _create_table_impl(DatabaseT &db, const std::string &tableName, bool if_not_exists)
        -> IoTask<Form<T, BackendTag, DatabaseT>> {
        if (!BackendDialect::check(db.sqlname())) {
            ILIAS_ERROR("ilias-sql", "Dialect {} is not supported", db.sqlname());
            co_return Unexpected(SqlError::DialectNotSupported);
        }

        // Validate SqlTags configuration before creating table
        auto validation_errors =
            detail::TableOperations<Form<T, BackendTag, DatabaseT>, T, BackendTag>::validateTableConfiguration();
        if (!validation_errors.empty()) {
            std::string error_msg = "SqlTags validation failed: " + detail::join_strs(validation_errors, "; ");
            ILIAS_ERROR("ilias-sql", "{}", error_msg);
            co_return Unexpected(SqlError::InvalidParameter);
        }

        T                                            obj;
        std::vector<std::string>                     colDefs;
        std::vector<std::pair<std::string, SqlTags>> colWithTags;

        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const SqlTags &tags) {
            std::string colDef = BackendDialect::template generate_column_definition<decltype(field)>(name, tags);
            colWithTags.emplace_back(name, tags);
            colDefs.push_back(colDef);
        });

        // Generate additional index statements if needed
        auto indexStatements = BackendDialect::generate_index_statements(tableName, colWithTags);

        std::string sql = std::string("CREATE TABLE ") + (if_not_exists ? "IF NOT EXISTS " : "") + tableName + " (" +
                          detail::join_strs(colDefs, ", ") + ")";
        auto ret = co_await db.execute(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        // Execute index creation statements
        for (const auto &indexSql : indexStatements) {
            auto indexRet = co_await db.execute(indexSql);
            if (!indexRet) {
                ILIAS_WARN("ilias-sql", "Failed to create index: {}", indexSql);
                // Continue with other indexes even if one fails
            }
        }

        Form<T, BackendTag, DatabaseT> form(db, tableName);
        ILIAS_TRACE("ilias-sql", "Created table {}, columns: {}, primary key: {}", tableName,
                    detail::join_strs(form.getColumnNames(), ", "), form.getPrimaryKey());
        co_return form;
    }

    static auto _validate_schema(const std::map<std::string, ColumnSchema> &actual_schema) -> ValidationResult {
        ValidationResult      result;
        T                     obj;
        std::set<std::string> struct_columns;

        // 1. 检查C++结构体中的每个字段是否与数据库匹配
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name_sv, const SqlTags &tags) {
            std::string name(name_sv);
            struct_columns.insert(name);

            auto it = actual_schema.find(name);
            if (it == actual_schema.end()) {
                result.add_error("Column '" + name + "' (in C++) not found in database table.");
                return; // 后续检查无意义
            }
            const auto &col_info = it->second;

            // 校验类型
            if (!BackendDialect::template are_types_compatible<decltype(field)>(col_info.db_type, tags)) {
                result.add_error("Column '" + name + "': Type mismatch. DB has '" + col_info.db_type + "'.");
            }

            // 校验 NOT NULL
            // 简单假设：非optional类型需要NOT NULL
            bool expected_not_null = tags.not_null;
            if (expected_not_null != col_info.tags.not_null) {
                result.add_error("Column '" + name +
                                 "': NOT NULL constraint mismatch. Expected: " + std::to_string(expected_not_null) +
                                 ", Actual: " + std::to_string(col_info.tags.not_null));
            }

            // 校验主键
            if (tags.primary_key != col_info.tags.primary_key) {
                result.add_error("Column '" + name + "': PRIMARY KEY constraint mismatch.");
            }
        });

        // 2. 检查数据库中是否有多余的字段
        for (const auto &[db_col_name, _] : actual_schema) {
            if (struct_columns.find(db_col_name) == struct_columns.end()) {
                result.add_error("Column '" + db_col_name + "' (in database) not found in C++ struct.");
            }
        }

        return result;
    }
};

template <typename T, typename BackendTag, typename DatabaseT> // 默认可以是 SQLite
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class Form final : public detail::TableOperations<Form<T, BackendTag, DatabaseT>, T, BackendTag> {
    friend class detail::TableOperations<Form<T, BackendTag, DatabaseT>, T, BackendTag>;
    friend class Form<T, BackendTag, void>;

public:
    using type            = T;
    using BackendDialect  = Dialect<BackendTag>;
    using RawDatabaseType = std::decay_t<DatabaseT>;

    template <typename DatabaseTT>
    static auto create_or_replace(DatabaseT &db, const std::string &tableName) {
        return Form<T, BackendTag, void>::create_or_replace(db, tableName);
    }

    template <typename DatabaseTT>
    static auto create_if_not_exists(DatabaseT &db, const std::string &tableName) {
        return Form<T, BackendTag, void>::create_if_not_exists(db, tableName);
    }

    template <typename DatabaseTT>
    static auto attach(DatabaseT &db, const std::string &tableName) {
        return Form<T, BackendTag, void>::attach(db, tableName);
    }

    static auto getColumnNames() noexcept -> const std::vector<std::string> & { return mTableHeaderNames; }
    static auto getColumnTags() noexcept -> const std::vector<SqlTags> & { return mTableHeaderTags; }
    static auto getColumnIndex() noexcept -> const std::map<std::ptrdiff_t, int> & { return mTableHeaderIndex; }
    static auto getPrimaryKey() noexcept -> const std::string & { return mPrimaryKey; }

    auto getTableName() const -> const std::string & { return mTableName; }
    auto tableRef() const -> const std::string & { return mTableName; }
    auto getAlias() const -> const std::string & { return mTableName; }
    auto db() -> RawDatabaseType & { return mDb; }
    auto db() const -> RawDatabaseType & { return mDb; }

    auto as(const std::string &alias);

    auto transaction() -> IoTask<std::pair<std::unique_ptr<SqlTransaction>, Form<T, BackendTag, SqlTransaction>>> {
        return Form<T, BackendTag, void>::transaction(mDb, mTableName);
    }

private:
    Form(RawDatabaseType &db, const std::string &tableName) : mDb(db), mTableName(tableName) {
        ILIAS_TRACE("ilias-sql", "Form<{}> with database<{}> name {}", (void *)this, (void *)&mDb, tableName);
    }

    RawDatabaseType                     &mDb;
    std::string                          mTableName;
    static std::vector<std::string>      mTableHeaderNames;
    static std::vector<SqlTags>          mTableHeaderTags;
    static std::map<std::ptrdiff_t, int> mTableHeaderIndex;
    static std::string                   mPrimaryKey;
};

template <typename T, typename BackendTag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::vector<std::string> Form<T, BackendTag, DatabaseT>::mTableHeaderNames = []() {
    auto names = NEKO_NAMESPACE::Reflect<T>::names();
    return std::vector<std::string>(names.begin(), names.end());
}();

template <typename T, typename BackendTag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::vector<SqlTags> Form<T, BackendTag, DatabaseT>::mTableHeaderTags = []() {
    std::vector<SqlTags> tags_array;
    tags_array.resize(NEKO_NAMESPACE::Reflect<T>::value_count);
    auto tags = NEKO_NAMESPACE::Reflect<T>::value_tags; // this is a tuple, may be has other tags in the field
    [&tags, &tags_array]<std::size_t... I>(std::index_sequence<I...>) {
        ((tags_array[I] = std::get<I>(tags)), ...);
    }(std::make_index_sequence<NEKO_NAMESPACE::Reflect<T>::value_count>());
    return tags_array;
}();

template <typename T, typename BackendTag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::map<std::ptrdiff_t, int> Form<T, BackendTag, DatabaseT>::mTableHeaderIndex = []() {
    T                             obj;
    std::map<std::ptrdiff_t, int> indexMap;
    NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field) {
        auto field_ptr      = (char *)(&field) - (char *)(&obj);
        indexMap[field_ptr] = static_cast<int>(indexMap.size());
    });
    return indexMap;
}();

template <typename T, typename BackendTag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
std::string Form<T, BackendTag, DatabaseT>::mPrimaryKey = []() {
    T           obj;
    std::string ret;
    NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto & /*field*/, std::string_view name, const SqlTags &tags) {
        if (tags.primary_key) {
            ret = std::string(name);
        }
    });
    return ret;
}();

template <typename T, typename BackendTag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
class TableAlias final : public detail::TableOperations<TableAlias<T, BackendTag, DatabaseT>, T, BackendTag> {
    friend class detail::TableOperations<TableAlias<T, BackendTag, DatabaseT>, T, BackendTag>;

public:
    using type           = T;
    using BackendDialect = Dialect<BackendTag>;
    using DatabaseType   = DatabaseT;

    TableAlias(const std::string &alias, Form<T, BackendTag, DatabaseType> &form) : mAlias(alias), mForm(form) {}

    template <typename M>
    auto col(M T::*memberPtr) const {
        std::string rawColName = mForm.getColumnName(memberPtr).value();
        return detail::TypedColumn<std::decay_t<M>>(mAlias + "." + rawColName);
    }
    auto tableRef() const -> std::string { return mForm.getTableName() + " AS " + mAlias; }
    auto getAlias() const -> const std::string & { return mAlias; }
    auto getTableName() const -> const std::string & { return mForm.getTableName(); }
    auto db() -> DatabaseType & { return mForm.db(); }
    auto db() const -> const DatabaseType & { return mForm.db(); }

    static decltype(auto) getColumnTags() noexcept { return Form<T, BackendDialect, DatabaseType>::getColumnTags(); }
    static decltype(auto) getColumnNames() noexcept { return Form<T, BackendDialect, DatabaseType>::getColumnNames(); }
    static decltype(auto) getColumnIndex() noexcept { return Form<T, BackendDialect, DatabaseType>::getColumnIndex(); }
    static decltype(auto) getPrimaryKey() noexcept { return Form<T, BackendDialect, DatabaseType>::getPrimaryKey(); }

    auto as(const std::string &alias) { return TableAlias(alias, mForm); }

private:
    std::string                        mAlias;
    Form<T, BackendTag, DatabaseType> &mForm;
};

template <typename T, typename BackendTag, typename DatabaseT>
    requires(NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<T>>)
auto Form<T, BackendTag, DatabaseT>::as(const std::string &alias) {
    TableAlias<T, BackendTag, DatabaseT> wrapper(alias, *this);
    return wrapper;
}

ILIAS_SQL_NS_END