#include "ilias/sql/driver_registry.hpp"

#include "ilias/mysql/mysql.hpp"
#include "ilias/mysql/mysqlresult.hpp"
#include "ilias/mysql/mysqlopt.hpp"
#include "ilias/sql/sql_plugin.hpp"
#include "ilias/mysql/mysql_parsers.hpp"
#include "ilias/sql/detail/placeholder_parser.hpp"

ILIAS_MYSQL_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

bool MySql::operator==(MySql &other) {
    return &mMysql == &other.mMysql;
}

MySql::~MySql() {
    close();
}

auto MySql::native() -> MYSQL * {
    return &mMysql;
}

auto MySql::pollStatus(int &status, uint32_t pollEvents) -> IoTask<void> {
    if (!mPoller) {
        co_return Unexpected(IoError::Unknown);
    }
    uint64_t timeOut = 0;
    if (status & MYSQL_WAIT_TIMEOUT) {
        timeOut = mysql_get_timeout_value_ms(&mMysql);
    }
    std::string events;
    if (pollEvents == 0) {
        if (status & MYSQL_WAIT_READ) {
            pollEvents |= POLLIN;
            events += "POLLIN ";
        }
        if (status & MYSQL_WAIT_WRITE) {
            pollEvents |= POLLOUT;
            events += "POLLOUT ";
        }
        if (status & MYSQL_WAIT_EXCEPT) {
            pollEvents |= POLLPRI;
            events += "POLLPRI";
        }
    }
    ILIAS_TRACE("ilias-mysql", "poll events: {}", events);
    IoResult<unsigned int> ret;
    if (timeOut == 0) {
        ret = co_await mPoller.poll(pollEvents);
        if (!ret) {
            ILIAS_ERROR("ilias-mysql", "poll failed, {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
    }
    else {
        auto pollRet = co_await (mPoller.poll(pollEvents) | timeout(std::chrono::milliseconds(timeOut)));
        if (!pollRet) {
            status = MYSQL_WAIT_TIMEOUT;
            co_return Unexpected(IoError::TimedOut);
        }
        if (!(*pollRet)) {
            if ((*pollRet).error() == IoError::TimedOut) {
                status = MYSQL_WAIT_TIMEOUT;
            }
            ILIAS_ERROR("ilias-mysql", "poll failed, no result in poll.");
            co_return Unexpected((*pollRet).error());
        }
        ret = std::move(*pollRet);
    }
    status = 0;
    if (ret.value_or(0) & POLLIN) {
        status |= MYSQL_WAIT_READ;
    }
    if (ret.value_or(0) & POLLOUT) {
        status |= MYSQL_WAIT_WRITE;
    }
    if (ret.value_or(0) & POLLPRI) {
        status |= MYSQL_WAIT_EXCEPT;
    }
    co_return {};
}

// please make poller when xxx_start() return not 0.
#define SQL_PRIVATE_MAKE_POLLER                                                                                        \
    {                                                                                                                  \
        if (mCtxt == nullptr) {                                                                                        \
            co_return Unexpected(IoError::Unknown);                                                                    \
        }                                                                                                              \
        auto fd = mysql_get_socket(&mMysql);                                                                           \
        if (fd == (decltype(fd))MARIADB_INVALID_SOCKET) {                                                              \
            ILIAS_ERROR("ilias-mysql", "get socket failed");                                                           \
            co_return Unexpected(IoError::Unknown);                                                                    \
        }                                                                                                              \
        if (!mPoller || (mPoller.fd() != (fd_t)fd)) {                                                                  \
            mPoller = (co_await Poller::make((fd_t)fd, IoDescriptor ::Socket)).value();                                \
            if (!mPoller) {                                                                                            \
                ILIAS_ERROR("ilias-mysql", "add fd({}) to IoContext failed.", fd);                                     \
                co_return Unexpected(IoError::Unknown);                                                                \
            }                                                                                                          \
        }                                                                                                              \
    }

#define SQL_PRIVATE_SYNC_CODE(OutP, MysqlFunc, ...)                                                                    \
    auto status = MysqlFunc##_start(&OutP, &mMysql, ##__VA_ARGS__);                                                    \
    if (status) {                                                                                                      \
        SQL_PRIVATE_MAKE_POLLER                                                                                        \
        while (status) {                                                                                               \
            ILIAS_TRACE("ilias-mysql", "{} waiting for status {}", #MysqlFunc, status);                                \
            auto pret = co_await pollStatus(status);                                                                   \
            status    = MysqlFunc##_cont(&OutP, &mMysql, status);                                                      \
            if (!pret) {                                                                                               \
                co_return Unexpected(pret.error());                                                                    \
            }                                                                                                          \
        }                                                                                                              \
    }                                                                                                                  \
    auto _check = [](auto p) {                                                                                         \
        if constexpr (std::is_pointer_v<decltype(p)>) {                                                                \
            if (p != nullptr)                                                                                          \
                return true;                                                                                           \
        }                                                                                                              \
        else if constexpr (std::is_same_v<decltype(p), my_bool>) {                                                     \
            if (p)                                                                                                     \
                return true;                                                                                           \
        }                                                                                                              \
        else if constexpr (std::is_integral_v<decltype(p)>) {                                                          \
            if (p == 0)                                                                                                \
                return true;                                                                                           \
        }                                                                                                              \
        else {                                                                                                         \
            ILIAS_ERROR("ilias-mysql", "unknow type?");                                                                \
            return false;                                                                                              \
        }                                                                                                              \
        return false;                                                                                                  \
    };                                                                                                                 \
    if (!_check(OutP)) {                                                                                               \
        auto errCode = mysql_errno(&mMysql);                                                                           \
        if (errCode != 0) {                                                                                            \
            [[maybe_unused]] auto error = mysql_error(&mMysql);                                                        \
            ILIAS_ERROR("ilias-mysql", "{} failed, error({}): {}", #MysqlFunc, errCode, error);                        \
            sql::SqlErrorCategory::instance().registerMessage(errCode, error);                                         \
            co_return Unexpected(sql::SqlError::Code(errCode));                                                        \
        }                                                                                                              \
    }

auto MySql::connect(std::string_view host, std::string_view user, std::string_view passwd, std::string_view db,
                    int port, std::string_view unix_socket, unsigned long client_flag) -> IoTask<void> {
    MYSQL      *ret;
    std::string targetHost(host);
    std::string targetUser(user);
    std::string targetPasswd(passwd);
    std::string targetDb(db);

    SQL_PRIVATE_SYNC_CODE(ret, mysql_real_connect, host == "" ? nullptr : targetHost.c_str(), targetUser.c_str(),
                          targetPasswd.c_str(), targetDb.c_str(), port,
                          unix_socket == "" ? nullptr : unix_socket.data(), client_flag)
    co_return {};
}

auto MySql::resetConnection() -> IoTask<int> {

    // this ret is what.
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_reset_connection);
    co_return {};
}

auto MySql::changeUser(std::string_view user, std::string_view passwd, std::string_view db) -> IoTask<bool> {

    // this ret is what.
    my_bool     ret;
    std::string localUser(user);
    std::string localPasswd(passwd);
    std::string localDb(db);
    SQL_PRIVATE_SYNC_CODE(ret, mysql_change_user, localUser.c_str(), localPasswd.c_str(), localDb.c_str())
    co_return {};
}

auto MySql::close() -> void {
    ILIAS_TRACE("ilias-mysql", "close mysql connection");
    mPoller.close();
    mysql_close(&mMysql);
}

auto MySql::dumpDebugInfo() -> IoTask<int> {
    // this ret is what.
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_dump_debug_info)
    co_return ret;
}

auto MySql::setServerOption(ServerOption option) -> IoTask<int> {
    // this ret is what.
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_set_server_option, static_cast<enum_mysql_set_option>(option))
    co_return ret;
}

auto MySql::setCharacterSet(std::string_view csname) -> IoTask<int> {
    // this ret is what.
    int         ret;
    std::string localCsname(csname);
    SQL_PRIVATE_SYNC_CODE(ret, mysql_set_character_set, localCsname.c_str())
    co_return ret;
}

auto MySql::selectDb(std::string_view db) -> IoTask<int> {
    // this ret is what.
    int         ret;
    std::string localDb(db);
    SQL_PRIVATE_SYNC_CODE(ret, mysql_select_db, localDb.c_str())
    co_return ret;
}

auto MySql::query(std::string_view sql) -> IoTask<int> {
    int ret;
    ILIAS_TRACE("ilias-mysql", "query: {}", sql);
    SQL_PRIVATE_SYNC_CODE(ret, mysql_real_query, sql.data(), (uint32_t)sql.size())
    // can use mysql_num_fields() to determine if a statement returned a result set.
    co_return ret;
}

auto MySql::commit() -> IoTask<bool> {
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_commit)
    co_return ret;
}

