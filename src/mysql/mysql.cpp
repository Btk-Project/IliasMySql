#include "ilias/sql/driver_registry.hpp"

#include "ilias/mysql/mysql.hpp"
#include "ilias/mysql/mysqlresult.hpp"
#include "ilias/mysql/mysqlopt.hpp"
#include "ilias/sql/sql_plugin.hpp"

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
        auto ret = co_await mPoller.poll(pollEvents);
        if (!ret) {
            ILIAS_ERROR("ilias-mysql", "poll failed, {}", ret.error().message());
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
            ILIAS_ERROR("ilias-mysql", "poll failed, no result in poll.");
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
    return {};
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
    auto result = co_await query();
    if (!result) {
        co_return Unexpected(result.error());
    }
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
    mIndexs.clear();

    std::string ret;
    ret.reserve(sql.size());
    int param_counter = 0;

    bool in_string  = false;
    char quote_char = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];

        // 1. 处理字符串字面量 (例如 'time: 12:00' 或 "name")
        if (in_string) {
            ret += c;
            if (c == quote_char) {
                // 处理转义字符，如 'It''s' 或 'It\'s' (取决于 SQL 模式，这里做简单处理)
                if (i + 1 < sql.size() && sql[i + 1] == quote_char) {
                    ret += sql[i + 1];
                    i++;
                }
                else {
                    in_string = false;
                }
            }
            continue;
        }

        // 进入字符串模式
        if (c == '\'' || c == '"' || c == '`') {
            in_string  = true;
            quote_char = c;
            ret += c;
            continue;
        }

        // 2. 处理位置参数 ?
        if (c == '?') {
            ret += '?';
            param_counter++; // 占据一个索引位
            continue;
        }

        // 3. 处理命名参数 :name
        if (c == ':') {
            // 处理双冒号 :: (通常用于类型转换，如 postgres，虽然这是 mysql 驱动，但在 sql 字符串中最好做兼容)
            if (i + 1 < sql.size() && sql[i + 1] == ':') {
                ret += "::";
                i++;
                continue;
            }

            // 检查冒号后面是否有合法的参数名字符
            size_t j = i + 1;
            if (j >= sql.size()) {
                // SQL 以 : 结尾，非法但保留原样
                ret += c;
                continue;
            }

            // 如果冒号后面不是字母、下划线，则视为普通冒号 (例如 12:30)
            // 你可以根据需求调整这里的判定，比如必须以字母开头
            if (!std::isalnum(static_cast<unsigned char>(sql[j])) && sql[j] != '_') {
                ret += c;
                continue;
            }

            // 提取参数名
            while (j < sql.size()) {
                char next_c = sql[j];
                // 允许的参数名字符: 字母, 数字, 下划线
                if (std::isalnum(static_cast<unsigned char>(next_c)) || next_c == '_') {
                    j++;
                }
                else {
                    break;
                }
            }

            std::string_view name_view = sql.substr(i + 1, j - (i + 1));
            std::string      name(name_view);

            // 记录映射关系：名字 -> 当前的全局索引
            mIndexs[name] = param_counter;

            // 替换为 ?
            ret += '?';
            param_counter++;

            // 移动主循环索引
            i = j - 1;
            continue;
        }

        // 普通字符
        ret += c;
    }

    // 4. 根据总参数量重新分配 Binds 数组
    // 这里的 param_counter 包含了所有的 ? 和 :name
    mBinds.resize(param_counter);

    // 初始化 MYSQL_BIND 内存
    if (param_counter > 0) {
        std::memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
        for (int i = 0; i < (int)mBinds.size(); ++i) {
            mBinds[i].buffer_type = MYSQL_TYPE_NULL;
        }
    }
    else {
        mBinds.clear();
        mDataGuards.clear();
    }

    return ret;
}

