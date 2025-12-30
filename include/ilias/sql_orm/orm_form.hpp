#pragma once

#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include "ilias/sql_orm/dialect.hpp"

#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_condition.hpp"
#include "ilias/sql_orm/detail/orm_builder.hpp"
#include "ilias/sql_orm/detail/orm_table_ops.hpp"
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sql_orm/detail/timestamp_manager.hpp"

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
        
        // Validate SqlTags configuration before creating table
        auto validation_errors = validateTableConfiguration();
        if (!validation_errors.empty()) {
            std::string error_msg = "SqlTags validation failed: " + detail::join_strs(validation_errors, "; ");
            ILIAS_ERROR("ilias-sql", "{}", error_msg);
            co_return Unexpected(SqlError::InvalidParameter);
        }
        
        T                        obj;
        std::vector<std::string> colDefs;

        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const SqlTags &tags) {
            std::string colDef = BackendDialect::template generate_column_definition<decltype(field)>(name, tags);
            colDefs.push_back(colDef);
        });

        // Generate additional index statements if needed
        auto indexStatements = generateIndexStatements(tableName);

        std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (" + detail::join_strs(colDefs, ", ") + ")";
        auto        ret = co_await db.execute(sql);
        if (!ret)
            co_return Unexpected(ret.error());

        // Execute index creation statements
        for (const auto& indexSql : indexStatements) {
            auto indexRet = co_await db.execute(indexSql);
            if (!indexRet) {
                ILIAS_WARN("ilias-sql", "Failed to create index: {}", indexSql);
                // Continue with other indexes even if one fails
            }
        }

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

    // =========================================================
    // Enhanced SqlTags Helper Methods
    // =========================================================
    
    /**
     * @brief Validate the SqlTags configuration for the entire table
     * 
     * Checks all field configurations for conflicts and invalid combinations.
     * @return Vector of validation error messages (empty if valid)
     */
    static std::vector<std::string> validateTableConfiguration() {
        std::vector<std::string> errors;
        
        T obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, 
            [&](const auto& /*field*/, std::string_view name, const SqlTags& tags) {
                auto fieldErrors = tags.getValidationErrors();
                for (const auto& error : fieldErrors) {
                    errors.push_back(std::string(name) + ": " + error);
                }
            });
        
        return errors;
    }
    
    /**
     * @brief Generate index creation statements for fields marked with index = true
     * 
     * @param tableName The name of the table to create indexes for
     * @return Vector of CREATE INDEX SQL statements
     */
    static std::vector<std::string> generateIndexStatements(const std::string& tableName) {
        std::vector<std::string> indexStatements;
        
        T obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, 
            [&](const auto& /*field*/, std::string_view name, const SqlTags& tags) {
                if (tags.requiresIndex() && !tags.primary_key && !tags.unique) {
                    // Only create explicit indexes for fields marked with index = true
                    // Primary key and unique constraints create their own indexes
                    std::string indexName = tableName + "_" + std::string(name) + "_idx";
                    std::string indexSql = "CREATE INDEX IF NOT EXISTS " + indexName + 
                                         " ON " + tableName + " (" + std::string(name) + ")";
                    indexStatements.push_back(indexSql);
                }
            });
        
        return indexStatements;
    }
    
    /**
     * @brief Get list of field names that have timestamp behavior (created_at or updated_at)
     * 
     * @return Vector of field names with timestamp automation
     */
    static std::vector<std::string> getTimestampFields() {
        std::vector<std::string> timestampFields;
        
        T obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, 
            [&](const auto& /*field*/, std::string_view name, const SqlTags& tags) {
                if (tags.hasTimestampBehavior()) {
                    timestampFields.emplace_back(name);
                }
            });
        
        return timestampFields;
    }
    
    /**
     * @brief Get list of field names that have created_at behavior
     * 
     * @return Vector of field names that should be auto-populated on insert
     */
    static std::vector<std::string> getCreatedAtFields() {
        std::vector<std::string> createdAtFields;
        
        T obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, 
            [&](const auto& /*field*/, std::string_view name, const SqlTags& tags) {
                if (detail::TimestampManager::shouldApplyCreatedAt(tags)) {
                    createdAtFields.emplace_back(name);
                }
            });
        
        return createdAtFields;
    }
    
    /**
     * @brief Get list of field names that have updated_at behavior
     * 
     * @return Vector of field names that should be auto-updated on update
     */
    static std::vector<std::string> getUpdatedAtFields() {
        std::vector<std::string> updatedAtFields;
        
        T obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, 
            [&](const auto& /*field*/, std::string_view name, const SqlTags& tags) {
                if (detail::TimestampManager::shouldApplyUpdatedAt(tags)) {
                    updatedAtFields.emplace_back(name);
                }
            });
        
        return updatedAtFields;
    }
    
    /**
     * @brief Create SqlTags configuration for a primary key field
     * 
     * Helper method for common primary key configuration.
     * @param autoIncrement Whether the primary key should auto-increment
     * @return SqlTags configured for primary key usage
     */
    static constexpr SqlTags createPrimaryKeyTags(bool autoIncrement = false) {
        return SqlTags{
            .primary_key = true,
            .not_null = true,
            .unique = true,
            .auto_increment = autoIncrement
        };
    }
    
    /**
     * @brief Create SqlTags configuration for a unique indexed field
     * 
     * Helper method for fields that need unique constraint and indexing.
     * @param length String length for VARCHAR fields (0 for TEXT)
     * @return SqlTags configured for unique indexed field
     */
    static constexpr SqlTags createUniqueIndexTags(int length = 0) {
        return SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,
            .length = length
        };
    }
    
    /**
     * @brief Create SqlTags configuration for a timestamp field
     * 
     * Helper method for automatic timestamp fields.
     * @param isCreatedAt True for created_at behavior, false for updated_at
     * @return SqlTags configured for timestamp automation
     */
    static constexpr SqlTags createTimestampTags(bool isCreatedAt = true) {
        return SqlTags{
            .not_null = true,
            .created_at = isCreatedAt,
            .updated_at = !isCreatedAt
        };
    }
    
    /**
     * @brief Create SqlTags configuration for a string field with length constraint
     * 
     * Helper method for VARCHAR fields with specific length.
     * @param length Maximum string length
     * @param required Whether the field is required (not null)
     * @param indexed Whether the field should be indexed
     * @return SqlTags configured for string field
     */
    static constexpr SqlTags createStringTags(int length, bool required = true, bool indexed = false) {
        return SqlTags{
            .not_null = required,
            .index = indexed,
            .length = length
        };
    }
    
    /**
     * @brief Create SqlTags configuration for a numeric field
     * 
     * Helper method for numeric fields with optional unsigned constraint.
     * @param required Whether the field is required (not null)
     * @param isUnsigned Whether the field should be unsigned
     * @return SqlTags configured for numeric field
     */
    static constexpr SqlTags createNumericTags(bool required = true, bool isUnsigned = false) {
        return SqlTags{
            .not_null = required,
            .unsigned_type = isUnsigned
        };
    }

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

    // Enhanced SqlTags helper methods (delegate to Form)
    static decltype(auto) validateTableConfiguration() { return Form<T, BackendTag>::validateTableConfiguration(); }
    static decltype(auto) generateIndexStatements(const std::string& tableName) { return Form<T, BackendTag>::generateIndexStatements(tableName); }
    static decltype(auto) getTimestampFields() { return Form<T, BackendTag>::getTimestampFields(); }
    static decltype(auto) getCreatedAtFields() { return Form<T, BackendTag>::getCreatedAtFields(); }
    static decltype(auto) getUpdatedAtFields() { return Form<T, BackendTag>::getUpdatedAtFields(); }
    static constexpr decltype(auto) createPrimaryKeyTags(bool autoIncrement = false) { return Form<T, BackendTag>::createPrimaryKeyTags(autoIncrement); }
    static constexpr decltype(auto) createUniqueIndexTags(int length = 0) { return Form<T, BackendTag>::createUniqueIndexTags(length); }
    static constexpr decltype(auto) createTimestampTags(bool isCreatedAt = true) { return Form<T, BackendTag>::createTimestampTags(isCreatedAt); }
    static constexpr decltype(auto) createStringTags(int length, bool required = true, bool indexed = false) { return Form<T, BackendTag>::createStringTags(length, required, indexed); }
    static constexpr decltype(auto) createNumericTags(bool required = true, bool isUnsigned = false) { return Form<T, BackendTag>::createNumericTags(required, isUnsigned); }

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