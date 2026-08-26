#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

#include <nekoproto/serialization/reflection.hpp>
#include <algorithm>
#include <set>
#include <string>
#include <sstream>
#include <stdexcept>
#include <vector>

ILIAS_SQL_NS_BEGIN

namespace detail {

/**
 * @brief Schema generator for creating database tables and indexes from SqlTags configurations
 *
 * This class template provides database-agnostic schema generation capabilities that work
 * with the existing dialect system to produce database-specific SQL statements.
 */
template <typename BackendTag>
class SchemaGenerator {
public:
    struct TableSchema {
        std::string                                  createTableSql;
        std::vector<std::string>                     columnDefinitions;
        std::vector<std::string>                     tableConstraints;
        std::vector<std::pair<std::string, SqlTags>> columns;
        std::vector<std::string>                     indexStatements;

        auto completeStatements() const -> std::vector<std::string> {
            std::vector<std::string> statements;
            statements.reserve(indexStatements.size() + 1);
            statements.push_back(createTableSql);
            statements.insert(statements.end(), indexStatements.begin(), indexStatements.end());
            return statements;
        }
    };

    /**
     * @brief Generate column definition SQL for a specific field type and SqlTags configuration
     *
     * @tparam T The C++ type of the database field
     * @param columnName Name of the database column
     * @param tags SqlTags configuration for the column
     * @return SQL column definition string
     * @throws std::invalid_argument if the SqlTags configuration is invalid
     */
    template <typename T>
    static std::string generateColumnDefinition(std::string_view columnName, const SqlTags &tags) {
        return generateColumnDefinition<T>(columnName, SqlColumnMetadata {.tags = tags});
    }

    template <typename T>
    static std::string generateColumnDefinition(std::string_view columnName, const SqlColumnMetadata &metadata) {
        // Validate the normalized core and extension metadata before generating SQL.
        auto errors = metadata.template getValidationErrors<T>();
        if (!errors.empty()) {
            std::ostringstream oss;
            oss << "Invalid SQL column metadata for column '" << columnName << "': ";
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << errors[i];
            }
            throw std::invalid_argument(oss.str());
        }

        // Delegate to the dialect-specific implementation
        return Dialect<BackendTag>::template generate_column_definition<T>(columnName, metadata);
    }

    template <typename EntityType>
    static IoResult<TableSchema> generateTableSchema(std::string_view tableName, bool ifNotExists = false) {
        static_assert(std::is_class_v<EntityType>, "EntityType must be a class type");

        try {
            TableSchema schema;
            EntityType  obj;
            nekoproto::Reflect<EntityType>::forEach(
                obj, [&](const auto &field, std::string_view name, const auto &tags) {
                    if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                        const auto columnName = detail::reflectedFieldName(name, tags);
                        const auto metadata   = detail::extractSqlColumnMetadata(tags);
                        schema.columns.emplace_back(std::string(columnName), metadata.tags);
                        schema.columnDefinitions.push_back(
                            generateColumnDefinition<std::decay_t<decltype(field)>>(columnName, metadata));
                        if (metadata.hasReference()) {
                            schema.tableConstraints.push_back(generateReferenceConstraint(columnName, metadata));
                        }
                    }
                });

            if (schema.columnDefinitions.empty()) {
                return Err(SqlError::Code::InvalidParameter);
            }

            schema.indexStatements = generateIndexStatements(tableName, schema.columns);
            appendTableMetadata<EntityType>(schema, tableName);

            auto definitions = schema.columnDefinitions;
            definitions.insert(definitions.end(), schema.tableConstraints.begin(), schema.tableConstraints.end());

            std::ostringstream sql;
            sql << "CREATE TABLE ";
            if (ifNotExists) {
                sql << "IF NOT EXISTS ";
            }
            sql << Dialect<BackendTag>::quote_identifier(tableName) << " (" << detail::join_strs(definitions, ", ")
                << ")";
            schema.createTableSql = sql.str();

            return schema;
        } catch (const std::invalid_argument &) {
            return Err(SqlError::Code::InvalidParameter);
        }
    }

    /**
     * @brief Generate CREATE TABLE statement for a form type
     *
     * @tparam FormType The ORM form type that defines the table structure
     * @param tableName Name of the database table
     * @return Complete CREATE TABLE SQL statement
     */
    template <typename FormType>
    static IoResult<std::string> generateCreateTable(std::string_view tableName) {
        ILIAS_TRY(auto schema, generateTableSchema<FormType>(tableName));
        return schema.createTableSql;
    }

    /**
     * @brief Generate CREATE INDEX statements for columns that require indexing
     *
     * @param tableName Name of the database table
     * @param columns Vector of column name and SqlTags pairs
     * @return Vector of CREATE INDEX SQL statements
     */
    static std::vector<std::string>
    generateIndexStatements(std::string_view tableName, const std::vector<std::pair<std::string, SqlTags>> &columns) {

        // Delegate to dialect-specific implementation
        return Dialect<BackendTag>::generate_index_statements(tableName, columns);
    }

    /**
     * @brief Generate complete schema including table and indexes
     *
     * @tparam FormType The ORM form type that defines the table structure
     * @param tableName Name of the database table
     * @return Vector containing CREATE TABLE statement followed by CREATE INDEX statements
     */
    template <typename FormType>
    static std::vector<std::string> generateCompleteSchema(std::string_view tableName) {
        auto schema = generateTableSchema<FormType>(tableName);
        if (!schema) {
            return {};
        }
        return schema->completeStatements();
    }

