#include "ilias/postgres/postgres_impl.hpp"
#include "ilias/postgres/postgres.hpp"
#include <libpq/libpq-fs.h>

#include <poll.h>
#include <atomic>
#include <string>
#include <variant>

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

// #############################################################################
// #  Postgres (Low-Level Wrapper) Implementation
// #############################################################################

Postgres::Postgres() {
    mCtxt = IoContext::currentThread();
    if (mCtxt == nullptr) {
        ILIAS_ERROR("ilias-pgsql", "no io context in current thread");
    }
}

Postgres::~Postgres() {
    disconnect();
}

auto Postgres::native() -> PGconn * {
    return mConn;
}

bool Postgres::operator==(Postgres &other) {
    return mConn == other.mConn;
}

auto Postgres::disconnect() -> void {
    if (mConn) {
        mPoller.close();
        PQfinish(mConn);
        mConn = nullptr;
    }
}

auto Postgres::lastError() -> int {
    if (!mConn)
        return -1;
    return static_cast<int>(PQstatus(mConn));
}

auto Postgres::lastErrorMessage() -> const char * {
    if (!mConn)
        return "No connection";
    return PQerrorMessage(mConn);
}

// 在 connect() 成功后调用
auto Postgres::initializeTypeMap() -> IoTask<void> {
    // 这个查询获取了基本类型的OID和它们的名称
    const char *query_sql =
        "SELECT oid, typname FROM pg_type WHERE typtype = 'b' AND typcategory IN ('B', 'N', 'S', 'T', 'U', 'V', 'X');";

    auto send_result = co_await sendQuery(query_sql);
    if (!send_result) {
        co_return Unexpected(send_result.error());
    }

    auto res_ptr = co_await getResult();
    if (!res_ptr) {
        co_return Unexpected(res_ptr.error());
    }
    auto ret = PQresultStatus(res_ptr.value());
    if (ret != PGRES_TUPLES_OK && ret != PGRES_COMMAND_OK) {
        std::unique_ptr<PGresult, decltype(&PQclear)> err_res(res_ptr.value(), &PQclear);
        auto                                          message = PQresultErrorMessage(err_res.get());
        ILIAS_ERROR("ilias-pgsql", "Query failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected((SqlError::Code)ret);
    }
    int size = PQntuples(res_ptr.value());
    for (int i = 0; i < size; i++) {
        Oid         oid     = std::stoul(PQgetvalue(res_ptr.value(), i, 0));
        std::string typname = PQgetvalue(res_ptr.value(), i, 1);
        mTypeMap[oid]       = typname;
    }
    co_return {};
}

auto Postgres::getTypeMap() -> std::unordered_map<Oid, std::string> & {
    return mTypeMap;
}

auto Postgres::info() -> std::string {
    if (!mConn || PQstatus(mConn) != CONNECTION_OK) {
        return "Not connected";
    }
    return "PostgreSQL server version: " + std::to_string(PQserverVersion(mConn));
}

auto Postgres::connect(std::string_view conninfo) -> IoTask<void> {
    if (mConn) {
        co_return Unexpected(SqlError::Code::AlreadyConnected);
    }

    mConn = PQconnectStart(std::string(conninfo).c_str());
    if (mConn == nullptr) {
        ILIAS_ERROR("ilias-pgsql", "PQconnectStart returned null, possibly out of memory");
        co_return Unexpected(std::make_error_code(std::errc::not_enough_memory));
    }

    if (PQstatus(mConn) == CONNECTION_BAD) {
        ILIAS_ERROR("ilias-pgsql", "Connection failed: {}", PQerrorMessage(mConn));
        PQfinish(mConn);
        mConn = nullptr;
        co_return Unexpected(std::make_error_code(std::errc::connection_aborted));
    }

    if (mCtxt == nullptr) {
        co_return Unexpected(IoError::InvalidArgument);
    }
    auto fd = PQsocket(mConn);
    if (fd < 0) {
        ILIAS_ERROR("ilias-pgsql", "get socket failed");
        co_return Unexpected(IoError::Unknown);
    }
    if (!mPoller || (mPoller.fd() != (fd_t)fd)) {
        auto poller_result = co_await Poller::make((fd_t)fd, IoDescriptor::Socket);
        if (!poller_result) {
            ILIAS_ERROR("ilias-pgsql", "add fd({}) to IoContext failed.", fd);
            co_return Unexpected(IoError::Unknown);
        }
        mPoller = std::move(poller_result.value());
    }

    PostgresPollingStatusType poll_status;
    while ((poll_status = PQconnectPoll(mConn)) != PGRES_POLLING_OK) {
        if (poll_status == PGRES_POLLING_FAILED) {
            ILIAS_ERROR("ilias-pgsql", "Connection poll failed: {}", PQerrorMessage(mConn));
            PQfinish(mConn);
            mConn = nullptr;
            co_return Unexpected(std::make_error_code(std::errc::connection_aborted));
        }

        uint32_t pollEvents = (poll_status == PGRES_POLLING_READING) ? POLLIN : POLLOUT;

        auto poll_ret = co_await mPoller.poll(pollEvents);
        if (!poll_ret) {
            co_return Unexpected(poll_ret.error());
        }
    }

    ILIAS_INFO("ilias-pgsql", "PostgreSQL connection established.");
    if (PQsetnonblocking(mConn, 1) != 0) {
        ILIAS_ERROR("ilias-pgsql", "Failed to set non-blocking mode");
        co_return Unexpected(SqlError::Code::UnknownError);
    }

    auto init_ret = co_await initializeTypeMap();
    if (!init_ret) {
        co_return Unexpected(init_ret.error());
    }

    co_return {};
}

auto Postgres::waitForReadable() -> IoTask<void> {
    auto poll_ret = co_await mPoller.poll(POLLIN);
    if (!poll_ret) {
        co_return Unexpected(poll_ret.error());
    }
    co_return {};
}

auto Postgres::consumeInput() -> IoResult<void> {
    if (!mConn) {
        return Unexpected(SqlError::Code::NotConnected);
    }
    auto ret = PQconsumeInput(mConn);
    if (ret != 1) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQconsumeInput failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        return Unexpected(SqlError::Code(ret));
    }
    return {};
}