auto MySql::disconnect() -> IoTask<void> {
    mPoller.close();
    auto status = mysql_close_start(&mMysql);
    if (status) {
        SQL_PRIVATE_MAKE_POLLER
        while (status) {
            ILIAS_TRACE("ilias-mysql", "disconnect mysql waiting for status {}", status);
            auto pret = co_await pollStatus(status);
            status    = mysql_close_cont(&mMysql, status);
            if (!pret) {
                co_return Unexpected(pret.error());
            }
        }
    }
    ILIAS_TRACE("ilias-mysql", "close mysql connection");
    co_return {};
}

auto MySql::autoCommit(bool autoMode) -> IoTask<bool> {
    // TODO: need query "select @@autocommit;" and get the result.
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_autocommit, autoMode)
    co_return ret;
}

auto MySql::syncAutoCommit(bool autoMode) -> bool {
    return mysql_autocommit(&mMysql, autoMode);
}

auto MySql::nextResult() -> IoTask<int> {
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_next_result)
    co_return ret;
}

auto MySql::fieldCount() -> std::size_t {
    return mysql_field_count(&mMysql);
}

auto MySql::useResult() -> IoTask<MYSQL_RES *> {
    co_return mysql_use_result(&mMysql);
}

auto MySql::storeResult() -> IoTask<MYSQL_RES *> {
    MYSQL_RES *result;
    SQL_PRIVATE_SYNC_CODE(result, mysql_store_result)
    co_return result;
}

