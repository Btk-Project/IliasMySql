#include "ilias/sqlite/sqlite.hpp"

#include "ilias/sql/sqlerror.hpp"
#include "ilias/sqlite/sqliteopt.hpp"
#include "ilias/sql/sql_plugin.hpp"

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
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    // ILIAS_TRACE("ilias-sqlite", "sqlite({}) Executing next", (void *)mSqlite.get());
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
    return sqlite3_column_name(mSqliteStmt.get(), static_cast<int>(index));
}

auto SqliteStmtResultSet::getValue(size_t index) -> IoResult<SqlValueView> {
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NotConnected);
    }
    if (index >= columnCount()) {
        return Unexpected(std::make_error_code(std::errc::result_out_of_range));
    }
    int          type_code = sqlite3_column_type(mSqliteStmt.get(), static_cast<int>(index));
    SqlValueView value;
    value.emplace<SqlNull>();
    switch (type_code) {
        case SQLITE_INTEGER:
            value.emplace<int64_t>(sqlite3_column_int64(mSqliteStmt.get(), static_cast<int>(index)));
            break;
        case SQLITE_FLOAT:
            value.emplace<double>(sqlite3_column_double(mSqliteStmt.get(), static_cast<int>(index)));
            break;
        case SQLITE_TEXT:
            value.emplace<std::string_view>(
                reinterpret_cast<const char *>(sqlite3_column_text(mSqliteStmt.get(), static_cast<int>(index))));
            break;
        case SQLITE_BLOB:
            const void *blob_ptr   = sqlite3_column_blob(mSqliteStmt.get(), static_cast<int>(index));
            int         blob_bytes = sqlite3_column_bytes(mSqliteStmt.get(), static_cast<int>(index));
            value.emplace<std::span<const std::byte>>(reinterpret_cast<const std::byte *>(blob_ptr), blob_bytes);
    }
    return value;
}

auto SqliteStmtResultSet::getValue(std::string_view name) -> IoResult<SqlValueView> {
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
    // reset(); // 如果resultset没有关闭，reset会导致resultset失效。
    mSqliteStmt.reset();
}

auto SqliteStatement::bind(size_t index, SqlValuePointer value) -> Result<void, std::error_code> {
    ILIAS_TRACE("ilias-sqlite", "sqlite({}) Binding index {} value {}", (void *)mSqlite.get(), index, value);
    if (!mSqlite || !mSqliteStmt) {
        ILIAS_TRACE("ilias-sqlite", "{} bind failed: not connected", index);
        return Unexpected(SqlError::Code::NotConnected);
    }
    switch ((SqlValueType)value.index()) {
        case SqlValueType::kNull:
            sqlite3_bind_null(mSqliteStmt.get(), static_cast<int>(index));
            break;
        case SqlValueType::kBool:
            sqlite3_bind_int(mSqliteStmt.get(), static_cast<int>(index), get<SqlValueType::kBool>(value));
            break;
        case SqlValueType::kChar:
            sqlite3_bind_int(mSqliteStmt.get(), static_cast<int>(index), get<SqlValueType::kChar>(value));
            break;
        case SqlValueType::kInt:
            sqlite3_bind_int(mSqliteStmt.get(), static_cast<int>(index), get<SqlValueType::kInt>(value));
            break;
        case SqlValueType::kBigInt:
            sqlite3_bind_int64(mSqliteStmt.get(), static_cast<int>(index), get<SqlValueType::kBigInt>(value));
            break;
        case SqlValueType::kFloat:
            sqlite3_bind_double(mSqliteStmt.get(), static_cast<int>(index), get<SqlValueType::kFloat>(value));
            break;
        case SqlValueType::kDouble:
            sqlite3_bind_double(mSqliteStmt.get(), static_cast<int>(index), get<SqlValueType::kDouble>(value));
            break;
        case SqlValueType::kText: {
            auto string = get<SqlValueType::kText>(value);
            sqlite3_bind_text(mSqliteStmt.get(), static_cast<int>(index), string.data(),
                              static_cast<int>(string.size()), SQLITE_STATIC);
        } break;
        case SqlValueType::kBlob: {
            auto data = get<SqlValueType::kBlob>(value);
            sqlite3_bind_blob(mSqliteStmt.get(), static_cast<int>(index), data.data(),
                              static_cast<int>(data.size_bytes()), SQLITE_STATIC);
        } break;
        case SqlValueType::kDate: {
            auto string = get<SqlValueType::kDate>(value).toUTCString();
            sqlite3_bind_text(mSqliteStmt.get(), static_cast<int>(index), string.data(),
                              static_cast<int>(string.size()), SQLITE_TRANSIENT);
        } break;
        default:
            ILIAS_TRACE("ilias-sqlite", "{} bind failed: invalid argument", index);
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return {};
}

auto SqliteStatement::bind(std::string_view name, SqlValuePointer value) -> Result<void, std::error_code> {
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NotConnected);
    }
    std::string key   = ":" + std::string(name);
    auto        index = sqlite3_bind_parameter_index(mSqliteStmt.get(), key.c_str());
    return bind(index, value);
}