auto Postgres::sendQuery(std::string_view sql) -> IoTask<void> {
    auto ret = PQsendQuery(mConn, std::string(sql).c_str());
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendQuery failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Postgres::sendQueryParams(std::string_view command, int nParams, const Oid *paramTypes,
                               const char *const *paramValues, const int *paramLengths, const int *paramFormats,
                               int resultFormat) -> IoTask<void> {
    auto ret = PQsendQueryParams(mConn, std::string(command).c_str(), nParams, paramTypes, paramValues, paramLengths,
                                 paramFormats, resultFormat);
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendQueryParams failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Postgres::sendPrepare(std::string_view stmtName, std::string_view query, int nParams, const Oid *paramTypes)
    -> IoTask<void> {
    auto ret = PQsendPrepare(mConn, std::string(stmtName).c_str(), std::string(query).c_str(), nParams, paramTypes);
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendPrepare failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Postgres::sendQueryPrepared(std::string_view stmtName, int nParams, const char *const *paramValues,
                                 const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void> {
    auto ret = PQsendQueryPrepared(mConn, std::string(stmtName).c_str(), nParams, paramValues, paramLengths,
                                   paramFormats, resultFormat);
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendQueryPrepared failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    co_return {};
}

auto Postgres::getResult() -> IoTask<PGresult *> {
    while (PQisBusy(mConn)) {
        co_await waitForReadable();
        if (auto ret = consumeInput(); !ret) {
            ILIAS_ERROR("ilias-pgsql", "Connection lost while waiting for result: {}", ret.error().message());
            co_return Unexpected(ret.error());
        }
    }

    PGresult *result = PQgetResult(mConn);
    co_return result;
}

// #############################################################################
// #  PostgresResultSet Implementation
// #############################################################################
PostgresResultSet::PostgresResultSet(std::shared_ptr<Postgres> pg, PGresult *result)
    : mPg(std::move(pg)), mResult(result, &PQclear) {
    if (mResult) {
        mTotalRows = PQntuples(mResult.get());
    }
}

PostgresResultSet::~PostgresResultSet() {
    // mResult unique_ptr handles cleanup
}

auto PostgresResultSet::native() -> PGresult * {
    return mResult.get();
}

auto PostgresResultSet::next() -> IoTask<bool> {
    if (mCurrentRow + 1 < mTotalRows) {
        mCurrentRow++;
        co_return true;
    }
    co_return false;
}

auto PostgresResultSet::rowCount() const -> size_t {
    return mTotalRows;
}

auto PostgresResultSet::columnCount() const -> size_t {
    if (!mResult)
        return 0;
    return PQnfields(mResult.get());
}

auto PostgresResultSet::columnName(size_t index) const -> std::string_view {
    if (!mResult || index >= columnCount()) {
        return {};
    }
    return PQfname(mResult.get(), index);
}

auto PostgresResultSet::getValue(size_t index) -> IoResult<SqlValue> {
    if (!mResult || mCurrentRow < 0 || mCurrentRow >= mTotalRows || index >= columnCount()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return toValue(mResult.get(), mCurrentRow, index);
}

auto PostgresResultSet::getValue(std::string_view name) -> IoResult<SqlValue> {
    if (!mResult) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    int colIndex = PQfnumber(mResult.get(), std::string(name).c_str());
    if (colIndex == -1) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return getValue(colIndex);
}

auto PostgresResultSet::toValue(const PGresult *res, int rowIndex, int colIndex) -> IoResult<SqlValue> {
    if (PQgetisnull(res, rowIndex, colIndex)) {
        return SqlValue(SqlNull());
    }

    // 获取原始字符串值和类型OID
    const char *value_str = PQgetvalue(res, rowIndex, colIndex);
    const Oid   type_oid  = PQftype(res, colIndex);

    auto it = mPg->getTypeMap().find(type_oid);
    const std::string_view type_name = (it != mPg->getTypeMap().end()) ? std::string_view(it->second) : "unknown";

    // --- 开始基于类型名称的转换 ---

    if (type_name == "bool") {
        // PostgreSQL 的布尔值字符串是 't' 或 'f'
        return SqlValue(*value_str == 't' ? (char)1 : (char)0);
    }

    // 类别 2: 整数类型
    if (type_name == "int2") { // smallint
        int32_t val;
        if (auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val); ec == std::errc()) {
            return SqlValue(val);
        }
    }
    if (type_name == "int4" || type_name == "oid") { // integer, oid
        int32_t val;
        if (auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val); ec == std::errc()) {
            return SqlValue(val);
        }
    }
    if (type_name == "int8") { // bigint
        int64_t val;
        if (auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val); ec == std::errc()) {
            return SqlValue(val);
        }
    }

    // 类别 3: 浮点数类型
    if (type_name == "float4") { // real
        float val;
        if (auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val); ec == std::errc()) {
            return SqlValue(val);
        }
    }
    if (type_name == "float8") { // double precision
        double val;
        if (auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val); ec == std::errc()) {
            return SqlValue(val);
        }
    }

    // 类别 4: 日期和时间类型
    if (type_name == "date" || type_name == "time" || type_name == "timetz" || type_name == "timestamp" ||
        type_name == "timestamptz") {
        return SqlValue(SqlDate(std::string_view(value_str)));
    }

    // 类别 5: 二进制数据
    if (type_name == "bytea") {
        size_t len;
        // PQunescapeBytea 用于处理 PostgreSQL 特有的十六进制转义格式 (e.g., \xDEADBEEF)
        unsigned char *decoded_data = PQunescapeBytea(reinterpret_cast<const unsigned char *>(value_str), &len);
        if (!decoded_data) {
            ILIAS_ERROR("sql-pgsql", "Failed to unescape bytea data");
            return Unexpected(SqlError::Code::UnknownError);
        }

        // 创建一个 std::vector<std::byte> 并拷贝数据
        SqlBlob blob(len);
        memcpy(blob.data(), decoded_data, len);

        PQfreemem(decoded_data);
        return SqlValue(std::move(blob));
    }

    // 类别 6: 默认处理 - 所有其他类型都作为字符串返回
    // 这包括：text, varchar, char(n), numeric, decimal, json, xml, uuid,
    // 以及所有我们不认识的自定义类型或未知类型。
    // 这是最安全、最健壮的默认行为。
    if (type_name == "numeric" || type_name == "text" || type_name == "varchar" || type_name == "bpchar" ||
        type_name == "json" || type_name == "jsonb" || type_name == "xml" || type_name == "uuid" ||
        type_name == "unknown") {
        return SqlValue(std::string(value_str));
    }

    ILIAS_WARN("sql-pgsql",
               "Failed to convert value '{}' of type '{}' ({}) to a specific type. Falling back to string.", value_str,
               type_name, type_oid);
    return SqlValue(std::string(value_str));
}

