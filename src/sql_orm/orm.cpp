#include "ilias/sql_orm/detail/orm_condition.hpp"
#include "ilias/sql_orm/detail/orm_builder.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/enhanced_error.hpp"

ILIAS_SQL_NS_BEGIN

// ================= Enhanced Error Handling =================

namespace detail {

/**
 * @brief Enhance SqlError with SqlTags constraint context for better error reporting
 */
ILIAS_SQL_API
EnhancedSqlError enhanceErrorWithConstraintContext(const SqlError& error,
                                                  const std::string& tableName,
                                                  const std::string& columnName,
                                                  const SqlTags& tags) {
    return SqlTagsErrorHandler::enhanceError(error, tableName, columnName, tags);
}

/**
 * @brief Enhance SqlError with multiple constraint contexts
 */
ILIAS_SQL_API
EnhancedSqlError enhanceErrorWithMultipleConstraints(const SqlError& error,
                                                    const std::string& tableName,
                                                    const std::vector<std::pair<std::string, SqlTags>>& contexts) {
    return SqlTagsErrorHandler::enhanceError(error, tableName, contexts);
}

/**
 * @brief Try to enhance a generic SqlError by analyzing its message and mapping to constraint types
 */
ILIAS_SQL_API
EnhancedSqlError tryEnhanceGenericError(const SqlError& error,
                                       const std::string& tableName) {
    // Try to map the error message to a more specific constraint type
    auto mappedCode = SqlTagsErrorHandler::mapDatabaseErrorToConstraintType(error.message());
    
    if (mappedCode.has_value()) {
        // Create a new SqlError with the mapped code but keep the original message
        SqlError enhancedBaseError(mappedCode.value(), error.message());
        return EnhancedSqlError(enhancedBaseError, tableName, "", SqlTags{});
    }
    
    // If we can't map it, just wrap the original error
    return EnhancedSqlError(error, tableName, "", SqlTags{});
}

} // namespace detail

// ================= SqlTags Validation Methods =================

bool SqlTags::hasTimestampBehavior() const {
    return created_at || updated_at;
}

bool SqlTags::requiresIndex() const {
    return index || primary_key || unique;
}

// ================= Utils =================
ILIAS_SQL_API
std::string detail::join_strs(const std::vector<std::string> &vec, const std::string &sep, const std::string &prefix,
                              const std::string &suffix) {
    std::string res;
    for (size_t i = 0; i < vec.size(); ++i) {
        if (i > 0)
            res += sep;
        res += prefix + vec[i] + suffix;
    }
    return res;
}