private:
    template <typename EntityType, auto Member>
    static auto quotedMemberColumn() -> std::string {
        constexpr auto index = detail::reflectedMemberPointerIndex<EntityType, Member>();
        static_assert(index >= 0, "Table metadata member must map to a reflected SQL column");
        constexpr auto names = detail::reflectedFieldNames<EntityType>();
        return Dialect<BackendTag>::quote_identifier(names[static_cast<std::size_t>(index)]);
    }

    template <typename EntityType, typename Item>
    static auto tableMemberColumns() -> std::vector<std::string> {
        std::vector<std::string> columns;
        Item::forEachMember([&]<auto Member>() {
            columns.push_back(quotedMemberColumn<EntityType, Member>());
        });
        if (std::set<std::string>(columns.begin(), columns.end()).size() != columns.size()) {
            throw std::invalid_argument("Duplicate column in table constraint");
        }
        return columns;
    }

    template <typename EntityType, typename Item>
    static void appendTableItem(TableSchema &schema, std::string_view tableName,
                                bool &hasTablePrimaryKey) {
        if constexpr (requires { Item::primary_key; }) {
            if (hasTablePrimaryKey) {
                throw std::invalid_argument("Only one table primary key may be declared");
            }
            const auto columns = tableMemberColumns<EntityType, Item>();
            if (std::ranges::any_of(schema.columns, [](const auto &column) {
                    return column.second.primary_key;
                })) {
                throw std::invalid_argument("Column and table primary keys cannot be combined");
            }
            for (const auto &column : columns) {
                const auto unquoted = column.substr(1, column.size() - 2);
                const auto found =
                    std::ranges::find_if(schema.columns, [&](const auto &entry) { return entry.first == unquoted; });
                if (found != schema.columns.end() && found->second.auto_increment) {
                    throw std::invalid_argument("Composite primary keys cannot contain auto-increment columns");
                }
                if (found != schema.columns.end() && !found->second.not_null) {
                    throw std::invalid_argument("Composite primary key columns must be NOT NULL");
                }
            }
            schema.tableConstraints.push_back("PRIMARY KEY (" + detail::join_strs(columns, ", ") + ")");
            hasTablePrimaryKey = true;
        }
        else if constexpr (requires { Item::check; }) {
            schema.tableConstraints.push_back("CHECK (" + std::string(Item::expression) + ")");
        }
        else if constexpr (requires { Item::index; }) {
            std::vector<std::string> columns;
            std::set<std::string>    memberColumns;
            Item::forEachColumn([&]<typename Column>() {
                auto column = quotedMemberColumn<EntityType, Column::member>();
                if (!memberColumns.insert(column).second) {
                    throw std::invalid_argument("Duplicate column in table index");
                }
                column += Column::order == SqlIndexOrder::Desc ? " DESC" : " ASC";
                columns.push_back(std::move(column));
            });
            std::string statement = "CREATE ";
            if (Item::unique) {
                statement += "UNIQUE ";
            }
            statement += "INDEX " + Dialect<BackendTag>::quote_identifier(Item::name) + " ON " +
                         Dialect<BackendTag>::quote_identifier(tableName) + " (" +
                         detail::join_strs(columns, ", ") + ")";
            schema.indexStatements.push_back(std::move(statement));
        }
        else if constexpr (requires { Item::unique; }) {
            const auto columns = tableMemberColumns<EntityType, Item>();
            schema.tableConstraints.push_back("UNIQUE (" + detail::join_strs(columns, ", ") + ")");
        }
        else {
            static_assert(std::is_void_v<Item>, "Unsupported SqlTableMeta item");
        }
    }

    template <typename EntityType>
    static void appendTableMetadata(TableSchema &schema, std::string_view tableName) {
        bool hasTablePrimaryKey = false;
        std::apply(
            [&](const auto &...item) {
                (appendTableItem<EntityType, std::remove_cvref_t<decltype(item)>>(
                     schema, tableName, hasTablePrimaryKey),
                 ...);
            },
            SqlTableMeta<EntityType>::value);
    }

    static std::string generateReferenceConstraint(std::string_view columnName, const SqlColumnMetadata &metadata) {
        std::string reference = "FOREIGN KEY (" + Dialect<BackendTag>::quote_identifier(columnName) + ") REFERENCES " +
                                Dialect<BackendTag>::quote_identifier_path(metadata.reference_table) + " (" +
                                Dialect<BackendTag>::quote_identifier(metadata.reference_column) + ")";
        if (const auto action = detail::referential_action_sql(metadata.on_delete); !action.empty()) {
            reference += " ON DELETE " + std::string(action);
        }
        if (const auto action = detail::referential_action_sql(metadata.on_update); !action.empty()) {
            reference += " ON UPDATE " + std::string(action);
        }
        return reference;
    }
};

} // namespace detail

ILIAS_SQL_NS_END
