#include "ilias/sqlite/sqlite.hpp"

#include "ilias/sql/sqlerror.hpp"
#include "ilias/sqlite/sqliteopt.hpp"
#include "ilias/sql/sql_plugin.hpp"

ILIAS_SQLITE_NS_BEGIN

SqlParserResult sqlite_parse_null(const SqlCellView &cell) {
    if (cell.is_null()) {
        return std::any(g_sql_null);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_NULL) {
            return std::any(g_sql_null);
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief (兜底) 将任何 SQLite 单元格的值解析为 std::string。
 *
 * @param cell 单元格视图。
 * @return 包含 std::string 的 Result，如果单元格为 NULL 则返回错误。
 */
SqlParserResult sqlite_parse_string(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        switch (type) {
            case SQLITE_TEXT:
                return std::any(SqlText(reinterpret_cast<const char *>(sqlite3_value_text(value))));
            case SQLITE_INTEGER:
                return std::any((SqlText)std::to_string(sqlite3_value_int64(value)));
            case SQLITE_FLOAT:
                return std::any((SqlText)std::to_string(sqlite3_value_double(value)));
            case SQLITE_BLOB:
                return std::any(
                    SqlText(reinterpret_cast<const char *>(sqlite3_value_blob(value)), sqlite3_value_bytes(value)));
            case SQLITE_NULL:
                return Unexpected(SqlError::Code::NullValue);
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief (兜底) 将任何 SQLite 单元格的值解析为 std::string_view。
 *
 * @param cell 单元格视图。
 * @return 包含 std::string_view 的 Result，如果单元格为 NULL 则返回错误。
 */
SqlParserResult sqlite_parse_string_view(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        switch (type) {
            case SQLITE_TEXT:
                return std::any(SqlTextView(reinterpret_cast<const char *>(sqlite3_value_text(value))));
            case SQLITE_BLOB:
                return std::any(
                    SqlTextView(reinterpret_cast<const char *>(sqlite3_value_blob(value)), sqlite3_value_bytes(value)));
            case SQLITE_NULL:
                return Unexpected(SqlError::Code::NullValue);
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的文本表示解析为 int。
 *
 * @param cell 单元格视图，应为 kString 格式。
 * @return 包含 int 的 Result，如果格式错误或为 NULL 则返回错误。
 */
template <typename T>
    requires std::is_integral_v<T>
SqlParserResult sqlite_parse_int(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_INTEGER) {
            return std::any(static_cast<T>(sqlite3_value_int64(value)));
        }
        else if (type == SQLITE_NULL) {
            return Unexpected(SqlError::Code::NullValue);
        }
        else if (type == SQLITE_FLOAT) {
            return std::any(static_cast<T>(sqlite3_value_double(value)));
        }
        else if (type == SQLITE_TEXT) {
            auto text = reinterpret_cast<const char *>(sqlite3_value_text(value));
            auto len  = sqlite3_value_bytes(value);
            T    rvalue;
            auto ret = std::from_chars(text, text + len, rvalue);
            if (ret.ec == std::errc()) {
                return std::any(rvalue);
            }
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的文本表示解析为 double。
 *
 * @param cell 单元格视图，应为 kString 格式。
 * @return 包含 double 的 Result，如果格式错误或为 NULL 则返回错误。
 */
template <typename T>
    requires std::is_floating_point_v<T>
SqlParserResult sqlite_parse_real(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_FLOAT) {
            return std::any(static_cast<T>(sqlite3_value_double(value)));
        }
        else if (type == SQLITE_INTEGER) {
            return std::any(static_cast<T>(sqlite3_value_int64(value)));
        }
        else if (type == SQLITE_NULL) {
            return Unexpected(SqlError::Code::NullValue);
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的二进制数据解析为 BLOB (std::span<const std::byte>)。
 *
 * @param cell 单元格视图，应为 kBinary 格式。
 * @return 包含 std::span<const std::byte> 的 Result，如果为 NULL 或格式错误则返回错误。
 */
SqlParserResult sqlite_parse_blob(const SqlCellView &cell) {
    if (cell.is_null()) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kNativeValue) {
        auto value_any = std::any_cast<sqlite3_value *>(&cell.sql_value());
        if (value_any == nullptr) {
            return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
        }
        auto value = *value_any;
        auto type  = sqlite3_value_type(value);
        if (type == SQLITE_BLOB) {
            auto  blob_size = sqlite3_value_bytes(value);
            auto *blob_ptr  = reinterpret_cast<const std::byte *>(sqlite3_value_blob(value));
            return std::any(std::span<const std::byte>(blob_ptr, blob_size));
        }
        else if (type == SQLITE_NULL) {
            return Unexpected(SqlError::Code::NullValue);
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

/**
 * @brief 将 SQLite 单元格的文本表示解析为 bool。
 *        SQLite 中 bool 通常存储为整数 0 或 1。
 *
 * @param cell 单元格视图，应为 kString 格式。
 * @return 包含 bool 的 Result，如果格式错误或为 NULL 则返回错误。
 */
SqlParserResult sqlite_parse_bool(const SqlCellView &cell) {
    auto ret = sqlite_parse_int<int>(cell);
    if (!ret) {
        return Unexpected(ret.error());
    }
    return std::any(std::any_cast<int>(ret.value()) != 0);
}

SqlParserResult sqlite_parse_date(const SqlCellView &cell) {
    auto value_str = sqlite_parse_string(cell);
    if (!value_str) {
        return Unexpected(value_str.error());
    }
    auto any_string = std::any_cast<std::string>(&(*value_str));
    if (any_string == nullptr) {
        return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
    }
    SqlDate date;
    date.fromUTCString(*any_string);
    return std::any(date);
}

SqlBinderResult sqlite_bind_null(const SqlCellView &cell, std::any data) {
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Unexpected(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (int ret = sqlite3_bind_null(nativeSqliteStmt, cell.index()); ret != SQLITE_OK) {
        return Unexpected((SqlError::Code)ret);
    }
    return make_null_sql_binder_result();
}

SqlBinderResult sqlite_bind_string(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Unexpected(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const char))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    if (int ret = sqlite3_bind_text(nativeSqliteStmt, cell.index(), reinterpret_cast<const char *>(cell.raw_value()),
                                    cell.raw_value_size(), SQLITE_STATIC);
        ret != SQLITE_OK) {
        return Unexpected((SqlError::Code)ret);
    }
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_integral_v<T>
SqlBinderResult sqlite_bind_int(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Unexpected(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    if (int ret = sqlite3_bind_int64(nativeSqliteStmt, cell.index(), *reinterpret_cast<const T *>(cell.raw_value()));
        ret != SQLITE_OK) {
        return Unexpected((SqlError::Code)ret);
    }
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_floating_point_v<T>
SqlBinderResult sqlite_bind_real(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Unexpected(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    if (int ret = sqlite3_bind_double(nativeSqliteStmt, cell.index(), *reinterpret_cast<const T *>(cell.raw_value()));
        ret != SQLITE_OK) {
        return Unexpected((SqlError::Code)ret);
    }
    return make_null_sql_binder_result();
}

SqlBinderResult sqlite_bind_blob(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Unexpected(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const std::byte))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    if (int ret =
            sqlite3_bind_blob(nativeSqliteStmt, cell.index(), cell.raw_value(), cell.raw_value_size(), SQLITE_STATIC);
        ret != SQLITE_OK) {
        return Unexpected((SqlError::Code)ret);
    }
    return make_null_sql_binder_result();
}

SqlBinderResult sqlite_bind_date(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return sqlite_bind_null(cell, data);
    }
    auto sqliteStmt = std::any_cast<SqliteStatement *>(data);
    if (sqliteStmt == nullptr) {
        return Unexpected(SqlError::Code::InvalidSqlStatement);
    }
    auto nativeSqliteStmt = static_cast<sqlite3_stmt *>(sqliteStmt->nativeHandle());
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const SqlDate))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto date = reinterpret_cast<const SqlDate *>(cell.raw_value());
    if (date == nullptr || date->type == SqlDate::kErrorTime) {
        return sqlite_bind_null(cell, data);
    }
    auto dateStr = date->toUTCString();
    if (int ret = sqlite3_bind_text(nativeSqliteStmt, cell.index(), dateStr.c_str(), dateStr.size(), SQLITE_TRANSIENT);
        ret != SQLITE_OK) {
        return Unexpected((SqlError::Code)ret);
    }
    return make_null_sql_binder_result();
}

SqliteStmtResultSet::SqliteStmtResultSet(std::shared_ptr<sqlite3> sqlite, std::shared_ptr<sqlite3_stmt> stmt,
                                         std::shared_ptr<SqlValueConverterContext> context)
    : mSqlite(sqlite), mSqliteStmt(stmt), mContext(context) {
    auto column_count = sqlite3_column_count(mSqliteStmt.get());
    for (int i = 0; i < column_count; i++) {
        mIndexs.insert(std::make_pair(sqlite3_column_name(mSqliteStmt.get(), i), i));
    }
}

auto SqliteStmtResultSet::nativeHandle() const -> void * {
    return mSqliteStmt.get();
}

SqliteStmtResultSet::~SqliteStmtResultSet() {
    mSqliteStmt.reset();
}

auto SqliteStmtResultSet::next() -> IoTask<bool> {
    if (!mSqlite || !mSqliteStmt) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    ILIAS_TRACE("ilias-sqlite", "sqlite({}) Executing next/step", (void *)mSqlite.get());
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

auto SqliteStmtResultSet::getValue(size_t index) -> IoResult<SqlCellView> {
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NotConnected);
    }
    if (index >= columnCount()) {
        return Unexpected(std::make_error_code(std::errc::result_out_of_range));
    }
    auto *value = sqlite3_column_value(mSqliteStmt.get(), static_cast<int>(index));
    return SqlCellView(mContext, value, index);
}

auto SqliteStmtResultSet::getValue(std::string_view name) -> IoResult<SqlCellView> {
    auto index = mIndexs.find(std::string(name));
    if (index != mIndexs.end()) {
        return getValue(index->second);
    }
    return Unexpected(std::make_error_code(std::errc::invalid_argument));
}

auto SqliteStmtResultSet::setPrivate(std::unique_ptr<SqliteStatement> mp) {
    mPrivate = std::move(mp);
}

SqliteStatement::SqliteStatement(std::shared_ptr<sqlite3> sqlite, std::shared_ptr<SqlValueConverterContext> context)
    : mSqlite(sqlite), mContext(context) {
}

SqliteStatement::~SqliteStatement() {
    // reset(); // 如果resultset没有关闭，reset会导致resultset失效。
    mSqliteStmt.reset();
}

auto SqliteStatement::native() const -> sqlite3_stmt * {
    return mSqliteStmt.get();
}

auto SqliteStatement::bind(std::type_index type_index, size_t index, const SqlCellView &value) -> IoResult<void> {
    auto ctxt = mContext.get();
    if (value.context() != nullptr) {
        ctxt = value.context();
    }
    auto binder = ctxt->findTypeBinder(type_index);
    if (binder) {
        auto store =
            binder(SqlCellView(nullptr, value.raw_value(), value.raw_value_size(), value.raw_type(), index), this);
        if (store) {
            if (*store) {
                mDataGuards.emplace_back(std::move(store.value()));
            }
        }
        else {
            return Unexpected(store.error());
        }
        return {};
    }
    ILIAS_ERROR("ilias-sqlite", "Unsupport bind type: {}", type_index);
    return Unexpected(SqlError::Code::UnsupportBindType);
}

auto SqliteStatement::bind(std::type_index type_index, std::string_view name, const SqlCellView &value)
    -> Result<void, std::error_code> {
    if (!mSqlite || !mSqliteStmt) {
        return Unexpected(SqlError::Code::NotConnected);
    }
    std::string key   = ":" + std::string(name);
    auto        index = sqlite3_bind_parameter_index(mSqliteStmt.get(), key.c_str());
    return bind(type_index, index, value);
}

auto SqliteStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    if (!mSqlite || !mSqliteStmt) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    co_return std::make_unique<SqliteStmtResultSet>(mSqlite, mSqliteStmt, mContext);
}

auto SqliteStatement::nativeHandle() const -> void * {
    return mSqliteStmt.get();
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
    mContext = std::make_unique<SqlValueConverterContext>();
    mContext->registerType<SqlNull>(sqlite_parse_null);
    mContext->registerType<SqlNull>(sqlite_bind_null);
    mContext->registerType<SqlBool>(sqlite_parse_bool);
    mContext->registerType<SqlBool>(sqlite_bind_int<SqlBool>);
    mContext->registerType<SqlTinyInt>(sqlite_parse_int<SqlTinyInt>);
    mContext->registerType<SqlTinyInt>(sqlite_bind_int<SqlTinyInt>);
    mContext->registerType<SqlInt>(sqlite_parse_int<SqlInt>);
    mContext->registerType<SqlInt>(sqlite_bind_int<SqlInt>);
    mContext->registerType<SqlBigInt>(sqlite_parse_int<SqlBigInt>);
    mContext->registerType<SqlBigInt>(sqlite_bind_int<SqlBigInt>);
    mContext->registerType<SqlFloat>(sqlite_parse_real<SqlFloat>);
    mContext->registerType<SqlFloat>(sqlite_bind_real<SqlFloat>);
    mContext->registerType<double>(sqlite_parse_real<double>);
    mContext->registerType<double>(sqlite_bind_real<double>);
    mContext->registerType<SqlText>(sqlite_parse_string);
    mContext->registerType<const char>(sqlite_bind_string);
    mContext->registerType<SqlTextView>(sqlite_parse_string_view);
    mContext->registerType<SqlBlob>(sqlite_parse_blob);
    mContext->registerType<SqlBlob>(sqlite_bind_blob);
    mContext->registerType<SqlDate>(sqlite_parse_date);
    mContext->registerType<SqlDate>(sqlite_bind_date);
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

auto Sqlite::nativeHandle() const -> void * {
    return mSqlite.get();
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
    auto stmt = std::make_unique<SqliteStatement>(mSqlite, mContext);
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
    auto stmt = std::make_unique<SqliteStatement>(mSqlite, mContext);
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

auto Sqlite::valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> {
    return mContext;
}

ILIAS_SQLITE_NS_END

ILIAS_SQL_REGISTER_PLUGIN(sqlite) {
    return new ILIAS_SQLITE_COMPLETE_NAMESPACE::Sqlite(options);
}
