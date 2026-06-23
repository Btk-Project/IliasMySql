#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/dialect.hpp"

#include <nekoproto/serialization/reflection.hpp>
#include <string>
#include <vector>
#include <sstream>
#include <stdexcept>

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
        // Validate SqlTags configuration before generating SQL
        auto errors = tags.getValidationErrors<T>();
        if (!errors.empty()) {
            std::ostringstream oss;
            oss << "Invalid SqlTags configuration for column '" << columnName << "': ";
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << errors[i];
            }
            throw std::invalid_argument(oss.str());
        }

        // Delegate to the dialect-specific implementation
        return Dialect<BackendTag>::template generate_column_definition<T>(columnName, tags);
    }

    template <typename EntityType>
    static IoResult<TableSchema> generateTableSchema(std::string_view tableName, bool ifNotExists = false) {
        static_assert(std::is_class_v<EntityType>, "EntityType must be a class type");

        try {
            TableSchema schema;
            EntityType  obj;
            NEKO_NAMESPACE::Reflect<EntityType>::forEach(
                obj, [&](const auto &field, std::string_view name, const auto &tags) {
                    const auto columnName = detail::reflectedFieldName(name, tags);
                    const auto sqlTags    = detail::extractSqlTags(tags);
                    schema.columns.emplace_back(std::string(columnName), sqlTags);
                    schema.columnDefinitions.push_back(
                        generateColumnDefinition<std::decay_t<decltype(field)>>(columnName, sqlTags));
                });

            if (schema.columnDefinitions.empty()) {
                return Err(SqlError::Code::InvalidParameter);
            }

            schema.indexStatements = generateIndexStatements(tableName, schema.columns);

            std::ostringstream sql;
            sql << "CREATE TABLE ";
            if (ifNotExists) {
                sql << "IF NOT EXISTS ";
            }
            sql << Dialect<BackendTag>::quote_identifier(tableName) << " ("
                << detail::join_strs(schema.columnDefinitions, ", ") << ")";
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
};

} // namespace detail

ILIAS_SQL_NS_END
