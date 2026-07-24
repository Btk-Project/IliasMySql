#include "ilias/sql_orm/detail/enhanced_error.hpp"
#include <sstream>
#include <algorithm>

ILIAS_SQL_NS_BEGIN
namespace detail {

// ================= EnhancedSqlError Implementation =================

EnhancedSqlError::EnhancedSqlError(const SqlError &baseError) : mBaseError(baseError) {
}

EnhancedSqlError::EnhancedSqlError(const SqlError &baseError, std::string_view tableName, std::string_view columnName,
                                   const SqlTags &constraintTags)
    : mBaseError(baseError), mTableName(tableName), mColumnName(columnName), mConstraintTags(constraintTags) {
}

EnhancedSqlError::EnhancedSqlError(const SqlError &baseError, std::string_view tableName,
                                   const std::vector<std::pair<std::string, SqlTags>> &constraintContexts)
    : mBaseError(baseError), mTableName(tableName), mConstraintContexts(constraintContexts) {
}

std::string EnhancedSqlError::getEnhancedMessage() const {
    return generateEnhancedMessage();
}

EnhancedSqlError::ErrorContext EnhancedSqlError::getErrorContext() const {
    ErrorContext context;
    context.errorCode       = mBaseError.error();
    context.tableName       = mTableName;
    context.columnName      = mColumnName;
    context.constraintTags  = mConstraintTags;
    context.enhancedMessage = getEnhancedMessage();

    // Collect violated constraints from all contexts
    if (mConstraintTags.has_value()) {
        auto violations = analyzeViolatedConstraints(mConstraintTags.value());
        context.violatedConstraints.insert(context.violatedConstraints.end(), violations.begin(), violations.end());
    }

    for (const auto &[colName, tags] : mConstraintContexts) {
        auto violations = analyzeViolatedConstraints(tags);
        context.violatedConstraints.insert(context.violatedConstraints.end(), violations.begin(), violations.end());
    }

    return context;
}

bool EnhancedSqlError::isConstraintViolation() const {
    auto code = mBaseError.error();
    return code == SqlError::ConstraintViolation || code == SqlError::PrimaryKeyViolation ||
           code == SqlError::UniqueConstraintViolation || code == SqlError::NotNullViolation ||
           code == SqlError::ForeignKeyViolation || code == SqlError::CheckConstraintViolation;
}

bool EnhancedSqlError::isPrimaryKeyViolation() const {
    return mBaseError.error() == SqlError::PrimaryKeyViolation;
}

bool EnhancedSqlError::isUniqueConstraintViolation() const {
    return mBaseError.error() == SqlError::UniqueConstraintViolation;
}

bool EnhancedSqlError::isNotNullViolation() const {
    return mBaseError.error() == SqlError::NotNullViolation;
}

bool EnhancedSqlError::isForeignKeyViolation() const {
    return mBaseError.error() == SqlError::ForeignKeyViolation;
}

std::vector<std::string> EnhancedSqlError::getViolatedConstraints() const {
    std::vector<std::string> violations;

    if (mConstraintTags.has_value()) {
        auto tagViolations = analyzeViolatedConstraints(mConstraintTags.value());
        violations.insert(violations.end(), tagViolations.begin(), tagViolations.end());
    }

    for (const auto &[colName, tags] : mConstraintContexts) {
        auto tagViolations = analyzeViolatedConstraints(tags);
        violations.insert(violations.end(), tagViolations.begin(), tagViolations.end());
    }

    return violations;
}

std::vector<std::string> EnhancedSqlError::analyzeViolatedConstraints(const SqlTags &tags) const {
    std::vector<std::string> violations;
    auto                     errorCode = mBaseError.error();

    // Map error codes to potential SqlTags constraints
    switch (errorCode) {
        case SqlError::PrimaryKeyViolation:
            if (tags.primary_key) {
                violations.push_back("PRIMARY KEY");
            }
            break;

        case SqlError::UniqueConstraintViolation:
            if (tags.unique) {
                violations.push_back("UNIQUE");
            }
            if (tags.primary_key) {
                violations.push_back("PRIMARY KEY (implies UNIQUE)");
            }
            break;

        case SqlError::NotNullViolation:
            if (tags.not_null) {
                violations.push_back("NOT NULL");
            }
            if (tags.primary_key) {
                violations.push_back("PRIMARY KEY (implies NOT NULL)");
            }
            break;

        case SqlError::ConstraintViolation:
            // Generic constraint violation - check all possible constraints
            if (tags.primary_key)
                violations.push_back("PRIMARY KEY");
            if (tags.not_null)
                violations.push_back("NOT NULL");
            if (tags.unique)
                violations.push_back("UNIQUE");
            if (tags.auto_increment)
                violations.push_back("AUTO_INCREMENT");
            break;

        default:
            // For other error types, still report active constraints for context
            if (tags.primary_key)
                violations.push_back("PRIMARY KEY (context)");
            if (tags.not_null)
                violations.push_back("NOT NULL (context)");
            if (tags.unique)
                violations.push_back("UNIQUE (context)");
            break;
    }

    return violations;
}

std::string EnhancedSqlError::generateEnhancedMessage() const {
    std::ostringstream oss;

    // Start with the base error message
    oss << mBaseError.message();

    // Add table context
    if (!mTableName.empty()) {
        oss << " in table '" << mTableName << "'";
    }

    // Add column context if available
    if (mColumnName.has_value()) {
        oss << " on column '" << mColumnName.value() << "'";
    }

    // Add constraint information
    auto violations = getViolatedConstraints();
    if (!violations.empty()) {
        oss << ". Violated constraints: ";
        for (size_t i = 0; i < violations.size(); ++i) {
            if (i > 0)
                oss << ", ";
            oss << violations[i];
        }
    }

    // Add SqlTags configuration details for debugging
    if (mConstraintTags.has_value()) {
        const auto &tags = mConstraintTags.value();
        oss << ". SqlTags configuration: {";

        std::vector<std::string> activeFlags;
        if (tags.primary_key)
            activeFlags.push_back("primary_key");
        if (tags.not_null)
            activeFlags.push_back("not_null");
        if (tags.unique)
            activeFlags.push_back("unique");
        if (tags.auto_increment)
            activeFlags.push_back("auto_increment");
        if (tags.index)
            activeFlags.push_back("index");
        if (tags.unsigned_type)
            activeFlags.push_back("unsigned_type");
        if (tags.created_at)
            activeFlags.push_back("created_at");
        if (tags.updated_at)
            activeFlags.push_back("updated_at");

        for (size_t i = 0; i < activeFlags.size(); ++i) {
            if (i > 0)
                oss << ", ";
            oss << activeFlags[i] << "=true";
        }

        if (tags.length > 0) {
            if (!activeFlags.empty())
                oss << ", ";
            oss << "length=" << tags.length;
        }

        oss << "}";
    }

    // Add multiple constraint contexts if available
    if (!mConstraintContexts.empty()) {
        oss << ". Multiple constraint contexts: ";
        for (size_t i = 0; i < mConstraintContexts.size(); ++i) {
            if (i > 0)
                oss << ", ";
            const auto &[colName, tags] = mConstraintContexts[i];
            oss << colName << " (";

            std::vector<std::string> activeFlags;
            if (tags.primary_key)
                activeFlags.push_back("PK");
            if (tags.not_null)
                activeFlags.push_back("NN");
            if (tags.unique)
                activeFlags.push_back("UQ");
            if (tags.auto_increment)
                activeFlags.push_back("AI");

            for (size_t j = 0; j < activeFlags.size(); ++j) {
                if (j > 0)
                    oss << ",";
                oss << activeFlags[j];
            }
            oss << ")";
        }
    }

    return oss.str();
}

// ================= SqlTagsErrorHandler Implementation =================

EnhancedSqlError SqlTagsErrorHandler::enhanceError(const SqlError &error, std::string_view tableName,
                                                   std::string_view columnName, const SqlTags &tags) {
    return EnhancedSqlError(error, tableName, columnName, tags);
}

EnhancedSqlError SqlTagsErrorHandler::enhanceError(const SqlError &error, std::string_view tableName,
                                                   const std::vector<std::pair<std::string, SqlTags>> &contexts) {
    return EnhancedSqlError(error, tableName, contexts);
}

std::optional<SqlError::Code>
SqlTagsErrorHandler::mapDatabaseErrorToConstraintType(std::string_view databaseErrorMessage) {

    // Convert to lowercase for case-insensitive matching
    std::string lowerMessage {databaseErrorMessage};
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(), ::tolower);

