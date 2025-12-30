#pragma once

#include "ilias/sql/sqlerror.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include <string>
#include <vector>
#include <optional>

ILIAS_SQL_NS_BEGIN
namespace detail {

/**
 * @brief Enhanced error information that includes SqlTags constraint context
 * 
 * This class extends the basic SqlError with additional context about
 * which SqlTags constraints were involved in the error, providing better
 * debugging and error handling capabilities.
 */
class ILIAS_SQL_API EnhancedSqlError {
public:
    /**
     * @brief Construct enhanced error from basic SqlError
     */
    EnhancedSqlError(const SqlError& baseError);
    
    /**
     * @brief Construct enhanced error with constraint context
     */
    EnhancedSqlError(const SqlError& baseError, 
                     const std::string& tableName,
                     const std::string& columnName,
                     const SqlTags& constraintTags);
    
    /**
     * @brief Construct enhanced error with multiple constraint contexts
     */
    EnhancedSqlError(const SqlError& baseError,
                     const std::string& tableName,
                     const std::vector<std::pair<std::string, SqlTags>>& constraintContexts);
    
    /**
     * @brief Get the underlying SqlError
     */
    const SqlError& getBaseError() const { return mBaseError; }
    
    /**
     * @brief Get table name where the error occurred
     */
    const std::string& getTableName() const { return mTableName; }
    
    /**
     * @brief Get column name where the error occurred (if applicable)
     */
    const std::optional<std::string>& getColumnName() const { return mColumnName; }
    
    /**
     * @brief Get SqlTags that were involved in the constraint violation
     */
    const std::optional<SqlTags>& getConstraintTags() const { return mConstraintTags; }
    
    /**
     * @brief Get all constraint contexts (for multi-column violations)
     */
    const std::vector<std::pair<std::string, SqlTags>>& getConstraintContexts() const { 
        return mConstraintContexts; 
    }
    
    /**
     * @brief Get enhanced error message with constraint context
     */
    std::string getEnhancedMessage() const;
    
    /**
     * @brief Get structured error information for programmatic handling
     */
    struct ErrorContext {
        SqlError::Code errorCode;
        std::string tableName;
        std::optional<std::string> columnName;
        std::optional<SqlTags> constraintTags;
        std::vector<std::string> violatedConstraints;
        std::string enhancedMessage;
    };
    
    ErrorContext getErrorContext() const;
    
    /**
     * @brief Check if this error is related to a specific constraint type
     */
    bool isConstraintViolation() const;
    bool isPrimaryKeyViolation() const;
    bool isUniqueConstraintViolation() const;
    bool isNotNullViolation() const;
    bool isForeignKeyViolation() const;
    
    /**
     * @brief Get list of violated constraints based on SqlTags
     */
    std::vector<std::string> getViolatedConstraints() const;

private:
    SqlError mBaseError;
    std::string mTableName;
    std::optional<std::string> mColumnName;
    std::optional<SqlTags> mConstraintTags;
    std::vector<std::pair<std::string, SqlTags>> mConstraintContexts;
    
    /**
     * @brief Analyze SqlTags to determine which constraints were likely violated
     */
    std::vector<std::string> analyzeViolatedConstraints(const SqlTags& tags) const;
    
    /**
     * @brief Generate enhanced error message with constraint details
     */
    std::string generateEnhancedMessage() const;
};

/**
 * @brief Error handler that can enhance SqlErrors with SqlTags context
 */
class ILIAS_SQL_API SqlTagsErrorHandler {
public:
    /**
     * @brief Enhance a SqlError with constraint context from SqlTags
     */
    static EnhancedSqlError enhanceError(const SqlError& error,
                                       const std::string& tableName,
                                       const std::string& columnName,
                                       const SqlTags& tags);
    
    /**
     * @brief Enhance a SqlError with multiple constraint contexts
     */
    static EnhancedSqlError enhanceError(const SqlError& error,
                                       const std::string& tableName,
                                       const std::vector<std::pair<std::string, SqlTags>>& contexts);
    
    /**
     * @brief Try to map database-specific error messages to SqlTags constraints
     */
    static std::optional<SqlError::Code> mapDatabaseErrorToConstraintType(
        const std::string& databaseErrorMessage);
    
    /**
     * @brief Create appropriate SqlError::Code based on violated SqlTags constraints
     */
    static SqlError::Code determineErrorCodeFromTags(const SqlTags& tags);
};

} // namespace detail
ILIAS_SQL_NS_END