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

    /**
     * @brief Generate CREATE TABLE statement for a form type
     *
     * @tparam FormType The ORM form type that defines the table structure
     * @param tableName Name of the database table
     * @return Complete CREATE TABLE SQL statement
     */
    template <typename FormType>
    static IoResult<std::string> generateCreateTable(std::string_view tableName) {
        static_assert(std::is_class_v<FormType>, "FormType must be a class type");

        std::vector<std::string>                     columnDefinitions;
        std::vector<std::pair<std::string, SqlTags>> columnInfo;

        // This would need to be implemented based on the specific ORM form reflection system
        // For now, we provide the interface that can be extended when form reflection is available

        // Extract column information from FormType using reflection/meta-programming
        // This is a placeholder - actual implementation would depend on the ORM's reflection system
        extractColumnInfo<FormType>(columnInfo);

        // Generate column definitions
        for (const auto &[colName, tags] : columnInfo) {
            // This would need type information from the form - placeholder for now
            auto colDef = generateColumnDefinitionForFormField<FormType>(colName, tags);
            if (!colDef) {
                return Unexpected(colDef.error());
            }
            columnDefinitions.push_back(colDef.value());
        }

        if (columnDefinitions.empty()) {
            throw std::invalid_argument("Cannot generate CREATE TABLE for empty form type");
        }

        // Build CREATE TABLE statement
        std::ostringstream sql;
        sql << "CREATE TABLE ";

        // Add table name with appropriate quoting for the dialect
        if constexpr (std::is_same_v<BackendTag, MysqlTag>) {
            sql << "`" << tableName << "`";
        }
        else if constexpr (std::is_same_v<BackendTag, PostgresTag>) {
            sql << "\"" << tableName << "\"";
        }
        else {
            sql << tableName; // SQLite doesn't require quoting
        }

        sql << " (\n";

        // Add column definitions
        for (size_t i = 0; i < columnDefinitions.size(); ++i) {
            if (i > 0)
                sql << ",\n";
            sql << "    " << columnDefinitions[i];
        }

        sql << "\n)";

        return sql.str();
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
        std::vector<std::string> statements;

        // Generate CREATE TABLE statement
        statements.push_back(generateCreateTable<FormType>(tableName).value_or(""));

        // Extract column information for index generation
        std::vector<std::pair<std::string, SqlTags>> columnInfo;
        extractColumnInfo<FormType>(columnInfo);

        // Generate CREATE INDEX statements
        auto indexStatements = generateIndexStatements(tableName, columnInfo);
        statements.insert(statements.end(), indexStatements.begin(), indexStatements.end());

        return statements;
    }

private:
    /**
     * @brief Extract column information from FormType using reflection
     *
     * This is a placeholder method that would be implemented based on the ORM's
     * reflection/meta-programming system to extract field names and SqlTags.
     */
    template <typename FormType>
    static void extractColumnInfo(std::vector<std::pair<std::string, SqlTags>> &columnInfo) {
        FormType obj; // FormType 需要默认构造函数
        NEKO_NAMESPACE::Reflect<FormType>::forEach(
            obj, [&](const auto & /*field*/, std::string_view name, const SqlTags &tags) {
                columnInfo.emplace_back(std::string(name), tags);
            });
    }

    /**
     * @brief Generate column definition for a specific form field
     *
     * This method would use the form's type information to generate the appropriate
     * column definition. It's a placeholder for the actual implementation.
     */
    template <typename FormType>
    static IoResult<std::string> generateColumnDefinitionForFormField(const std::string &columnName,
                                                                      const SqlTags     &tags) {
        std::string result;
        bool        found = false;
        FormType    obj; // 实体对象
        // 这是一个运行时遍历来匹配列名的过程（略显低效但通用）
        // 更高效的方法是在 FormType 中建立列名到类型的映射，但需要更复杂的元编程
        NEKO_NAMESPACE::Reflect<FormType>::forEach(obj, [&](const auto &field, std::string_view name) {
            if (!found && name == columnName) {
                // 找到对应的字段，使用其实际类型 T 调用生成函数
                result = generateColumnDefinition<std::decay_t<decltype(field)>>(columnName, tags);
                found  = true;
            }
        });
        if (!found) {
            return Unexpected(SqlError::Code::ColumnNotFound);
        }
        return result;
    }
};

} // namespace detail

ILIAS_SQL_NS_END