    // Common database error message patterns
    if (lowerMessage.find("primary key") != std::string::npos ||
        lowerMessage.find("duplicate entry") != std::string::npos) {
        return SqlError::PrimaryKeyViolation;
    }

    if (lowerMessage.find("unique constraint") != std::string::npos ||
        lowerMessage.find("unique violation") != std::string::npos) {
        return SqlError::UniqueConstraintViolation;
    }

    if (lowerMessage.find("not null") != std::string::npos || lowerMessage.find("null value") != std::string::npos) {
        return SqlError::NotNullViolation;
    }

    if (lowerMessage.find("foreign key") != std::string::npos ||
        lowerMessage.find("referential integrity") != std::string::npos) {
        return SqlError::ForeignKeyViolation;
    }

    if (lowerMessage.find("check constraint") != std::string::npos) {
        return SqlError::CheckConstraintViolation;
    }

    if (lowerMessage.find("constraint") != std::string::npos) {
        return SqlError::ConstraintViolation;
    }

    return std::nullopt;
}

SqlError::Code SqlTagsErrorHandler::determineErrorCodeFromTags(const SqlTags &tags) {
    // Determine the most likely error type based on active constraints
    // Priority: Primary Key > Unique > Not Null > Generic Constraint

    if (tags.primary_key) {
        return SqlError::PrimaryKeyViolation;
    }

    if (tags.unique) {
        return SqlError::UniqueConstraintViolation;
    }

    if (tags.not_null) {
        return SqlError::NotNullViolation;
    }

    // If any constraint is active, return generic constraint violation
    if (tags.auto_increment || tags.index) {
        return SqlError::ConstraintViolation;
    }

    // Default to unknown error if no constraints are active
    return SqlError::UnknownError;
}

} // namespace detail
ILIAS_SQL_NS_END