auto MySql::rollback() -> IoTask<bool> {
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_rollback)
    ILIAS_TRACE("ilias-mysql", "rollback result {}", static_cast<int>(ret));
    co_return ret;
}

auto MySql::syncRollback() -> bool {
    return mysql_rollback(&mMysql);
}

auto MySql::listFields(std::string_view table, std::string_view wildcard) -> IoTask<MYSQL_RES *> {
    MYSQL_RES  *ret;
    std::string localTable(table);
    std::string localWildcard(wildcard);
    SQL_PRIVATE_SYNC_CODE(ret, mysql_list_fields, localTable.c_str(), localWildcard.c_str());
    co_return ret;
}

auto MySql::sendQuery(std::string_view sql) -> IoTask<int> {
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_real_query, sql.data(), (uint32_t)sql.size())
    co_return ret;
}

auto MySql::refresh(uint32_t refreshOptions) -> IoTask<int> {
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_refresh, refreshOptions)
    co_return ret;
}

auto MySql::kill(uint64_t pid) -> IoTask<int> {
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_kill, pid)
    co_return ret;
}

auto MySql::ping() -> IoTask<int> {
    int ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_ping)
    co_return ret;
}

auto MySql::stat() -> IoTask<const char *> {
    const char *ret; // FIXME: this var's lifetime who knows ?
    SQL_PRIVATE_SYNC_CODE(ret, mysql_stat)
    co_return ret;
}

auto MySql::readQueryResult() -> IoTask<bool> {
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_read_query_result)
    co_return ret;
}

auto MySql::setOpt(sqlopt::OptionBase *opt) -> int {
    return opt->setopt(mMysql);
}

auto MySql::getOpt(sqlopt::OptionBase *opt) -> int {
    return opt->getopt(mMysql);
}

auto MySql::stmtInit() -> MYSQL_STMT * {
    return mysql_stmt_init(&mMysql);
}

auto MySql::lastError() -> int {
    return mysql_errno(&mMysql);
}

auto MySql::lastErrorMessage() -> const char * {
    return mysql_error(&mMysql);
}
auto MySql::info() -> std::string {
    return mysql_get_server_info(&mMysql);
}

auto MySql::valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> {
    return mContext;
}

#undef SQL_PRIVATE_MAKE_POLLER
#undef SQL_PRIVATE_SYNC_CODE

MysqlResultSet::~MysqlResultSet() {
}

MysqlResultSet::MysqlResultSet(std::unique_ptr<MySqlResultBase> &&imp) : mImp(std::move(imp)) {
}

auto MysqlResultSet::next() -> IoTask<bool> {
    return mImp->next();
}

auto MysqlResultSet::rowCount() const -> size_t {
    return mImp->countRows();
}