auto SqliteStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    if (!mSqlite || !mSqliteStmt) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    co_return std::make_unique<SqliteStmtResultSet>(mSqlite, mSqliteStmt);
}

auto SqliteStatement::execute() -> IoTask<size_t> {
    auto ret = co_await query();
    if (ret) {
        auto doret = co_await ret.value()->next();
        if (!doret) {
            co_return Unexpected(doret.error());
        }
        auto num = sqlite3_changes(mSqlite.get());
        num      = num < 0 ? 0 : num;
        co_return num;
    }
    co_return Unexpected(ret.error());
}

auto SqliteStatement::reset() -> void {
    ILIAS_TRACE("ilias-sqlite", "sqlite({}) Executing reset", (void *)mSqlite.get());
    if (!mSqlite || !mSqliteStmt) {
        return;
    }
    sqlite3_reset(mSqliteStmt.get());
    sqlite3_clear_bindings(mSqliteStmt.get());
}

auto SqliteStatement::prepare(std::string_view sql) -> IoTask<void> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    auto ret = co_await blocking([this, sql = sql]() -> int {
        sqlite3_stmt *stmt;
        auto          ret = sqlite3_prepare_v2(mSqlite.get(), sql.data(), static_cast<int>(sql.size()), &stmt, nullptr);
        ILIAS_TRACE("ilias-sqlite", "sqlite({}) Executing prepare: {}", (void *)mSqlite.get(), sql);
        if (ret != SQLITE_OK) {
            return ret;
        }
        mSqliteStmt = std::shared_ptr<sqlite3_stmt>(stmt, [](sqlite3_stmt *stmt) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_finalize(stmt);
        });
        return ret;
    });
    if (ret != SQLITE_OK) {
        SqlErrorCategory::instance().registerMessage(ret, sqlite3_errmsg(mSqlite.get()));
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
        co_return Unexpected(SqlError::Code::NotConnected);
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
        co_return Unexpected(SqlError::Code::AlreadyConnected);
    }
    auto ret = co_await blocking([this]() -> int {
        auto filename = mOptions.filename;
        if (filename.empty()) {
            filename = ":memory:";
        }
        int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
        if (auto it = mOptions.extra.find("flags"); it != mOptions.extra.end()) {
            int userFlags = sqlopt::parseOpenFlags(it->second);
            if (userFlags != 0) {
                flags = userFlags;
            }
        }
        const char *vfsName = nullptr;
        if (auto it = mOptions.extra.find("vfs"); it != mOptions.extra.end()) {
            vfsName = it->second.c_str();
        }
        sqlite3 *sql;
        auto     ret = sqlite3_open_v2(filename.c_str(), &sql, flags, vfsName);
        if (ret != SQLITE_OK) {
            if (sql) {
                sqlite3_close(sql);
            }
            return ret;
        }
#if defined(ENABLE_SQLCIPHER_PLUGINS)
        if (auto it = mOptions.extra.find("key"); it != mOptions.extra.end()) {
            auto cipher = it->second;
            ret         = sqlite3_key(sql, cipher.c_str(), cipher.size());
            if (ret != SQLITE_OK) {
                sqlite3_close(sql);
                return ret;
            }
        }

        if (auto it = mOptions.extra.find("rekey"); it != mOptions.extra.end()) {
            auto cipher_pass = it->second;
            ret              = sqlite3_rekey(sql, cipher_pass.c_str(), cipher_pass.size());
            if (ret != SQLITE_OK) {
                sqlite3_close(sql);
                return ret;
            }
        }
#endif
        for (const auto &[key, value] : mOptions.extra) {
#if defined(ENABLE_SQLCIPHER_PLUGINS)
            if (key == "key" || key == "rekey")
                continue;
#endif
            // 跳过已处理的特殊 key
            if (key == "flags" || key == "vfs")
                continue;

            // 查找对应的 Enum ID
            int optEnum = sqlopt::detail::getMySqlOptEnum(key);
            if (optEnum != -1) {
                // 创建并执行 Option
                // 注意：createOption 返回 raw pointer，需要手动管理生命周期
                std::unique_ptr<sqlopt::OptionBase> opt(sqlopt::createOption(optEnum, value));
                if (opt) {
                    int optRet = opt->setopt(*sql);
                    if (optRet != SQLITE_OK) {
                        ILIAS_WARN("sql", "Failed to set option {}: {}", key, optRet);
                    }
                }
            }
        }
        mSqlite = std::shared_ptr<sqlite3>(sql, [](sqlite3 *sql) { sqlite3_close(sql); });
        return ret;
    });
    if (ret != SQLITE_OK) {
        const char *errMsg = mSqlite ? sqlite3_errmsg(mSqlite.get()) : sqlite3_errstr(ret);
        SqlErrorCategory::instance().registerMessage(ret, errMsg);
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Sqlite::disconnect() -> IoTask<void> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NotConnected);
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
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    co_return Unexpected(SqlError::Code::UnsupportedApi);
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
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    auto stmt = std::make_unique<SqliteStatement>(mSqlite);
    auto ret  = co_await stmt->prepare(sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto ret1 = co_await stmt->query();
    if (!ret1) {
        co_return Unexpected(ret1.error());
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
    auto doret = co_await ret.value()->next();
    if (!doret) {
        co_return Unexpected(doret.error());
    }
    auto num = sqlite3_changes(mSqlite.get());
    num      = num < 0 ? 0 : num;
    co_return num;
}

auto Sqlite::beginTransaction() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    char *err;
    auto  ret = co_await blocking([this, &err]() -> int {
        auto ret = sqlite3_exec(mSqlite.get(), "BEGIN", nullptr, nullptr, &err);
        ILIAS_TRACE("ilias-sqlite", "sqlite({}) begin transaction ret={}", (void *)mSqlite.get(), ret);
        return ret;
    });
    if (ret != SQLITE_OK) {
        SqlErrorCategory::instance().registerMessage(ret, err);
        sqlite3_free(err); // 记得释放错误消息内存
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return true;
}

auto Sqlite::commit() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    char *err;
    auto  ret = co_await blocking([this, &err]() -> int {
        ILIAS_TRACE("ilias-sqlite", "commit transaction");
        return sqlite3_exec(mSqlite.get(), "COMMIT", nullptr, nullptr, &err);
    });
    if (ret != SQLITE_OK) {
        SqlErrorCategory::instance().registerMessage(ret, err);
        sqlite3_free(err); // 记得释放错误消息内存
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return true;
}

auto Sqlite::rollback() -> IoTask<bool> {
    if (!mSqlite) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    char *err;
    auto  ret = co_await blocking([this, &err]() -> int {
        auto ret = sqlite3_exec(mSqlite.get(), "ROLLBACK", nullptr, nullptr, &err);
        ILIAS_TRACE("ilias-sqlite", "sqlite({}) rollback transaction ret={}", (void *)mSqlite.get(), ret);
        return ret;
    });
    if (ret != SQLITE_OK) {
        SqlErrorCategory::instance().registerMessage(ret, err);
        sqlite3_free(err); // 记得释放错误消息内存
        co_return Unexpected((SqlError::Code)ret);
    }
    co_return true;
}

auto Sqlite::syncRollback() -> bool {
    if (!mSqlite) {
        return false;
    }
    char *err;
    int   rc = sqlite3_exec(mSqlite.get(), "ROLLBACK", nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        ILIAS_WARN("ilias-sqlite", "sqlite({}) rollback transaction ret={}", (void *)mSqlite.get(), rc);
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
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    co_return true;
}

ILIAS_SQLITE_NS_END

ILIAS_SQL_REGISTER_PLUGIN(sqlite) {
    return new ILIAS_SQLITE_COMPLETE_NAMESPACE::Sqlite(options);
}