// #############################################################################
// #  PostgresStatement Implementation
// #############################################################################

PostgresStatement::PostgresStatement(std::shared_ptr<Postgres> pg) : mPg(std::move(pg)) {
    static std::atomic<uint64_t> counter = 0;
    mStatementName                       = "_ilias_stmt_" + std::to_string(counter++);
}

PostgresStatement::~PostgresStatement() {
    // DEALLOCATE statement on server if connection is still alive
    if (mPg && mPg->native() && PQstatus(mPg->native()) == CONNECTION_OK) {
        std::string dealloc_sql = "DEALLOCATE " + mStatementName;
        PQsendQuery(mPg->native(), dealloc_sql.c_str());
    }
}

auto PostgresStatement::prepare(std::string_view sql) -> IoTask<void> {
    mPreparedSql    = parser(sql);
    auto paramCount = mBindValues.size();

    // Asynchronously send the prepare command
    co_await mPg->sendPrepare(mStatementName, mPreparedSql, paramCount, nullptr); // Let server infer types

    // Wait for the result of the prepare command
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        ILIAS_ERROR("ilias-pgsql", "Prepare failed for '{}': {}", mStatementName, mPg->lastErrorMessage());
        co_return Unexpected(res_ptr.error());
    }
    std::unique_ptr<PGresult, decltype(&PQclear)> result(res_ptr.value(), &PQclear);

    if (!result || PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
        ILIAS_ERROR("ilias-pgsql", "Prepare failed for '{}': {}", mStatementName, mPg->lastErrorMessage());
        co_return Unexpected(SqlError::Code::NotPrepared);
    }
    co_return {};
}