auto MysqlResultSet::columnCount() const -> size_t {
    return mImp->countFields();
}
auto MysqlResultSet::columnName(size_t index) const -> std::string_view {
    return mImp->fieldName(index);
}
auto MysqlResultSet::getValue(size_t index) -> IoResult<SqlCellView> {
    return mImp->get(index);
}
auto MysqlResultSet::getValue(std::string_view name) -> IoResult<SqlCellView> {
    return mImp->get(name);
}

auto MysqlResultSet::nativeHandle() const -> void * {
    return mImp->nativeResult();
}

auto MysqlResultSet::native() -> MYSQL_RES * {
    return mImp->nativeResult();
}

MysqlStatement::MysqlStatement(std::shared_ptr<MySql> mysql) {
    mMysql = std::move(mysql);
}

MysqlStatement::~MysqlStatement() {
}

auto MysqlStatement::bind(std::type_index type_index, size_t index, const SqlCellView &value)
    -> Result<void, std::error_code> {
    // ILIAS_TRACE("ilias-sql", "bind {} with {}", index, type_index);
    if (mMysqlStmt == nullptr) {
        return Unexpected(SqlError::NotPrepared);
    }
    if (index - 1 >= mBinds.size()) {
        return Unexpected(SqlError::InvalidIndex);
    }
    auto ctxt = mMysql->valueConverterContext().get();
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
    ILIAS_ERROR("ilias-mysql", "Unsupported bind type: {}", type_index);
    return Unexpected(SqlError::Code::UnsupportBindType);
}

auto MysqlStatement::bind(std::type_index type_index, std::string_view name, const SqlCellView &value)
    -> Result<void, std::error_code> {
    // ILIAS_INFO("ilias-mysql", "bind {} = {}", name, value);
    auto index = mIndexs.find(std::string(name));
    if (index == mIndexs.end()) {
        return Unexpected(SqlError::InvalidIndex);
    }
    return bind(type_index, index->second + 1, value);
}

auto MysqlStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    if (mMysqlStmt == nullptr) {
        co_return Unexpected(SqlError::Code::NotPrepared);
    }
    int ret = 0;
    if (mBinds.size() > 0) {
        ret = mysql_stmt_bind_param(mMysqlStmt.get(), mBinds.data());
    }
    if (ret != 0) {
        auto lastererror = mMysql->lastError();
        auto message     = mMysql->lastErrorMessage();
        ILIAS_ERROR("ilias-mysql", "stmt bind failed. (error {}:{})", ret, message);
        if (lastererror != 0) {
            sql::SqlErrorCategory::instance().registerMessage(lastererror, message);
            co_return Unexpected((SqlError::Code)lastererror);
        }
        else {
            co_return Unexpected((SqlError::Code)ret);
        }
    }
    auto status = mysql_stmt_execute_start(&ret, mMysqlStmt.get());
    while (status) {
        ILIAS_TRACE("ilias-mysql", "stmt execute waiting for status {}", status);
        auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
        if (!pret) {
            ILIAS_ERROR("ilias-mysql", "stmt execute failed. (error: {})", pret.error().message());
            co_return Unexpected(pret.error());
        }
        status = mysql_stmt_execute_cont(&ret, mMysqlStmt.get(), status);
    }
    if (ret != 0) {
        auto lastererror = mMysql->lastError();
        if (lastererror != 0) {
            auto message = mMysql->lastErrorMessage();
            // ILIAS_ERROR("ilias-mysql", "stmt execute failed. (error {}:{})", ret, mMysql->lastErrorMessage());
            sql::SqlErrorCategory::instance().registerMessage(lastererror, message);
            co_return Unexpected((SqlError::Code)lastererror);
        }
        else {
            co_return Unexpected((SqlError::Code)ret);
        }
    }
    auto sqlResult = std::make_unique<SqlStmtResult>(mMysql, mMysqlStmt);
    co_return std::make_unique<MysqlResultSet>(std::move(sqlResult));
}

auto MysqlStatement::execute() -> IoTask<size_t> {
    ILIAS_CO_TRYV(co_await query());
    auto rows = mysql_stmt_affected_rows(mMysqlStmt.get());
    co_return rows;
}

auto MysqlStatement::reset() -> void {
    clearBinds();
    if (mMysqlStmt != nullptr) {
        mysql_stmt_reset(mMysqlStmt.get());
    }
}

