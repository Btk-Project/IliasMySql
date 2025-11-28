#include "ilias/sqlite/sqlite.hpp"

#include "ilias/sql/sqlerror.hpp"

ILIAS_SQLITE_NS_BEGIN

SqliteStmtResultSet::SqliteStmtResultSet(std::shared_ptr<sqlite3> sqlite, std::shared_ptr<sqlite3_stmt> stmt)
    : mSqlite(sqlite), mSqliteStmt(stmt) {
    auto column_count = sqlite3_column_count(mSqliteStmt.get());
    for (int i = 0; i < column_count; i++) {
        mIndexs.insert(std::make_pair(sqlite3_column_name(mSqliteStmt.get(), i), i));
    }
}

SqliteStmtResultSet::~SqliteStmtResultSet() {
    mSqliteStmt.reset();
}

auto SqliteStmtResultSet::next() -> IoTask<bool> {
    if (!mSqlite || !mSqliteStmt) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    if (mIsFirst) {
        mIsFirst = false;
        co_return true;
    }
    auto ret = co_await blocking([this]() -> int { return sqlite3_step(mSqliteStmt.get()); });
    if (ret == SQLITE_DONE) {
        co_return false;
    }
    if (ret != SQLITE_ROW) {
        SqlErrorCategory::instance().registerMessage(ret, sqlite3_errmsg(mSqlite.get()));
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return true;
}

auto SqliteStmtResultSet::rowCount() const -> size_t {
    return 0;
}

auto SqliteStmtResultSet::columnCount() const -> size_t {
    if (!mSqlite || !mSqliteStmt) {
        return 0;
    }
    return sqlite3_column_count(mSqliteStmt.get());
}

auto SqliteStmtResultSet::columnName(size_t index) const -> std::string_view {
    if (!mSqlite || !mSqliteStmt) {
        return "";
    }
    return sqlite3_column_name(mSqliteStmt.get(), index);
}

auto SqliteStmtResultSet::getValue(size_t index) -> IoResult<SqlValue> {
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    if (index >= columnCount()) {
        return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    int      type_code = sqlite3_column_type(mSqliteStmt.get(), index);
    SqlValue value;
    value.emplace<SqlNull>();
    switch (type_code) {
        case SQLITE_INTEGER:
            value.emplace<int64_t>(sqlite3_column_int64(mSqliteStmt.get(), index));
            break;
        case SQLITE_FLOAT:
            value.emplace<double>(sqlite3_column_double(mSqliteStmt.get(), index));
            break;
        case SQLITE_TEXT:
            value.emplace<std::string>(reinterpret_cast<const char *>(sqlite3_column_text(mSqliteStmt.get(), index)));
            break;
        case SQLITE_BLOB:
            const void *blob_ptr   = sqlite3_column_blob(mSqliteStmt.get(), index);
            int         blob_bytes = sqlite3_column_bytes(mSqliteStmt.get(), index);
            value.emplace<std::vector<std::byte>>(reinterpret_cast<const std::byte *>(blob_ptr),
                                                  reinterpret_cast<const std::byte *>(blob_ptr) + blob_bytes);
    }
    return value;
}

auto SqliteStmtResultSet::getValue(std::string_view name) -> IoResult<SqlValue> {
    auto index = mIndexs.find(std::string(name));
    if (index != mIndexs.end()) {
        return getValue(index->second);
    }
    return Unexpected(std::make_error_code(std::errc::invalid_argument));
}

auto SqliteStmtResultSet::setPrivate(std::unique_ptr<SqliteStatement> mp) {
    mPrivate = std::move(mp);
}

SqliteStatement::SqliteStatement(std::shared_ptr<sqlite3> sqlite) : mSqlite(sqlite) {
}

SqliteStatement::~SqliteStatement() {
    reset();
    mSqliteStmt.reset();
}

auto SqliteStatement::bind(size_t index, SqlValueView value) -> Result<void, std::error_code> {
    ILIAS_INFO("ilias-sqlite", "bind index: {}, value: {}", index, value);
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    switch ((SqlValueType)value.index()) {
        case SqlValueType::kNull:
            sqlite3_bind_null(mSqliteStmt.get(), index);
            break;
        case SqlValueType::kChar:
            sqlite3_bind_int(mSqliteStmt.get(), index, get<SqlValueType::kChar>(value));
            break;
        case SqlValueType::kInt:
            sqlite3_bind_int(mSqliteStmt.get(), index, get<SqlValueType::kInt>(value));
            break;
        case SqlValueType::kBigInt:
            sqlite3_bind_int64(mSqliteStmt.get(), index, get<SqlValueType::kBigInt>(value));
            break;
        case SqlValueType::kFloat:
            sqlite3_bind_double(mSqliteStmt.get(), index, get<SqlValueType::kFloat>(value));
            break;
        case SqlValueType::kDouble:
            sqlite3_bind_double(mSqliteStmt.get(), index, get<SqlValueType::kDouble>(value));
            break;
        case SqlValueType::kText: {
            auto string = get<SqlValueType::kText>(value);
            sqlite3_bind_text(mSqliteStmt.get(), index, string.data(), string.size(), SQLITE_STATIC);
        } break;
        case SqlValueType::kBlob: {
            auto data = get<SqlValueType::kBlob>(value);
            sqlite3_bind_blob(mSqliteStmt.get(), index, data.data(), data.size_bytes(), SQLITE_STATIC);
        } break;
        case SqlValueType::kDate: {
            auto string = get<SqlValueType::kDate>(value).toString();
            sqlite3_bind_text(mSqliteStmt.get(), index, string.data(), string.size(), SQLITE_TRANSIENT);
        } break;
        default:
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return {};
}

auto SqliteStatement::bind(std::string_view name, SqlValueView value) -> Result<void, std::error_code> {
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    std::string key   = ":" + std::string(name);
    auto        index = sqlite3_bind_parameter_index(mSqliteStmt.get(), key.c_str());
    return bind(index, value);
}

auto SqliteStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    if (!mSqlite || !mSqliteStmt) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    auto ret = co_await blocking([this]() -> int { return sqlite3_step(mSqliteStmt.get()); });
    if (ret != SQLITE_DONE && ret != SQLITE_ROW) {
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return std::make_unique<SqliteStmtResultSet>(mSqlite, mSqliteStmt);
}

auto SqliteStatement::execute() -> IoTask<size_t> {
    auto ret = co_await query();
    if (ret) {
        auto num = sqlite3_changes(mSqlite.get());
        num      = num < 0 ? 0 : num;
        co_return num;
    }
    co_return Unexpected(ret.error());
}

auto SqliteStatement::reset() -> void {
    if (!mSqlite || !mSqliteStmt) {
        return;
    }
    sqlite3_reset(mSqliteStmt.get());
    sqlite3_clear_bindings(mSqliteStmt.get());
}

auto SqliteStatement::prepare(std::string_view sql) -> IoTask<void> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    auto ret = co_await blocking([this, sql = sql]() -> int {
        sqlite3_stmt *stmt;
        auto          ret = sqlite3_prepare_v2(mSqlite.get(), sql.data(), sql.size(), &stmt, nullptr);
        if (ret != SQLITE_OK) {
            return ret;
        }
        mSqliteStmt = std::shared_ptr<sqlite3_stmt>(stmt, [](sqlite3_stmt *stmt) { sqlite3_finalize(stmt); });
        return ret;
    });
    if (ret != SQLITE_OK) {
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return {};
}

auto SqliteStatement::clearBinds() -> void {
    if (!mSqlite || !mSqliteStmt) {
        return;
    }
    sqlite3_clear_bindings(mSqliteStmt.get());
}

auto SqliteStatement::close() -> IoTask<void> {
    if (!mSqlite || !mSqliteStmt) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    auto ret    = co_await blocking([this]() -> int {
        sqlite3_reset(mSqliteStmt.get());
        sqlite3_clear_bindings(mSqliteStmt.get());
        mSqliteStmt.reset();
        return SQLITE_OK;
    });
    mSqliteStmt = nullptr;
    if (ret != SQLITE_OK) {
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return {};
}

Sqlite::Sqlite(const ConnectOptions &options) {
    mOptions = options;
}

Sqlite::~Sqlite() {
    mSqlite.reset();
}

auto Sqlite::native() -> sqlite3 * {
    return mSqlite.get();
}

auto Sqlite::sqlname() -> std::string {
    return "sqlite";
}

auto Sqlite::sqlinfo() -> std::string {
    return sqlite3_libversion();
}

auto Sqlite::connect() -> IoTask<void> {
    if (mSqlite) {
        co_return Unexpected(SqlError::Code::ALREADY_CONNECTED);
    }
    auto ret = co_await blocking([this]() -> int {
        auto filename = mOptions.filename;
        if (filename.empty()) {
            filename = ":memory:";
        }
        sqlite3 *sql;
        auto     ret = sqlite3_open(filename.c_str(), &sql);
        if (ret != SQLITE_OK) {
            return ret;
        }
        mSqlite = std::shared_ptr<sqlite3>(sql, [](sqlite3 *sql) { sqlite3_close(sql); });
        return ret;
    });
    if (ret != SQLITE_OK) {
        SqlErrorCategory::instance().registerMessage(ret, sqlite3_errmsg(mSqlite.get()));
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Sqlite::disconnect() -> IoTask<void> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    auto ret = co_await blocking([this]() -> int {
        mSqlite.reset();
        return SQLITE_OK;
    });
    if (ret != SQLITE_OK) {
        SqlErrorCategory::instance().registerMessage(ret, sqlite3_errmsg(mSqlite.get()));
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Sqlite::selectDatabase([[maybe_unused]] std::string_view name) -> IoTask<void> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    co_return Unexpected(SqlError::Code::UNSUPPORTED_API);
}

auto Sqlite::prepare(std::string_view sql) -> IoTask<std::unique_ptr<sql::IStatement>> {
    auto stmt = std::make_unique<SqliteStatement>(mSqlite);
    auto ret  = co_await stmt->prepare(sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return stmt;
}

auto Sqlite::query(std::string_view sql) -> IoTask<std::unique_ptr<sql::IResultSet>> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    auto stmt = std::make_unique<SqliteStatement>(mSqlite);
    auto ret  = co_await stmt->prepare(sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await stmt->query();
    if (!ret1) {
        co_return Unexpected(ret.error());
    }
    auto sqlresult = dynamic_cast<SqliteStmtResultSet *>(ret1.value().get());
    if (sqlresult) {
        sqlresult->setPrivate(std::move(stmt));
    }
    co_return std::move(ret1.value());
}

auto Sqlite::execute(std::string_view sql) -> IoTask<size_t> {
    auto ret = co_await query(sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto num = sqlite3_changes(mSqlite.get());
    num      = num < 0 ? 0 : num;
    co_return num;
}

auto Sqlite::beginTransaction() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    char *err;
    auto  ret = co_await blocking(
        [this, &err]() -> int { return sqlite3_exec(mSqlite.get(), "BEGIN", nullptr, nullptr, &err); });
    if (ret != SQLITE_OK) {
        sqlite3_free(err); // 记得释放错误消息内存
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return true;
}

auto Sqlite::commit() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    char *err;
    auto  ret = co_await blocking(
        [this, &err]() -> int { return sqlite3_exec(mSqlite.get(), "COMMIT", nullptr, nullptr, &err); });
    if (ret != SQLITE_OK) {
        sqlite3_free(err); // 记得释放错误消息内存
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return true;
}

auto Sqlite::rollback() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    char *err;
    auto  ret = co_await blocking(
        [this, &err]() -> int { return sqlite3_exec(mSqlite.get(), "ROLLBACK", nullptr, nullptr, &err); });
    if (ret != SQLITE_OK) {
        sqlite3_free(err); // 记得释放错误消息内存
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return true;
}

auto Sqlite::syncRollback() -> bool {
    char *err;
    int   rc = sqlite3_exec(mSqlite.get(), "ROLLBACK", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        sqlite3_free(err); // 记得释放错误消息内存
        return false;
    }
    return true;
}

auto Sqlite::lastInsertId() const -> int64_t {
    return sqlite3_last_insert_rowid(mSqlite.get());
}

auto Sqlite::ping() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NOT_CONNECTED);
    }
    co_return true;
}

void ilias_register_sql_plugin(DriverManager *manager) {
    manager->registerDriver("sqlite", [](const ConnectOptions &options) -> std::unique_ptr<IConnection> {
        auto connection = std::make_unique<Sqlite>(options);
        return connection;
    });
}

ILIAS_SQLITE_NS_END