auto PostgresStatement::reset() -> void {
    clearBinds();
}

auto PostgresStatement::bind(size_t index, SqlValuePointer value) -> Result<void, std::error_code> {
    if (index == 0 || index > mBindValues.size()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    mBindValues[index - 1] = value;
    return {};
}

auto PostgresStatement::bind(std::string_view name, SqlValuePointer value) -> Result<void, std::error_code> {
    auto it = mNamedParamIndex.find(std::string(name));
    if (it == mNamedParamIndex.end()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return bind(it->second + 1, value);
}

auto PostgresStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    if (!convertBinds()) {
        co_return Unexpected(SqlError::Code::InvalidParameter);
    }

    co_await mPg->sendQueryPrepared(mStatementName, mParamValuesPtrs.size(), mParamValuesPtrs.data(),
                                    mParamLengths.data(), nullptr, 0); // text format

    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        ILIAS_ERROR("ilias-pgsql", "Statement query failed: {}", mPg->lastErrorMessage());
        co_return Unexpected(res_ptr.error());
    }
    auto status = PQresultStatus(res_ptr.value());
    if ((status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK)) {
        std::unique_ptr<PGresult, decltype(&PQclear)> err_res(res_ptr.value(), &PQclear);
        auto                                          message = PQresultErrorMessage(err_res.get());
        ILIAS_ERROR("ilias-pgsql", "Statement query failed: {}", message);
        SqlErrorCategory::instance().registerMessage(status, message);
        co_return Unexpected((SqlError::Code)status);
    }

    co_return std::make_unique<PostgresResultSet>(mPg, res_ptr.value());
}

auto PostgresStatement::execute() -> IoTask<size_t> {
    auto result_set_wrapper = co_await query();
    if (!result_set_wrapper) {
        co_return Unexpected(result_set_wrapper.error());
    }
    auto  *pg_result         = static_cast<PostgresResultSet *>(result_set_wrapper->get())->native();
    char  *rows_affected_str = PQcmdTuples(pg_result);
    size_t rows_affected     = 0;
    if (rows_affected_str && *rows_affected_str) {
        rows_affected = std::stoull(rows_affected_str);
    }
    co_return rows_affected;
}