auto MysqlStatement::clearBinds() -> void {
    memset(mBinds.data(), 0, sizeof(MYSQL_BIND) * mBinds.size());
    for (int i = 0; i < (int)mBinds.size(); ++i) {
        mBinds[i].buffer_type = MYSQL_TYPE_NULL;
    }
    mDataGuards.clear();
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
    auto ret =
        co_await mMysql->connect(mOptions.host, mOptions.user, mOptions.password, mOptions.database, mOptions.port);
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto set_time_zone = co_await mMysql->query("SET time_zone = '+00:00'");
    if (!set_time_zone) {
        co_return Unexpected(set_time_zone.error());
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
    // ILIAS_TRACE("ilias-mysql", "exec query {}", sql);
    auto ret = co_await (mMysql->query(sql) | unstoppable());
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto sqlResult = std::make_unique<SqlQueryResult>(mMysql);
    co_return std::make_unique<MysqlResultSet>(std::move(sqlResult));
}

auto MysqlConnection::beginTransaction() -> IoTask<bool> {
    auto close_auto_commit = co_await mMysql->autoCommit(false);
    if (!close_auto_commit) {
        co_return Unexpected(close_auto_commit.error());
    }
    auto ret = co_await mMysql->query("START TRANSACTION");
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
    auto open_auto_commit = co_await mMysql->autoCommit(true);
    if (!open_auto_commit) {
        co_return Unexpected(open_auto_commit.error());
    }
    co_return ret;
}

auto MysqlConnection::rollback() -> IoTask<bool> {
    auto ret = co_await mMysql->rollback();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    auto open_auto_commit = co_await mMysql->autoCommit(true);
    if (!open_auto_commit) {
        co_return Unexpected(open_auto_commit.error());
    }
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
    auto ret = co_await mMysql->ping();
    if (!ret) {
        co_return Unexpected(ret.error());
    }
    co_return true;
}

auto MysqlConnection::nativeHandle() const -> void * {
    return mMysql->native();
}

auto MysqlConnection::valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> {
    return mMysql->valueConverterContext();
}

// ----------------------- parser and binder -----------------------
SqlParserResult mysql_parse_null(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return std::any(g_sql_null);
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}
SqlBinderResult mysql_bind_null(const SqlCellView &cell, std::any data) {
    auto index = cell.index();
    // 无视cell内部的值
    auto mysqlstmt      = std::any_cast<MysqlStatement *>(data);
    auto bind           = mysqlstmt->dataBind(index);
    bind->buffer_type   = MYSQL_TYPE_NULL;
    bind->is_null_value = true;
    bind->buffer_length = 0;
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_integral_v<T>
SqlParserResult mysql_parse_interage(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    // mysql返回的可能是字符串，也可能是数据的指针
    auto string = cell.formatted_value();
    switch (cell.formatted_type()) {
        case MYSQL_TYPE_TINY: {
            int res;
            auto [ptr, ec] = std::from_chars(string.begin(), string.end(), res);
            if (ec != std::errc() || ptr != string.end()) {
                // 可能是数字 或者 true/false
                if (string == "true" || string == "false") {
                    return std::any(static_cast<T>(string == "true"));
                } // if is a boolean
                return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
            }
            return std::any(static_cast<T>(res));
        }
        case MYSQL_TYPE_SHORT: // short
        case MYSQL_TYPE_LONG:  // int
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_LONGLONG:
        case MYSQL_TYPE_NEWDECIMAL: {
            int64_t res;
            auto [ptr, ec] = std::from_chars(string.begin(), string.end(), res);
            if (ec != std::errc() || ptr != string.end()) {
                return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
            }
            return std::any(static_cast<T>(res));
        }
    }

    if (cell.raw_value() == nullptr) {
        return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
    }
    if (cell.raw_type() == std::type_index(typeid(int32_t))) {
        int32_t value = *reinterpret_cast<const int32_t *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    else if (cell.raw_type() == std::type_index(typeid(int64_t))) {
        int64_t value = *reinterpret_cast<const int64_t *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
}

template <typename T>
    requires std::is_integral_v<T>
SqlBinderResult mysql_bind_interage(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind = mysqlstmt->dataBind(index);
    switch (sizeof(T)) {
        case sizeof(char):
            bind->buffer_type = MYSQL_TYPE_TINY;
            break;
        case sizeof(int32_t):
            bind->buffer_type = MYSQL_TYPE_LONG;
            break;
        case sizeof(int64_t):
            bind->buffer_type = MYSQL_TYPE_LONGLONG;
            break;
        default:
            return Unexpected(SqlError::Code::UnsupportBindType);
    }
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

template <typename T>
    requires std::is_floating_point_v<T>
SqlParserResult mysql_parse_real(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    // mysql返回的可能是字符串，也可能是数据的指针
    auto string = cell.formatted_value();
    switch (cell.formatted_type()) {
        case MYSQL_TYPE_FLOAT:
        case MYSQL_TYPE_SHORT:    // short
        case MYSQL_TYPE_LONG:     // int
        case MYSQL_TYPE_LONGLONG: // long long
        case MYSQL_TYPE_INT24:
        case MYSQL_TYPE_DOUBLE:
        case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL: {
            T res;
            auto [ptr, ec] = std::from_chars(string.begin(), string.end(), res);
            if (ec != std::errc() || ptr != string.end()) {
                return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
            }
            return std::any(static_cast<T>(res));
        }
    }

    if (cell.raw_value() == nullptr) {
        return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
    }
    if (cell.raw_type() == std::type_index(typeid(float))) {
        float value = *reinterpret_cast<const float *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    else if (cell.raw_type() == std::type_index(typeid(double))) {
        double value = *reinterpret_cast<const double *>(cell.raw_value());
        return std::any(static_cast<T>(value));
    }
    return Unexpected(sql::SqlError::UnsupportConvertFromSqlType);
}

template <typename T>
    requires std::is_floating_point_v<T>
SqlBinderResult mysql_bind_real(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const T))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind = mysqlstmt->dataBind(index);
    switch (sizeof(T)) {
        case sizeof(float):
            bind->buffer_type = MYSQL_TYPE_FLOAT;
            break;
        case sizeof(double):
            bind->buffer_type = MYSQL_TYPE_DOUBLE;
            break;
        default:
            return Unexpected(SqlError::Code::UnsupportBindType);
    }
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

SqlParserResult mysql_parse_string(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        return std::any(SqlText(cell.formatted_value()));
    }
    if (cell.format() == SqlCellView::DataFormat::kValuePointer) {
        if (cell.raw_type() == std::type_index(typeid(float))) {
            float value = *reinterpret_cast<const float *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
        else if (cell.raw_type() == std::type_index(typeid(double))) {
            double value = *reinterpret_cast<const double *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
        else if (cell.raw_type() == std::type_index(typeid(int32_t))) {
            int32_t value = *reinterpret_cast<const int32_t *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
        else if (cell.raw_type() == std::type_index(typeid(int64_t))) {
            int64_t value = *reinterpret_cast<const int64_t *>(cell.raw_value());
            return std::any(std::to_string(value));
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlParserResult mysql_parse_string_view(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        return std::any(SqlTextView(cell.formatted_value()));
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlBinderResult mysql_bind_string(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const char *))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind           = mysqlstmt->dataBind(index);
    bind->buffer_type   = MYSQL_TYPE_STRING;
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

SqlParserResult mysql_parse_blob_view(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        auto value = cell.formatted_value();
        return std::any(std::span<const std::byte>(reinterpret_cast<const std::byte *>(value.data()), value.size()));
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlParserResult mysql_parse_blob(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        auto value = cell.formatted_value();
        return std::any(std::vector<std::byte>(reinterpret_cast<const std::byte *>(value.data()),
                                               reinterpret_cast<const std::byte *>(value.data()) + value.size()));
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlBinderResult mysql_bind_blob(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const std::byte *))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    auto bind           = mysqlstmt->dataBind(index);
    bind->buffer_type   = MYSQL_TYPE_BLOB;
    bind->buffer        = const_cast<void *>(cell.raw_value());
    bind->buffer_length = cell.raw_value_size();
    return make_null_sql_binder_result();
}

SqlParserResult mysql_parse_date(const SqlCellView &cell) {
    if (cell.is_null() || cell.formatted_type() == MYSQL_TYPE_NULL ||
        cell.raw_type() == std::type_index(typeid(g_sql_null))) {
        return Unexpected(SqlError::Code::NullValue);
    }
    if (cell.format() == SqlCellView::DataFormat::kString) {
        auto type  = cell.formatted_type();
        auto value = cell.formatted_value();
        switch (type) {
            case MYSQL_TYPE_TIMESTAMP:
            case MYSQL_TYPE_DATETIME:
            case MYSQL_TYPE_TIME:
            case MYSQL_TYPE_DATE: {
                sql::SqlDate date;
                date.fromUTCString(value);
                return std::any(date);
            }
        }
    }
    return Unexpected(SqlError::Code::UnsupportConvertFromSqlType);
}

SqlBinderResult mysql_bind_date(const SqlCellView &cell, std::any data) {
    if (cell.is_null()) {
        return mysql_bind_null(cell, data);
    }
    auto index     = cell.index();
    auto mysqlstmt = std::any_cast<MysqlStatement *>(data);
    if (cell.format() != SqlCellView::DataFormat::kValuePointer ||
        cell.raw_type() != std::type_index(typeid(const SqlDate))) {
        return Unexpected(SqlError::Code::InvalidDataFormat);
    }
    std::unique_ptr<void, void (*)(void *)> ptr {malloc(sizeof(MYSQL_TIME)), free};

    auto mtime = reinterpret_cast<MYSQL_TIME *>(ptr.get());
    *mtime     = toMysqlTime(*reinterpret_cast<const SqlDate *>(cell.raw_value()));
    auto bind  = mysqlstmt->dataBind(index);
    switch (mtime->time_type) {
        case MYSQL_TIMESTAMP_DATE:
            bind->buffer_type = MYSQL_TYPE_DATE;
            break;
        case MYSQL_TIMESTAMP_DATETIME:
            bind->buffer_type = MYSQL_TYPE_DATETIME;
            break;
        case MYSQL_TIMESTAMP_TIME:
            bind->buffer_type = MYSQL_TYPE_TIME;
            break;
        default:
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
    }
    bind->buffer        = const_cast<void *>(ptr.get());
    bind->buffer_length = sizeof(MYSQL_TIME);
    return ptr;
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

    mContext->registerType<SqlNull>(mysql_parse_null);
    mContext->registerType<SqlNull>(mysql_bind_null);
    mContext->registerType<SqlBool>(mysql_parse_interage<SqlBool>);
    mContext->registerType<SqlBool>(mysql_bind_interage<SqlBool>);
    mContext->registerType<SqlTinyInt>(mysql_parse_interage<SqlTinyInt>);
    mContext->registerType<char>(mysql_parse_interage<char>);
    mContext->registerType<SqlTinyInt>(mysql_bind_interage<SqlTinyInt>);
    mContext->registerType<char>(mysql_bind_interage<char>);
    mContext->registerType<SqlInt>(mysql_parse_interage<SqlInt>);
    mContext->registerType<SqlInt>(mysql_bind_interage<SqlInt>);
    mContext->registerType<SqlBigInt>(mysql_parse_interage<SqlBigInt>);
    mContext->registerType<SqlBigInt>(mysql_bind_interage<SqlBigInt>);
    mContext->registerType<SqlFloat>(mysql_parse_real<SqlFloat>);
    mContext->registerType<SqlFloat>(mysql_bind_real<SqlFloat>);
    mContext->registerType<double>(mysql_parse_real<double>);
    mContext->registerType<double>(mysql_bind_real<double>);
    mContext->registerType<SqlText>(mysql_parse_string);
    mContext->registerType<const char *>(mysql_bind_string);
    mContext->registerType<SqlTextView>(mysql_parse_string_view);
    mContext->registerType<SqlBlob>(mysql_parse_blob);
    mContext->registerType<SqlBlobView>(mysql_parse_blob_view);
    mContext->registerType<const std::byte *>(mysql_bind_blob);
    mContext->registerType<SqlDate>(mysql_parse_date);
    mContext->registerType<SqlDate>(mysql_bind_date);
}

ILIAS_MYSQL_NS_END

ILIAS_SQL_REGISTER_PLUGIN(mysql) {
    return new ILIAS_MYSQL_COMPLETE_NAMESPACE::MysqlConnection(
        std::make_shared<ILIAS_MYSQL_COMPLETE_NAMESPACE::MySql>(), options);
}