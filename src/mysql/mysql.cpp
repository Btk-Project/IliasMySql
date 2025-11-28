#include "ilias/sql/driver_registry.hpp"

#include "ilias/mysql/mysql.hpp"
#include "ilias/mysql/mysqlresult.hpp"
#include "ilias/mysql/mysqlopt.hpp"

ILIAS_MYSQL_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE
MySql::MySql() {
    mCtxt = IoContext::currentThread();
    if (mCtxt == nullptr) {
        ILIAS_ERROR("sql", "no io context in current thread");
        return;
    }
    if (mysql_init(&mMysql) == nullptr) {
        ILIAS_ERROR("sql", "mysql init failed");
    }

    auto ret = mysql_options(&mMysql, MYSQL_OPT_NONBLOCK, 0);
    if (ret != 0) {
        ILIAS_ERROR("sql", "mysql set option failed, {}", ret);
    }
}

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
    ILIAS_TRACE("sql", "poll events: {}", events);
    IoResult<unsigned int> ret;
    if (timeOut == 0) {
        auto ret = co_await mPoller.poll(pollEvents);
        if (!ret) {
            ILIAS_ERROR("sql", "poll failed, {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
    }
    else {
        auto ret = co_await (mPoller.poll(pollEvents) | setTimeout(std::chrono::milliseconds(timeOut)));
        if (!ret) {
            status = MYSQL_WAIT_TIMEOUT;
            co_return Unexpected(IoError::TimedOut);
        }
        if (!(*ret)) {
            if ((*ret).error() == IoError::TimedOut) {
                status = MYSQL_WAIT_TIMEOUT;
            }
            ILIAS_ERROR("sql", "poll failed, no result in poll.");
            co_return Unexpected((*ret).error());
        }
    }
    status = 0;
    if (ret.value_or(0) & POLLIN) {
        status |= MYSQL_WAIT_READ;
    }
    if (ret.value_or(0) | POLLOUT) {
        status |= MYSQL_WAIT_WRITE;
    }
    if (ret.value_or(0) | POLLPRI) {
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
            ILIAS_ERROR("sql", "get socket failed");                                                                   \
            co_return Unexpected(IoError::Unknown);                                                                    \
        }                                                                                                              \
        if (!mPoller || (mPoller.fd() != (fd_t)fd)) {                                                                  \
            mPoller = (co_await Poller::make((fd_t)fd, IoDescriptor ::Socket)).value();                                \
            if (!mPoller) {                                                                                            \
                ILIAS_ERROR("sql", "add fd({}) to IoContext failed.", fd);                                             \
                co_return Unexpected(IoError::Unknown);                                                                \
            }                                                                                                          \
        }                                                                                                              \
    }

#define SQL_PRIVATE_SYNC_CODE(OutP, MysqlFunc, ...)                                                                    \
    auto status = MysqlFunc##_start(&OutP, &mMysql, ##__VA_ARGS__);                                                    \
    if (status) {                                                                                                      \
        SQL_PRIVATE_MAKE_POLLER                                                                                        \
        while (status) {                                                                                               \
            ILIAS_TRACE("sql", "{} waiting for status {}", #MysqlFunc, status);                                        \
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
            ILIAS_ERROR("sql", "unknow type?");                                                                        \
            return false;                                                                                              \
        }                                                                                                              \
        return false;                                                                                                  \
    };                                                                                                                 \
    if (!_check(OutP)) {                                                                                               \
        auto errCode = mysql_errno(&mMysql);                                                                           \
        if (errCode != 0) {                                                                                            \
            [[maybe_unused]] auto error = mysql_error(&mMysql);                                                        \
            ILIAS_ERROR("sql", "{} failed, error({}): {}", #MysqlFunc, errCode, error);                                \
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
    ILIAS_TRACE("sql", "close mysql connection");
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
            ILIAS_TRACE("sql", "disconnect mysql waiting for status {}", status);
            auto pret = co_await pollStatus(status);
            status    = mysql_close_cont(&mMysql, status);
            if (!pret) {
                co_return Unexpected(pret.error());
            }
        }
    }
    ILIAS_TRACE("sql", "close mysql connection");
    co_return {};
}

auto MySql::autoCommit(bool autoMode) -> IoTask<bool> {
    // TODO: need query "select @@autocommit;" and get the result.
    my_bool ret;
    SQL_PRIVATE_SYNC_CODE(ret, mysql_autocommit, autoMode)
    co_return ret;
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

#undef SQL_PRIVATE_MAKE_POLLER
#undef SQL_PRIVATE_SYNC_CODE

class MysqlResultSet : public IResultSet {
public:
    MysqlResultSet(const MysqlResultSet &)            = delete;
    MysqlResultSet(MysqlResultSet &&)                 = default;
    MysqlResultSet &operator=(const MysqlResultSet &) = delete;
    MysqlResultSet &operator=(MysqlResultSet &&)      = default;
    MysqlResultSet(std::unique_ptr<MySqlResultBase> imp);
    virtual ~MysqlResultSet();
    auto next() -> IoTask<bool> override;
    auto rowCount() const -> size_t override;
    auto columnCount() const -> size_t override;
    auto columnName(size_t index) const -> std::string_view override;
    auto getValue(size_t index) -> IoResult<SqlValue> override;
    auto getValue(std::string_view name) -> IoResult<SqlValue> override;
    auto native() -> MYSQL_RES *;

private:
    std::unique_ptr<MySqlResultBase> mImp;
};
MysqlResultSet::~MysqlResultSet() {
}

MysqlResultSet::MysqlResultSet(std::unique_ptr<MySqlResultBase> imp) : mImp(std::move(imp)) {
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
auto MysqlResultSet::getValue(size_t index) -> IoResult<SqlValue> {
    return mImp->get(index);
}
auto MysqlResultSet::getValue(std::string_view name) -> IoResult<SqlValue> {
    return mImp->get(name);
}

auto MysqlResultSet::native() -> MYSQL_RES * {
    return mImp->nativeResult();
}

class MysqlStatement : public IStatement {
public:
    using SqlError                                    = sql::SqlError;
    MysqlStatement(const MysqlStatement &)            = delete;
    MysqlStatement(MysqlStatement &&)                 = default;
    MysqlStatement &operator=(const MysqlStatement &) = delete;
    MysqlStatement &operator=(MysqlStatement &&)      = default;
    MysqlStatement(std::shared_ptr<MySql> mysql);
    ~MysqlStatement();

    auto bind(size_t index, SqlValueView value) -> Result<void, std::error_code> override;
    auto bind(std::string_view name, SqlValueView value) -> Result<void, std::error_code> override;
    // 执行查询 (SELECT)，返回结果集
    auto query() -> IoTask<std::unique_ptr<IResultSet>> override;
    // 执行命令 (INSERT, UPDATE, DELETE)，返回影响行数
    auto execute() -> IoTask<size_t> override;
    // 重置状态以便复用
    auto reset() -> void override;
    auto prepare(std::string_view sql) -> IoTask<void>;
    auto clearBinds() -> void;
    auto close() -> IoTask<void>;

private:
    // this query should like "SELECT * FROM table WHERE name=:name,age=:age;"
    // return query like "SELECT * FROM table WHERE name=?,age=?;"
    auto parser(std::string_view sql) -> std::string;
    auto makeBindData(SqlValueView value) -> Result<MYSQL_BIND, std::error_code>;

private:
    std::shared_ptr<MySql> mMysql;
    MYSQL_STMT            *mMysqlStmt = nullptr;
    std::vector<std::variant<SqlValueTraits<SqlValueType::kChar>::type, SqlValueTraits<SqlValueType::kInt>::type,
                             SqlValueTraits<SqlValueType::kBigInt>::type, SqlValueTraits<SqlValueType::kFloat>::type,
                             SqlValueTraits<SqlValueType::kDouble>::type, MYSQL_TIME>>
                                         mBindBuffer; // save var to continue it is life.
    std::vector<MYSQL_BIND>              mBinds;
    std::unordered_map<std::string, int> mIndexs;
};

MysqlStatement::MysqlStatement(std::shared_ptr<MySql> mysql) {
    mMysql = std::move(mysql);
}

MysqlStatement::~MysqlStatement() {
    if (mMysqlStmt) {
        mysql_stmt_close(mMysqlStmt);
    }
}

MYSQL_TIME toMysqlTime(const SqlDate &dt) {
    MYSQL_TIME time;
    memset(&time, 0, sizeof(time));
    switch (dt.type) {
        case SqlDate::kDate:
            time.year      = dt.year;
            time.month     = dt.month;
            time.day       = dt.day;
            time.time_type = MYSQL_TIMESTAMP_DATE;
            break;
        case SqlDate::kDateTime:
            time.year        = dt.year;
            time.month       = dt.month;
            time.day         = dt.day;
            time.hour        = dt.hour;
            time.minute      = dt.minute;
            time.second      = dt.second;
            time.second_part = dt.microsecond * 1000;
            time.time_type   = MYSQL_TIMESTAMP_DATETIME;
            break;
        case SqlDate::kTime:
            time.hour        = dt.hour;
            time.minute      = dt.minute;
            time.second      = dt.second;
            time.second_part = dt.microsecond * 1000;
            time.time_type   = MYSQL_TIMESTAMP_TIME;
            break;
        default:
            time.time_type = MYSQL_TIMESTAMP_NONE;
    }
    return time;
}

auto MysqlStatement::makeBindData(SqlValueView value) -> Result<MYSQL_BIND, std::error_code> {
    MYSQL_BIND bind;
    memset(&bind, 0, sizeof(bind));
    switch ((SqlValueType)value.index()) {
        case SqlValueType::kNull:
            bind.buffer_type   = MYSQL_TYPE_NULL;
            bind.is_null_value = true;
            bind.buffer_length = 0;
            break;
        case SqlValueType::kChar:
            bind.buffer_type = MYSQL_TYPE_TINY;
            mBindBuffer.push_back(get<SqlValueType::kChar>(value));
            bind.buffer        = std::get_if<SqlValueTraits<SqlValueType::kChar>::type>(&mBindBuffer.back());
            bind.buffer_length = sizeof(SqlValueTraits<SqlValueType::kChar>::type);
            break;
        case SqlValueType::kInt:
            bind.buffer_type = MYSQL_TYPE_LONG;
            mBindBuffer.push_back(get<SqlValueType::kInt>(value));
            bind.buffer        = std::get_if<SqlValueTraits<SqlValueType::kInt>::type>(&mBindBuffer.back());
            bind.buffer_length = sizeof(SqlValueTraits<SqlValueType::kInt>::type);
            break;
        case SqlValueType::kBigInt:
            bind.buffer_type = MYSQL_TYPE_LONGLONG;
            mBindBuffer.push_back(get<SqlValueType::kBigInt>(value));
            bind.buffer        = std::get_if<SqlValueTraits<SqlValueType::kBigInt>::type>(&mBindBuffer.back());
            bind.buffer_length = sizeof(SqlValueTraits<SqlValueType::kBigInt>::type);
            break;
        case SqlValueType::kFloat:
            bind.buffer_type = MYSQL_TYPE_FLOAT;
            mBindBuffer.push_back(get<SqlValueType::kFloat>(value));
            bind.buffer        = std::get_if<SqlValueTraits<SqlValueType::kFloat>::type>(&mBindBuffer.back());
            bind.buffer_length = sizeof(SqlValueTraits<SqlValueType::kFloat>::type);
            break;
        case SqlValueType::kDouble:
            bind.buffer_type = MYSQL_TYPE_DOUBLE;
            mBindBuffer.push_back(get<SqlValueType::kDouble>(value));
            bind.buffer        = std::get_if<SqlValueTraits<SqlValueType::kDouble>::type>(&mBindBuffer.back());
            bind.buffer_length = sizeof(SqlValueTraits<SqlValueType::kDouble>::type);
            break;
        case SqlValueType::kText:
            bind.buffer_type   = MYSQL_TYPE_STRING;
            bind.buffer        = const_cast<char *>(get<SqlValueType::kText>(value).data());
            bind.buffer_length = get<SqlValueType::kText>(value).size();
            break;
        case SqlValueType::kBlob:
            bind.buffer_type   = MYSQL_TYPE_BLOB;
            bind.buffer        = const_cast<std::byte *>(get<SqlValueType::kBlob>(value).data());
            bind.buffer_length = get<SqlValueType::kBlob>(value).size();
            break;
        case SqlValueType::kDate:
            mBindBuffer.push_back(toMysqlTime(get<SqlValueType::kDate>(value)));
            switch (std::get<MYSQL_TIME>(mBindBuffer.back()).time_type) {
                case MYSQL_TIMESTAMP_DATE:
                    bind.buffer_type = MYSQL_TYPE_DATE;
                    break;
                case MYSQL_TIMESTAMP_DATETIME:
                    bind.buffer_type = MYSQL_TYPE_DATETIME;
                    break;
                case MYSQL_TIMESTAMP_TIME:
                    bind.buffer_type = MYSQL_TYPE_TIME;
                    break;
                default:
                    return Unexpected(std::make_error_code(std::errc::invalid_argument));
            }
            bind.buffer        = std::get_if<MYSQL_TIME>(&mBindBuffer.back());
            bind.buffer_length = sizeof(MYSQL_TIME);
            break;
        default:
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    return bind;
}

auto MysqlStatement::bind(size_t index, SqlValueView value) -> Result<void, std::error_code> {
    ILIAS_INFO("ilias-mysql", "bind {} with {}", index, value);
    if (mMysqlStmt == nullptr) {
        return Unexpected(SqlError::NOT_PREPARED);
    }
    if (index - 1 >= mBinds.size()) {
        return Unexpected(SqlError::INVALID_INDEX);
    }
    auto data = makeBindData(value);
    if (!data) {
        return Unexpected(data.error());
    }
    mBinds[index - 1] = data.value();
    return {};
}

auto MysqlStatement::bind(std::string_view name, SqlValueView value) -> Result<void, std::error_code> {
    // ILIAS_INFO("ilias-mysql", "bind {} = {}", name, value);
    auto index = mIndexs.find(std::string(name));
    if (index == mIndexs.end()) {
        return Unexpected(SqlError::INVALID_INDEX);
    }
    return bind(index->second + 1, value);
}

auto MysqlStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    if (mMysqlStmt == nullptr) {
        co_return Unexpected(SqlError::Code::NOT_PREPARED);
    }
    int ret = 0;
    if (mBinds.size() > 0) {
        ret = mysql_stmt_bind_param(mMysqlStmt, mBinds.data());
    }
    if (ret != 0) {
        auto lastererror = mMysql->lastError();
        auto message     = mMysql->lastErrorMessage();
        ILIAS_ERROR("sql", "stmt bind failed. (error {}:{})", ret, message);
        if (lastererror != 0) {
            sql::SqlErrorCategory::instance().registerMessage(lastererror, message);
            co_return Unexpected((SqlError::Code)lastererror);
        }
        else {
            co_return Unexpected((SqlError::Code)ret);
        }
    }
    auto status = mysql_stmt_execute_start(&ret, mMysqlStmt);
    while (status) {
        ILIAS_TRACE("sql", "stmt execute waiting for status {}", status);
        auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
        if (!pret) {
            ILIAS_ERROR("sql", "stmt execute failed. (error: {})", pret.error().message());
            co_return Unexpected(pret.error());
        }
        status = mysql_stmt_execute_cont(&ret, mMysqlStmt, status);
    }
    if (ret != 0) {
        auto lastererror = mMysql->lastError();
        auto message     = mMysql->lastErrorMessage();
        ILIAS_ERROR("sql", "stmt execute failed. (error {}:{})", ret, mMysql->lastErrorMessage());
        if (lastererror != 0) {
            sql::SqlErrorCategory::instance().registerMessage(lastererror, message);
            co_return Unexpected((SqlError::Code)lastererror);
        }
        else {
            co_return Unexpected((SqlError::Code)ret);
        }
    }
    auto sqlResult = std::make_unique<SqlStmtResult>(mMysql, mMysqlStmt);
    auto ret1      = co_await sqlResult->getResult();
    clearBinds();
    if (!ret1) {
        co_return Unexpected(ret1.error());
    }
    co_return std::make_unique<MysqlResultSet>(std::move(sqlResult));
}

auto MysqlStatement::execute() -> IoTask<size_t> {
    auto result = co_await query();
    if (!result) {
        co_return Unexpected(result.error());
    }
    auto rows = mysql_stmt_affected_rows(mMysqlStmt);
    co_return rows;
}

auto MysqlStatement::reset() -> void {
    if (mMysqlStmt != nullptr) {
        mysql_stmt_reset(mMysqlStmt);
    }
}

auto MysqlStatement::prepare(std::string_view sql) -> IoTask<void> {
    if (mMysqlStmt != nullptr) {
        co_await close();
    }
    mMysqlStmt = mMysql->stmtInit();
    int  ret;
    auto queryp = parser(sql);
    ILIAS_TRACE("sql", "prepare :{}", queryp);
    auto status = mysql_stmt_prepare_start(&ret, mMysqlStmt, queryp.data(), (unsigned long)queryp.size());
    while (status) {
        ILIAS_TRACE("sql", "stmt prepare waiting for status {}", status);
        auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
        if (!pret) {
            co_return Unexpected(pret.error());
        }
        status = mysql_stmt_prepare_cont(&ret, mMysqlStmt, status);
    }
    if (ret != 0) {
        auto lastererror = mMysql->lastError();
        auto message     = mMysql->lastErrorMessage();
        ILIAS_ERROR("sql", "stmt prepare failed. (error {}:{})", ret, mMysql->lastErrorMessage());
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
        auto    status = mysql_stmt_close_start(&ret, mMysqlStmt);
        while (status) {
            ILIAS_TRACE("sql", "stmt close waiting for status {}", status);
            auto pret = co_await (mMysql->pollStatus(status) | unstoppable());
            if (!pret) {
                co_return Unexpected(pret.error());
            }
            status = mysql_stmt_close_cont(&ret, mMysqlStmt, status);
        }
        if (ret != 0) {
            auto lastererror = mMysql->lastError();
            auto message     = mMysql->lastErrorMessage();
            ILIAS_ERROR("sql", "stmt close failed, error: {}", mMysql->lastErrorMessage());
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

auto MysqlStatement::parser(std::string_view sql) -> std::string {
    mIndexs.clear();
    mBindBuffer.clear();
    mBinds.clear();
    std::string ret;
    auto        start = sql.find_first_of(':');
    if (start != std::string::npos) {
        ret = sql.substr(0, start);
    }
    else {
        return std::string(sql);
    }
    auto end = start;
    while (start != std::string::npos) {
        end = start;
        while (end < sql.size() && sql[end] != ' ' && sql[end] != ',' && sql[end] != '\t' && sql[end] != '\n' &&
               sql[end] != '\r' && sql[end] != ')' && sql[end] != '(' && sql[end] != '"') {
            end++;
        }
        auto name = sql.substr(start + 1, end - start - 1);
        mIndexs.emplace(name, mIndexs.size());
        ret += '?';
        start = sql.find_first_of(':', end);
        ret += sql.substr(end, start - end);
    }
    mBinds.resize(mIndexs.size());
    memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
    for (int i = 0; i < (int)mBinds.size(); ++i) {
        mBinds[i].buffer_type = MYSQL_TYPE_NULL;
    }
    mBindBuffer.resize(mIndexs.size());
    return ret;
}

auto MysqlStatement::clearBinds() -> void {
    memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
    for (int i = 0; i < (int)mBinds.size(); ++i) {
        mBinds[i].buffer_type = MYSQL_TYPE_NULL;
    }
}

class MysqlConnection : public IConnection {
public:
    MysqlConnection(std::shared_ptr<MySql> mysql, ConnectOptions options) : mMysql(mysql), mOptions(options) {}
    auto sqlname() -> std::string override;
    auto sqlinfo() -> std::string override;
    auto connect() -> IoTask<void> override;
    auto disconnect() -> IoTask<void> override;
    auto selectDatabase(std::string_view name) -> IoTask<void> override;
    // 预编译 SQL
    auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> override;
    // 直接执行 SQL (不带参)
    auto execute(std::string_view sql) -> IoTask<size_t> override;
    auto query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> override;
    // 事务控制
    auto beginTransaction() -> IoTask<bool> override;
    auto commit() -> IoTask<bool> override;
    auto rollback() -> IoTask<bool> override;
    auto syncRollback() -> bool override;
    // 获取最后一次插入的 ID
    auto lastInsertId() const -> int64_t override;
    // 连通性检测
    auto ping() -> IoTask<bool> override;
    auto mysql() -> std::shared_ptr<MySql> { return mMysql; }

private:
    std::shared_ptr<MySql> mMysql;
    ConnectOptions         mOptions;
    bool                   mIsConnected = false;
};

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
        co_return Unexpected(SqlError::Code::ALREADY_CONNECTED);
    }
    for (const auto &[key, value] : mOptions.extra) {
        if (auto type = sqlopt::detail::getMySqlOptEnum(key.c_str()); type != (mysql_option)-1) {
            sqlopt::OptionBase *option = sqlopt::createOption(type, value);
            mMysql->setOpt(option);
            delete option;
        }
    }
    auto ret =
        co_await mMysql->connect(mOptions.host, mOptions.user, mOptions.password, mOptions.database, mOptions.port);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    mIsConnected = true;
    co_return {};
}

auto MysqlConnection::disconnect() -> IoTask<void> {
    mIsConnected = false;
    auto ret     = co_await mMysql->disconnect();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return {};
}

auto MysqlConnection::selectDatabase(std::string_view name) -> IoTask<void> {
    auto ret = co_await mMysql->selectDb(name);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return {};
}

auto MysqlConnection::prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> {
    auto stmt = std::make_unique<MysqlStatement>(mMysql);
    auto ret  = co_await stmt->prepare(sql);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return std::move(stmt);
}

auto MysqlConnection::execute(std::string_view sql) -> IoTask<size_t> {
    auto retult = co_await query(sql);
    if (!retult) {
        co_return Unexpected(retult.error());
    }
    auto ret = mysql_affected_rows(mMysql->native());
    co_return (size_t) ret;
}

auto MysqlConnection::query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> {
    ILIAS_ASSERT(mMysql != nullptr);
    ILIAS_TRACE("sql", "exec query {}", sql);
    auto ret = co_await (mMysql->query(sql) | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto sqlResult = std::make_unique<SqlQueryResult>(mMysql);
    auto ret1      = co_await sqlResult->getResult();
    if (!ret1) {
        co_return Unexpected(ret1.error());
    }
    co_return std::make_unique<MysqlResultSet>(std::move(sqlResult));
}

auto MysqlConnection::beginTransaction() -> IoTask<bool> {
    auto ret = co_await mMysql->autoCommit(false);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return ret;
}

auto MysqlConnection::commit() -> IoTask<bool> {
    auto ret = co_await mMysql->commit();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return ret;
}

auto MysqlConnection::rollback() -> IoTask<bool> {
    auto ret = co_await mMysql->rollback();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return ret;
}

auto MysqlConnection::syncRollback() -> bool {
    return mMysql->syncRollback();
}

auto MysqlConnection::lastInsertId() const -> int64_t {
    return (int64_t)mysql_insert_id(mMysql->native());
}

auto MysqlConnection::ping() -> IoTask<bool> {
    auto ret = co_await mMysql->ping();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return true;
}

void ilias_register_sql_plugin(DriverManager *manager) {
    manager->registerDriver("mysql", [](const ConnectOptions &options) -> std::unique_ptr<IConnection> {
        auto connection = std::make_unique<MysqlConnection>(std::make_shared<MySql>(), options);
        return connection;
    });
}

ILIAS_MYSQL_NS_END