auto PostgresStatement::parser(std::string_view sql) -> std::string {
    mNamedParamIndex.clear();
    std::string ret;
    ret.reserve(sql.size());
    int param_counter = 0;

    bool in_string  = false;
    char quote_char = 0;
    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];
        if (in_string) {
            ret += c;
            if (c == quote_char) {
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
        if (c == '\'' || c == '"' || c == '`') {
            in_string  = true;
            quote_char = c;
            ret += c;
            continue;
        }
        if (c == '?') {
            ret += '$';
            ret += std::to_string(++param_counter);
            continue;
        }
        if (c == ':') {
            size_t j = i + 1;
            if (j < sql.size() && std::isalpha(sql[j])) {
                while (j < sql.size() && (std::isalnum(sql[j]) || sql[j] == '_')) {
                    j++;
                }
                std::string name(sql.substr(i + 1, j - (i + 1)));
                mNamedParamIndex[name] = param_counter;
                ret += '$';
                ret += std::to_string(++param_counter);
                i = j - 1;
                continue;
            }
        }
        ret += c;
    }
    mBindValues.resize(param_counter);
    return ret;
}

void PostgresStatement::clearBinds() {
    for (auto &val : mBindValues) {
        val = &g_sql_null;
    }
}

bool PostgresStatement::convertBinds() {
    mParamData.clear();
    mParamValuesPtrs.clear();
    mParamLengths.clear();

    mParamData.reserve(mBindValues.size());
    mParamValuesPtrs.reserve(mBindValues.size());
    mParamLengths.reserve(mBindValues.size());

    for (const auto &val : mBindValues) {
        std::visit(
            [this](auto arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, SqlNull *>) {
                    mParamValuesPtrs.push_back(nullptr);
                    mParamLengths.push_back(0);
                }
                else if constexpr (std::is_same_v<T, std::span<const std::byte>>) {
                    mParamData.emplace_back("");
                    mParamValuesPtrs.push_back(reinterpret_cast<const char *>(arg.data()));
                    mParamLengths.push_back(arg.size());
                }
                else if constexpr (std::is_same_v<T, std::string_view>) {
                    mParamData.push_back(std::string(arg));
                    mParamValuesPtrs.push_back(mParamData.back().c_str());
                    mParamLengths.push_back(mParamData.back().length());
                }
                else if constexpr (std::is_same_v<T, ilias::sql::SqlDate *>) {
                    mParamData.push_back(arg->toString());
                    mParamValuesPtrs.push_back(mParamData.back().c_str());
                    mParamLengths.push_back(mParamData.back().length());
                }
                else {
                    mParamData.push_back(std::to_string(*arg));
                    mParamValuesPtrs.push_back(mParamData.back().c_str());
                    mParamLengths.push_back(mParamData.back().length());
                }
            },
            val);
    }
    return true;
}

// #############################################################################
// #  PostgresConnection Implementation
// #############################################################################
PostgresConnection::PostgresConnection(std::shared_ptr<Postgres> pg, ConnectOptions options)
    : mPg(std::move(pg)), mOptions(std::move(options)) {
}

auto PostgresConnection::sqlname() -> std::string {
    return "PostgreSQL";
}

auto PostgresConnection::sqlinfo() -> std::string {
    return mPg->info();
}

auto PostgresConnection::connect() -> IoTask<void> {
    if (mIsConnected) {
        co_return Unexpected(SqlError::Code::AlreadyConnected);
    }

    std::string conninfo;
    if (!mOptions.host.empty())
        conninfo += "host=" + mOptions.host + " ";
    if (mOptions.port > 0)
        conninfo += "port=" + std::to_string(mOptions.port) + " ";
    if (!mOptions.user.empty())
        conninfo += "user=" + mOptions.user + " ";
    if (!mOptions.password.empty())
        conninfo += "password=" + mOptions.password + " ";
    if (!mOptions.database.empty())
        conninfo += "dbname=" + mOptions.database + " ";
    for (const auto &[key, value] : mOptions.extra) {
        conninfo += key + "=" + value + " ";
    }

    auto result = co_await mPg->connect(conninfo);
    if (!result) {
        co_return Unexpected(result.error());
    }

    mIsConnected = true;
    co_return {};
}

auto PostgresConnection::disconnect() -> IoTask<void> {
    mIsConnected = false;
    mPg->disconnect();
    co_return {};
}