auto MysqlStatement::prepare(std::string_view sql) -> IoTask<void> {
    if (mMysqlStmt != nullptr) {
        co_await close();
    }
    mMysqlStmt = std::shared_ptr<MYSQL_STMT>(mMysql->stmtInit(), [](MYSQL_STMT *stmt) { mysql_stmt_close(stmt); });
    int  ret;
    auto queryp = parser(sql);
    ILIAS_TRACE("ilias-mysql", "prepare :{}", queryp);
    auto status = mysql_stmt_prepare_start(&ret, mMysqlStmt.get(), queryp.data(), (unsigned long)queryp.size());
    while (status) {
        ILIAS_TRACE("ilias-mysql", "stmt prepare waiting for status {}", status);
        auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
        if (!pret) {
            co_return Unexpected(pret.error());
        }
        status = mysql_stmt_prepare_cont(&ret, mMysqlStmt.get(), status);
    }
    if (ret != 0) {
        auto lastererror = mMysql->lastError();
        auto message     = mMysql->lastErrorMessage();
        ILIAS_ERROR("ilias-mysql", "stmt prepare failed. (error {}:{})", ret, mMysql->lastErrorMessage());
        if (lastererror != 0) {
            sql::SqlErrorCategory::instance().registerMessage(lastererror, message);
            co_return Unexpected((SqlError::Code)lastererror);
        }
        else {
            co_return Unexpected((SqlError::Code)ret);
        }
    }
    co_return {};
}

auto MysqlStatement::close() -> IoTask<void> {
    if (mMysqlStmt != nullptr) {
        my_bool ret    = 0;
        auto    status = mysql_stmt_close_start(&ret, mMysqlStmt.get());
        while (status) {
            ILIAS_TRACE("ilias-mysql", "stmt close waiting for status {}", status);
            auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
            if (!pret) {
                co_return Unexpected(pret.error());
            }
            status = mysql_stmt_close_cont(&ret, mMysqlStmt.get(), status);
        }
        if (ret != 0) {
            auto lastererror = mMysql->lastError();
            auto message     = mMysql->lastErrorMessage();
            ILIAS_ERROR("ilias-mysql", "stmt close failed, error: {}", mMysql->lastErrorMessage());
            if (lastererror != 0) {
                sql::SqlErrorCategory::instance().registerMessage(lastererror, message);
                co_return Unexpected((SqlError::Code)lastererror);
            }
            else {
                co_return Unexpected((SqlError::Code)ret);
            }
        }
    }
    mMysqlStmt = nullptr;
    co_return {};
}

auto MysqlStatement::nativeHandle() const -> void * {
    return mMysqlStmt.get();
}

auto MysqlStatement::dataBind(size_t index) -> MYSQL_BIND * {
    if (index <= mBinds.size()) {
        return &mBinds[index - 1];
    }
    return nullptr;
}

auto MysqlStatement::parser(std::string_view sql) -> std::string {
    auto parsed = sql::detail::rewrite_sql_placeholders(sql, sql::detail::SqlPlaceholderDialect::MySql,
                                                        sql::detail::SqlPlaceholderRewriteStyle::QuestionMark);
    mIndexs.clear();
    for (const auto &[name, index] : parsed.named_param_indices) {
        mIndexs[name] = static_cast<int>(index);
    }

    // 4. 根据总参数量重新分配 Binds 数组
    // 这里的 parameter_count 包含了所有的 ? 和 :name
    mBinds.resize(parsed.parameter_count);

    // 初始化 MYSQL_BIND 内存
    if (parsed.parameter_count > 0) {
        std::memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
        for (int i = 0; i < (int)mBinds.size(); ++i) {
            mBinds[i].buffer_type = MYSQL_TYPE_NULL;
        }
    }
    else {
        mBinds.clear();
        mDataGuards.clear();
    }

    return parsed.sql;
}

auto MysqlStatement::clearBinds() -> void {
    memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
    for (int i = 0; i < (int)mBinds.size(); ++i) {
        mBinds[i].buffer_type = MYSQL_TYPE_NULL;
    }
    mDataGuards.clear();
}

MysqlConnection::~MysqlConnection() {
}

auto MysqlConnection::sqlname() -> std::string {
    auto info = sqlinfo();
    if (info.find("MariaDB") != std::string::npos) {
        return "MariaDB";
    }
    else {
        return "MySQL";
    }
}

auto MysqlConnection::sqlinfo() -> std::string {
    return mMysql->info();
}