// ================= SqlCondition =================
namespace detail {

SqlCondition::SqlCondition(std::string sql, std::vector<std::shared_ptr<SqlStatementBinder>> binders)
    : mSql(std::move(sql)), mBinders(std::move(binders)) {
}

auto SqlCondition::invalid(std::string diagnostic) -> SqlCondition {
    SqlCondition condition;
    condition.mDiagnostic = std::move(diagnostic);
    return condition;
}

SqlCondition SqlCondition::operator&&(const SqlCondition &other) const {
    if (!isValid())
        return *this;
    if (!other.isValid())
        return other;
    if (mSql.empty())
        return other;
    if (other.mSql.empty())
        return *this;
    auto newBinders = mBinders;
    newBinders.insert(newBinders.end(), other.mBinders.begin(), other.mBinders.end());
    return SqlCondition("(" + mSql + " AND " + other.mSql + ")", std::move(newBinders));
}

SqlCondition SqlCondition::operator||(const SqlCondition &other) const {
    if (!isValid())
        return *this;
    if (!other.isValid())
        return other;
    if (mSql.empty())
        return other;
    if (other.mSql.empty())
        return *this;
    auto newBinders = mBinders;
    newBinders.insert(newBinders.end(), other.mBinders.begin(), other.mBinders.end());
    return SqlCondition("(" + mSql + " OR " + other.mSql + ")", std::move(newBinders));
}

SqlCondition SqlCondition::operator!() const {
    if (!isValid())
        return *this;
    if (mSql.empty())
        return *this;
    return SqlCondition("NOT (" + mSql + ")", mBinders);
}

const std::string &SqlCondition::sql() const {
    return mSql;
}

int SqlCondition::bindTo(SqlStatement<void> &stmt, int startIndex) const {
    int index = startIndex;
    for (const auto &binder : mBinders) {
        binder->bind(index++, stmt);
    }
    return index;
}

bool SqlCondition::empty() const {
    return mSql.empty();
}

SqlVariable::SqlVariable(std::string_view name) : mSql(name), mBindName(name) {
}

SqlVariable::SqlVariable(std::string sql, std::string bindName) : mSql(std::move(sql)), mBindName(std::move(bindName)) {
}

SqlVariable::SqlVariable(std::string sql, std::string bindName, std::string diagnostic)
    : mSql(std::move(sql)), mBindName(std::move(bindName)), mDiagnostic(std::move(diagnostic)) {
}

auto SqlVariable::invalid(std::string diagnostic) -> SqlVariable {
    return SqlVariable("", "", std::move(diagnostic));
}

// ================= SelectBuilder =================

SelectBuilder::SelectBuilder(SqlDatabase &db, std::string tableName, const std::vector<std::string> &cols,
                             IdentifierQuoter quoteIdentifier, std::vector<std::string> diagnostics)
    : mDb(db), mTableName(std::move(tableName)), mQuoteIdentifier(quoteIdentifier),
      mDiagnostics(std::move(diagnostics)) {
    if (cols.empty())
        mSelectColumns = "*";
    else
        mSelectColumns = detail::join_strs(cols, ", ");
}

SelectBuilder &SelectBuilder::where(const SqlCondition &cond) {
    mWhereCondition = mWhereCondition && cond;
    return *this;
}

SelectBuilder &SelectBuilder::orderBy(std::string_view column, bool desc) {
    if (!mQuoteIdentifier) {
        throw std::invalid_argument("SelectBuilder has no SQL identifier quoter");
    }
    mOrderBy = " ORDER BY " + mQuoteIdentifier(column) + (desc ? " DESC" : " ASC");
    return *this;
}

void SelectBuilder::addDiagnostic(std::string diagnostic) {
    if (!diagnostic.empty()) {
        mDiagnostics.push_back(std::move(diagnostic));
    }
}

SelectBuilder &SelectBuilder::limit(int limit) {
    mLimit = " LIMIT " + std::to_string(limit);
    return *this;
}

SelectBuilder &SelectBuilder::offset(int offset) {
    mOffset = " OFFSET " + std::to_string(offset);
    return *this;
}

IoTask<SqlResult<void>> SelectBuilder::query() const {
    auto ret = co_await prepare();
    if (!ret)
        co_return Err(ret.error());

    bind(ret.value());
    auto res = co_await ret->query();
    if (!res) {
        co_return Err(res.error());
    }
    storage(res.value());
    co_return res;
}

IoTask<SqlStatement<void>> SelectBuilder::prepare() const {
    auto diag = diagnostic();
    if (!diag.empty()) {
        ILIAS_ERROR("ilias-sql", "Invalid ORM SQL expression: {}", diag);
        co_return Err(SqlError::Code::InvalidParameter);
    }

    std::string sql = "SELECT " + mSelectColumns + " FROM " + mTableName;
    if (!mWhereCondition.empty()) {
        sql += " WHERE " + mWhereCondition.sql();
    }
    sql += mOrderBy + mLimit + mOffset;

    co_return co_await mDb.prepare(sql);
}

void SelectBuilder::bind(SqlStatement<void> &stmt) const {
    mWhereCondition.bindTo(stmt);
}

void SelectBuilder::storage(SqlResult<void> &res) const {
    for (auto &binder : mWhereCondition.binds()) {
        res.storage(binder);
    }
}

auto SelectBuilder::diagnostic() const -> std::string {
    std::vector<std::string> diagnostics = mDiagnostics;
    if (!mWhereCondition.isValid()) {
        diagnostics.push_back(mWhereCondition.diagnostic());
    }
    return detail::join_strs(diagnostics, "; ");
}

IoGenerator<SqlResult<void>> SelectBuilder::loop(int count) {
    return queryLoopWrap(std::move(*this), count);
}

} // namespace detail

// 用户字面量实现
detail::SqlVariable operator""_sql(const char *str, size_t len) {
    return detail::SqlVariable(std::string_view(str, len));
}

ILIAS_SQL_NS_END