auto PostgresConnection::selectDatabase(std::string_view name) -> IoTask<void> {
    // In PostgreSQL, you can't switch databases on an existing connection.
    // This is a fundamental difference from MySQL. A proper implementation
    // would be to disconnect and reconnect to the new database.
    ILIAS_ERROR("ilias-pgsql", "PostgreSQL does not support changing databases on an active connection.");
    co_return Unexpected(SqlError::Code::UnsupportedApi);
}

auto PostgresConnection::prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> {
    auto stmt        = std::make_unique<PostgresStatement>(mPg);
    auto prep_result = co_await stmt->prepare(sql);
    if (!prep_result) {
        co_return Unexpected(prep_result.error());
    }
    co_return std::move(stmt);
}

auto PostgresConnection::execute(std::string_view sql) -> IoTask<size_t> {
    auto result_set_wrapper = co_await query(sql);
    if (!result_set_wrapper) {
        co_return Unexpected(result_set_wrapper.error());
    }
    auto  *pg_result         = static_cast<PostgresResultSet *>(result_set_wrapper->get())->native();
    char  *rows_affected_str = PQcmdTuples(pg_result);
    size_t rows_affected     = 0;
    if (rows_affected_str && *rows_affected_str) {
        rows_affected = std::stoull(rows_affected_str);
    }
    co_return rows_affected;
}

auto PostgresConnection::query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> {
    ILIAS_TRACE("ilias-pgsql", "exec query {}", sql);
    auto send_result = co_await mPg->sendQuery(sql);
    if (!send_result) {
        co_return Unexpected(send_result.error());
    }

    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        co_return Unexpected(res_ptr.error());
    }
    auto ret = PQresultStatus(res_ptr.value());
    if (ret != PGRES_TUPLES_OK && ret != PGRES_COMMAND_OK) {
        std::unique_ptr<PGresult, decltype(&PQclear)> err_res(res_ptr.value(), &PQclear);
        auto                                          message = PQresultErrorMessage(err_res.get());
        ILIAS_ERROR("ilias-pgsql", "Query failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected((SqlError::Code)ret);
    }

    // Check for RETURNING clause to get last insert id
    auto tuples_result  = PQntuples(res_ptr.value());
    auto defines_result = PQnfields(res_ptr.value());
    if (ret == PGRES_TUPLES_OK && tuples_result > 0 && defines_result > 0) {
        const char *val = PQgetvalue(res_ptr.value(), 0, 0);
        mLastInsertId   = std::stoll(val);
    }

    // Drain any other potential results from the wire for this command
    while (auto extra_res = PQgetResult(mPg->native())) {
        PQclear(extra_res);
    }

    co_return std::make_unique<PostgresResultSet>(mPg, res_ptr.value());
}

auto PostgresConnection::beginTransaction() -> IoTask<bool> {
    auto res = co_await query("BEGIN");
    co_return res.has_value();
}

auto PostgresConnection::commit() -> IoTask<bool> {
    auto res = co_await query("COMMIT");
    co_return res.has_value();
}

auto PostgresConnection::rollback() -> IoTask<bool> {
    auto res = co_await query("ROLLBACK");
    co_return res.has_value();
}

auto PostgresConnection::syncRollback() -> bool {
    // Not truly synchronous, but best effort without a sync API
    if (mPg && mPg->native() && PQstatus(mPg->native()) == CONNECTION_OK) {
        auto *res     = PQexec(mPg->native(), "ROLLBACK");
        bool  success = (res && PQresultStatus(res) == PGRES_COMMAND_OK);
        if (res)
            PQclear(res);
        return success;
    }
    return false;
}

auto PostgresConnection::lastInsertId() const -> int64_t {
    return mLastInsertId;
}

auto PostgresConnection::ping() -> IoTask<bool> {
    auto status = PQstatus(mPg->native());
    co_return (status == CONNECTION_OK);
}

ILIAS_POSTGRES_NS_END

ILIAS_SQL_REGISTER_PLUGIN(postgres) {
    return new ILIAS_POSTGRES_COMPLETE_NAMESPACE::PostgresConnection(
        std::make_shared<ILIAS_POSTGRES_COMPLETE_NAMESPACE::Postgres>(), options);
}