auto MysqlConnection::connect() -> IoTask<void> {
    if (mIsConnected) {
        co_return Unexpected(SqlError::Code::AlreadyConnected);
    }
    for (const auto &[key, value] : mOptions.extra) {
        if (auto type = sqlopt::detail::getMySqlOptEnum(key.c_str()); type != (mysql_option)-1) {
            sqlopt::OptionBase *option = sqlopt::createOption(type, value);
            mMysql->setOpt(option);
            delete option;
        }
    }
    ILIAS_CO_TRYV(
        co_await mMysql->connect(mOptions.host, mOptions.user, mOptions.password, mOptions.database, mOptions.port));
    ILIAS_CO_TRYV(co_await mMysql->query("SET time_zone = '+00:00'"));
    mIsConnected = true;
    co_return {};
}

auto MysqlConnection::disconnect() -> IoTask<void> {
    mIsConnected = false;
    ILIAS_CO_TRYV(co_await mMysql->disconnect());
    co_return {};
}

auto MysqlConnection::selectDatabase(std::string_view name) -> IoTask<void> {
    ILIAS_CO_TRYV(co_await mMysql->selectDb(name));
    co_return {};
}

auto MysqlConnection::prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> {
    auto stmt = std::make_unique<MysqlStatement>(mMysql);
    ILIAS_CO_TRYV(co_await stmt->prepare(sql));
    co_return std::move(stmt);
}

auto MysqlConnection::execute(std::string_view sql) -> IoTask<size_t> {
    ILIAS_CO_TRYV(co_await query(sql));
    auto ret = mysql_affected_rows(mMysql->native());
    co_return (size_t) ret;
}

auto MysqlConnection::query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> {
    ILIAS_ASSERT(mMysql != nullptr);
    // ILIAS_TRACE("ilias-mysql", "exec query {}", sql);
    ILIAS_CO_TRYV(co_await (mMysql->query(sql) | unstoppable()));
    auto sqlResult = std::make_unique<SqlQueryResult>(mMysql);
    co_return std::make_unique<MysqlResultSet>(std::move(sqlResult));
}

auto MysqlConnection::beginTransaction() -> IoTask<bool> {
    ILIAS_CO_TRYV(co_await mMysql->autoCommit(false));
    ILIAS_CO_TRY(auto ret, co_await mMysql->query("START TRANSACTION"));
    co_return ret;
}

auto MysqlConnection::commit() -> IoTask<bool> {
    ILIAS_CO_TRY(auto ret, co_await mMysql->commit());
    ILIAS_CO_TRYV(co_await mMysql->autoCommit(true));
    co_return ret;
}

auto MysqlConnection::rollback() -> IoTask<bool> {
    ILIAS_CO_TRY(auto ret, co_await mMysql->rollback());
    ILIAS_CO_TRYV(co_await mMysql->autoCommit(true));
    co_return ret;
}

auto MysqlConnection::syncRollback() -> bool {
    auto ret = mMysql->syncRollback();
    mMysql->syncAutoCommit(true);
    return ret;
}

auto MysqlConnection::lastInsertId() const -> int64_t {
    return (int64_t)mysql_insert_id(mMysql->native());
}

auto MysqlConnection::ping() -> IoTask<bool> {
    ILIAS_CO_TRYV(co_await mMysql->ping());
    co_return true;
}

auto MysqlConnection::nativeHandle() const -> void * {
    return mMysql->native();
}

auto MysqlConnection::valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> {
    return mMysql->valueConverterContext();
}

MySql::MySql() {
    mCtxt    = IoContext::currentThread();
    mContext = std::make_shared<SqlValueConverterContext>();
    if (mCtxt == nullptr) {
        ILIAS_ERROR("ilias-mysql", "no io context in current thread");
        return;
    }
    if (mysql_init(&mMysql) == nullptr) {
        ILIAS_ERROR("ilias-mysql", "mysql init failed");
    }

    auto ret = mysql_options(&mMysql, MYSQL_OPT_NONBLOCK, 0);
    if (ret != 0) {
        ILIAS_ERROR("ilias-mysql", "mysql set option failed, {}", ret);
    }

    registerMysqlTypeParsers(*mContext);
}

ILIAS_MYSQL_NS_END

ILIAS_SQL_REGISTER_PLUGIN(mysql) {
    return new ILIAS_MYSQL_COMPLETE_NAMESPACE::MysqlConnection(
        std::make_shared<ILIAS_MYSQL_COMPLETE_NAMESPACE::MySql>(), options);
}
