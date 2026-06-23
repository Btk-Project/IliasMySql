#include "ilias/postgres/postgres_impl.hpp"
#include "ilias/postgres/postgres.hpp"
#include "ilias/postgres/postgres_context.hpp"
#include "ilias/postgres/postgres_parsers.hpp"
#include "ilias/sql/detail/placeholder_parser.hpp"
#include <libpq/libpq-fs.h>

#include <atomic>
#include <string>
#include <variant>
#include <memory>

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

// #############################################################################
// #  Error Handling Utilities
// #############################################################################

/**
 * @brief Extract detailed error information from a PGresult using PQresultErrorField
 *
 * This function extracts the error message, SQLSTATE code, and other details
 * from a PostgreSQL error result.
 *
 * @param result The PGresult containing the error
 * @return A formatted error message string
 */
static auto extractDetailedErrorMessage(PGresult *result) -> std::string {
    if (!result) {
        return "Unknown error (null result)";
    }
    std::string message;
    // Get the primary error message
    const char *primary = PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
    if (primary) {
        message = primary;
    }
    else {
        // Fallback to PQresultErrorMessage
        const char *fallback = PQresultErrorMessage(result);
        if (fallback) {
            message = fallback;
        }
    }
    // Get SQLSTATE code (5-character error code)
    const char *sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    if (sqlstate && *sqlstate) {
        message += " [SQLSTATE: ";
        message += sqlstate;
        message += "]";
    }
    // Get detail message if available
    const char *detail = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL);
    if (detail && *detail) {
        message += " Detail: ";
        message += detail;
    }
    // Get hint if available
    const char *hint = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT);
    if (hint && *hint) {
        message += " Hint: ";
        message += hint;
    }
    return message;
}

/**
 * @brief Map PostgreSQL SQLSTATE codes to SqlError codes
 *
 * PostgreSQL SQLSTATE codes follow the SQL standard:
 * - Class 00: Successful Completion
 * - Class 23: Integrity Constraint Violation
 * - Class 42: Syntax Error or Access Rule Violation
 * - etc.
 *
 * @param result The PGresult containing the error
 * @return The appropriate SqlError::Code
 */
static auto mapPostgresErrorToSqlError(PGresult *result) -> SqlError::Code {
    if (!result) {
        return SqlError::Code::UnknownError;
    }
    const char *sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    if (!sqlstate || !*sqlstate) {
        // No SQLSTATE available, use generic error
        return SqlError::Code::UnknownError;
    }
    std::string_view state(sqlstate, 5);
    std::string_view errorClass = state.substr(0, 2);

    // Class 23: Integrity Constraint Violation
    if (errorClass == "23") {
        if (state == "23505") {
            // unique_violation
            return SqlError::Code::UniqueConstraintViolation;
        }
        if (state == "23503") {
            // foreign_key_violation
            return SqlError::Code::ForeignKeyViolation;
        }
        if (state == "23502") {
            // not_null_violation
            return SqlError::Code::NotNullViolation;
        }
        if (state == "23514") {
            // check_violation
            return SqlError::Code::CheckConstraintViolation;
        }
        if (state == "23P01") {
            // exclusion_violation
            return SqlError::Code::ConstraintViolation;
        }
        // Generic constraint violation
        return SqlError::Code::ConstraintViolation;
    }

    // Class 42: Syntax Error or Access Rule Violation
    if (errorClass == "42") {
        if (state == "42P01") {
            // undefined_table
            return SqlError::Code::TableNotFound;
        }
        if (state == "42703") {
            // undefined_column
            return SqlError::Code::ColumnNotFound;
        }
        // Generic syntax/access error
        return SqlError::Code::InvalidSqlStatement;
    }
    // Class 22: Data Exception
    if (errorClass == "22") {
        if (state == "22003") {
            // numeric_value_out_of_range
            return SqlError::Code::NumericValueOutOfRange;
        }
        if (state == "22001") {
            // string_data_right_truncation
            return SqlError::Code::DataTruncated;
        }
        // Generic data exception
        return SqlError::Code::InvalidDataFormat;
    }
    // Class 08: Connection Exception
    if (errorClass == "08") {
        return SqlError::Code::NotConnected;
    }
    // Default to unknown error
    return SqlError::Code::UnknownError;
}

static auto makePostgresNativeError(PGresult *result) -> NativeSqlError {
    NativeSqlError error;
    error.backend = "postgresql";
    if (!result) {
        error.code    = static_cast<int>(PGRES_FATAL_ERROR);
        error.message = "Unknown error (null result)";
        return error;
    }
    error.code = static_cast<int>(PQresultStatus(result));
    if (const char *sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE); sqlstate) {
        error.sqlstate = sqlstate;
    }
    error.message = extractDetailedErrorMessage(result);
    return error;
}

static auto makePostgresNativeError(int ret, PGconn *conn) -> NativeSqlError {
    NativeSqlError error;
    error.backend = "postgresql";
    error.code    = ret;
    if (conn) {
        if (const char *message = PQerrorMessage(conn); message) {
            error.message = message;
        }
    }
    return error;
}

/**
 * @brief Map a PostgreSQL error to the portable SqlError code.
 *
 * @return The appropriate SqlError::Code
 */
static auto registerPostgresError(PGresult *result) -> SqlError::Code {
    return mapPostgresErrorToSqlError(result);
}

static auto registerPostgresError(int ret, PGconn *conn) -> SqlError::Code {
    if (conn && PQstatus(conn) == CONNECTION_BAD) {
        return SqlError::Code::NotConnected;
    }
    return ret == 0 ? SqlError::Code::UnknownError : SqlError::Code::UnknownError;
}

template <typename Fn>
static auto withBlockingConnection(PGconn *conn, Fn &&fn) -> void {
    if (!conn) {
        return;
    }

    const int wasNonblocking = PQisnonblocking(conn);
    if (wasNonblocking == 1) {
        if (int ret = PQsetnonblocking(conn, 0); ret != 0) {
            auto code = registerPostgresError(ret, conn);
            ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code),
                        std::error_code(code).message());
            return;
        }
    }

    fn();

    if (wasNonblocking == 1) {
        if (int ret = PQsetnonblocking(conn, 1); ret != 0) {
            auto code = registerPostgresError(ret, conn);
            ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code),
                        std::error_code(code).message());
        }
    }
}

static auto drainPendingResultsBlocking(PGconn *conn) -> void {
    withBlockingConnection(conn, [conn]() {
        while (auto *res = PQgetResult(conn)) {
            PQclear(res);
        }
    });
}

// #############################################################################
// #  Postgres (Low-Level Wrapper) Implementation
// #############################################################################

Postgres::Postgres() {
    mCtxt = IoContext::currentThread();
    if (mCtxt == nullptr) {
        ILIAS_ERROR("ilias-pgsql", "no io context in current thread");
    }
    // 初始化PostgresValueConverterContext
    mContext = std::make_shared<PostgresValueConverterContext>();
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
        // Drain pending async results with libpq's blocking API before close.
        drainPendingResultsBlocking(mConn);

        // Close the poller before finishing the connection
        mPoller.close();
        PQfinish(mConn);
        mConn = nullptr;
    }
}

auto Postgres::disconnectAsync() -> IoTask<void> {
    if (!mConn) {
        co_return {};
    }

    // Drain any pending results from the connection asynchronously
    // This ensures we don't leave the connection in a bad state
    while (true) {
        // Wait for the connection to be ready (not busy)
        while (PQisBusy(mConn)) {
            auto wait_ret = co_await waitForReadable();
            if (!wait_ret) {
                // If we can't wait, just break and clean up
                break;
            }
            if (auto consume_ret = consumeInput(); !consume_ret) {
                break;
            }
        }

        PGresult *res = PQgetResult(mConn);
        if (res == nullptr)
            break;
        PQclear(res);
    }

    // Close the poller before finishing the connection
    mPoller.close();
    PQfinish(mConn);
    mConn = nullptr;

    co_return {};
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

auto Postgres::setLastNativeError(NativeSqlError error) -> void {
    mLastNativeError = std::move(error);
}

auto Postgres::lastNativeError() const -> std::optional<NativeSqlError> {
    return mLastNativeError;
}

// 在 connect() 成功后调用
auto Postgres::initializeTypeMap() -> IoTask<void> {
    // 这个查询获取了基本类型的OID和它们的名称
    const char *query_sql =
        "SELECT oid, typname FROM pg_type WHERE typtype = 'b' AND typcategory IN ('B', 'N', 'S', 'T', 'U', 'V', 'X');";

    auto send_result = co_await sendQuery(query_sql);
    if (!send_result) {
        co_return Err(send_result.error());
    }

    auto res_ptr = co_await getResult();
    if (!res_ptr) {
        co_return Err(res_ptr.error());
    }

    // Use unique_ptr to ensure PQclear is called
    std::unique_ptr<PGresult, decltype(&PQclear)> result(res_ptr.value(), &PQclear);

    auto ret = PQresultStatus(result.get());
    if (ret != PGRES_TUPLES_OK && ret != PGRES_COMMAND_OK) {
        mLastNativeError = makePostgresNativeError(result.get());
        auto code = registerPostgresError(result.get());
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }

    int size = PQntuples(result.get());
    for (int i = 0; i < size; i++) {
        Oid         oid     = std::stoul(PQgetvalue(result.get(), i, 0));
        std::string typname = PQgetvalue(result.get(), i, 1);
        mTypeMap[oid]       = typname;
        // ILIAS_TRACE("ilias-pgsql", "Postgres type {} -> {}", oid, typname);
    }

    // Drain any remaining results from the connection asynchronously
    while (true) {
        auto extra_res_ptr = co_await getResult();
        if (!extra_res_ptr || extra_res_ptr.value() == nullptr) {
            break;
        }
        PQclear(extra_res_ptr.value());
    }

    co_return {};
}

auto Postgres::getTypeMap() -> std::map<Oid, std::string> & {
    return mTypeMap;
}

auto Postgres::valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> {
    return mContext;
}

auto Postgres::info() -> std::string {
    if (!mConn || PQstatus(mConn) != CONNECTION_OK) {
        return "Not connected";
    }
    return "PostgreSQL server version: " + std::to_string(PQserverVersion(mConn));
}

auto Postgres::connect(std::string_view conninfo) -> IoTask<void> {
    if (mConn) {
        co_return Err(SqlError::Code::AlreadyConnected);
    }

    mConn = PQconnectStart(std::string(conninfo).c_str());
    if (mConn == nullptr) {
        ILIAS_ERROR("ilias-pgsql", "PQconnectStart returned null, possibly out of memory");
        co_return Err(std::make_error_code(std::errc::not_enough_memory));
    }

    if (PQstatus(mConn) == CONNECTION_BAD) {
        mLastNativeError = makePostgresNativeError(CONNECTION_BAD, mConn);
        auto code = registerPostgresError(CONNECTION_BAD, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        PQfinish(mConn);
        mConn = nullptr;
        co_return Err(code);
    }

    if (mCtxt == nullptr) {
        co_return Err(IoError::InvalidArgument);
    }
    auto fd = PQsocket(mConn);
    if (fd < 0) {
        ILIAS_ERROR("ilias-pgsql", "get socket failed");
        co_return Err(IoError::Unknown);
    }
    if (!mPoller || (mPoller.fd() != (fd_t)fd)) {
        auto poller_result = co_await Poller::make((fd_t)fd, IoDescriptor::Socket);
        if (!poller_result) {
            ILIAS_ERROR("ilias-pgsql", "add fd({}) to IoContext failed.", fd);
            co_return Err(poller_result.error());
        }
        mPoller = std::move(poller_result.value());
    }

    PostgresPollingStatusType poll_status;
    while ((poll_status = PQconnectPoll(mConn)) != PGRES_POLLING_OK) {
        if (poll_status == PGRES_POLLING_FAILED) {
            mLastNativeError = makePostgresNativeError(PGRES_POLLING_FAILED, mConn);
            auto code = registerPostgresError(PGRES_POLLING_FAILED, mConn);
            ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code),
                        std::error_code(code).message());
            PQfinish(mConn);
            mConn = nullptr;
            co_return Err(code);
        }

        uint32_t pollEvents = (poll_status == PGRES_POLLING_READING) ? POLLIN : POLLOUT;

        auto poll_ret = co_await mPoller.poll(pollEvents);
        if (!poll_ret) {
            co_return Err(poll_ret.error());
        }
    }

    ILIAS_TRACE("ilias-pgsql", "PostgreSQL connection established.");
    if (int ret = PQsetnonblocking(mConn, 1); ret != 0) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        auto code = registerPostgresError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }

    auto init_ret = co_await initializeTypeMap();
    if (!init_ret) {
        co_return Err(init_ret.error());
    }

    // 注册PostgreSQL类型解析器和绑定器
    auto pg_context = std::static_pointer_cast<PostgresValueConverterContext>(mContext);
    registerPostgresTypeParsers(*pg_context);

    // 设置类型映射到PostgresValueConverterContext
    pg_context->setTypeMap(mTypeMap);

    co_return {};
}

auto Postgres::waitForReadable() -> IoTask<void> {
    if (!mPoller) {
        co_return Err(IoError::Unknown);
    }
    auto poll_ret = co_await mPoller.poll(POLLIN);
    if (!poll_ret) {
        co_return Err(poll_ret.error());
    }
    co_return {};
}

auto Postgres::waitForWritable() -> IoTask<void> {
    if (!mPoller) {
        co_return Err(IoError::SocketIsNotConnected);
    }
    auto poll_ret = co_await mPoller.poll(POLLOUT);
    if (!poll_ret) {
        co_return Err(poll_ret.error());
    }
    co_return {};
}

auto Postgres::flushOutput() -> IoTask<void> {
    if (!mConn) {
        co_return Err(SqlError::Code::NotConnected);
    }
    while (true) {
        int ret = PQflush(mConn);
        if (ret == 0) {
            co_return {};
        }
        if (ret == -1) {
            mLastNativeError = makePostgresNativeError(ret, mConn);
            auto code = registerPostgresError(ret, mConn);
            ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code),
                        std::error_code(code).message());
            co_return Err(code);
        }
        // ret == 1: Need to wait for socket to be writable and try again
        auto wait_ret = co_await waitForWritable();
        if (!wait_ret) {
            co_return Err(wait_ret.error());
        }
    }
}

auto Postgres::consumeInput() -> IoResult<void> {
    if (!mConn) {
        return Err(SqlError::Code::NotConnected);
    }
    auto ret = PQconsumeInput(mConn);
    if (ret != 1) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        auto code = registerPostgresError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        return Err(code);
    }
    return {};
}

auto Postgres::sendQuery(std::string_view sql) -> IoTask<void> {
    ILIAS_TRACE("ilias-pgsql", "Executing SQL: {}", sql);
    if (!mConn) {
        co_return Err(SqlError::Code::NotConnected);
    }
    auto ret = PQsendQuery(mConn, std::string(sql).c_str());
    if (ret == 0) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        auto code = registerPostgresError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }
    // Flush the output buffer to ensure the query is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Err(flush_ret.error());
    }
    co_return {};
}

auto Postgres::sendQueryParams(std::string_view command, int nParams, const Oid *paramTypes,
                               const char *const *paramValues, const int *paramLengths, const int *paramFormats,
                               int resultFormat) -> IoTask<void> {
    ILIAS_TRACE("ilias-pgsql", "Executing SQL with {} params: {}", nParams, command);
    if (!mConn) {
        co_return Err(SqlError::Code::NotConnected);
    }
    auto ret = PQsendQueryParams(mConn, std::string(command).c_str(), nParams, paramTypes, paramValues, paramLengths,
                                 paramFormats, resultFormat);
    if (ret == 0) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        auto code = registerPostgresError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }
    // Flush the output buffer to ensure the query is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Err(flush_ret.error());
    }

    co_return {};
}

auto Postgres::sendPrepare(std::string_view stmtName, std::string_view query, int nParams, const Oid *paramTypes)
    -> IoTask<void> {
    ILIAS_TRACE("ilias-pgsql", "Preparing statement '{}' with {} params: {}", stmtName, nParams, query);
    if (!mConn) {
        co_return Err(SqlError::Code::NotConnected);
    }
    auto ret = PQsendPrepare(mConn, std::string(stmtName).c_str(), std::string(query).c_str(), nParams, paramTypes);
    if (ret == 0) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        auto code = registerPostgresError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }
    // Flush the output buffer to ensure the prepare command is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Err(flush_ret.error());
    }
    co_return {};
}

auto Postgres::sendQueryPrepared(std::string_view stmtName, int nParams, const char *const *paramValues,
                                 const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void> {
    ILIAS_TRACE("ilias-pgsql", "Executing prepared statement '{}' with {} params", stmtName, nParams);
    if (!mConn) {
        co_return Err(SqlError::Code::NotConnected);
    }
    auto ret = PQsendQueryPrepared(mConn, std::string(stmtName).c_str(), nParams, paramValues, paramLengths,
                                   paramFormats, resultFormat);
    if (ret == 0) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        auto code = registerPostgresError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }
    // Flush the output buffer to ensure the query is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Err(flush_ret.error());
    }
    co_return {};
}

auto Postgres::setSingleRowMode() -> bool {
    if (!mConn) {
        ILIAS_ERROR("ilias-pgsql", "setSingleRowMode called with no connection");
        return false;
    }
    int ret = PQsetSingleRowMode(mConn);
    if (ret == 0) {
        mLastNativeError = makePostgresNativeError(ret, mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsetSingleRowMode failed: {}", PQerrorMessage(mConn));
        return false;
    }
    return true;
}

auto Postgres::getResult() -> IoTask<PGresult *> {
    while (PQisBusy(mConn)) {
        co_await waitForReadable();
        if (auto ret = consumeInput(); !ret) {
            ILIAS_ERROR("ilias-pgsql", "Connection lost while waiting for result: {}", ret.error().message());
            co_return Err(ret.error());
        }
    }
    PGresult *result = PQgetResult(mConn);
    co_return result;
}

// #############################################################################
// #  PostgresStreamingResultSet Implementation (Streaming mode)
// #############################################################################
PostgresStreamingResultSet::PostgresStreamingResultSet(std::shared_ptr<Postgres> pg) : mPg(std::move(pg)) {
}

PostgresStreamingResultSet::~PostgresStreamingResultSet() {
    if (mPg && mPg->native() && PQstatus(mPg->native()) == CONNECTION_OK) {
        drainPendingResultsBlocking(mPg->native());
    }
}

auto PostgresStreamingResultSet::drainRemainingResults() -> IoTask<void> {
    while (!mEndOfResults) {
        auto res_ptr = co_await mPg->getResult();
        if (!res_ptr) {
            mEndOfResults = true;
            break;
        }
        PGresult *result = res_ptr.value();
        if (result == nullptr) {
            mEndOfResults = true;
            break;
        }
        PQclear(result);
    }
    co_return {};
}

auto PostgresStreamingResultSet::initColumnMetadata() -> void {
    if (mMetadataInitialized || !mCurrentRow) {
        return;
    }
    mColumnCount = PQnfields(mCurrentRow.get());
    mColumnNames.clear();
    mColumnNames.reserve(mColumnCount);
    mColumnIndex.clear();
    for (int i = 0; i < mColumnCount; ++i) {
        const char *name = PQfname(mCurrentRow.get(), i);
        mColumnNames.emplace_back(name ? name : "");
        mColumnIndex[mColumnNames.back()] = i;
    }
    ILIAS_TRACE("ilias-pgsql", "Initialized column metadata with {} columns.", mColumnCount);
    mMetadataInitialized = true;
}

auto PostgresStreamingResultSet::next() -> IoTask<bool> {
    if (mIsResultFetched == true) {
        mIsResultFetched = false;
        ++mRowsFetched;
        co_return true;
    }
    mCurrentRow.reset(nullptr);
    if (mEndOfResults) {
        ILIAS_TRACE("ilias-pgsql", "No more results to fetch.");
        co_return false;
    }
    // Fetch the next row
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        co_return Err(res_ptr.error());
    }
    PGresult *result = res_ptr.value();
    if (result == nullptr) {
        ILIAS_TRACE("ilias-pgsql", "No more results to fetch.");
        mEndOfResults = true;
        co_return false;
    }
    auto status = PQresultStatus(result);
    if (status == PGRES_SINGLE_TUPLE) {
        ILIAS_TRACE("ilias-pgsql", "Fetched a new row.");
        mCurrentRow.reset(result);
        mRowsFetched++;
        if (!mMetadataInitialized) {
            initColumnMetadata();
        }
        co_return true;
    }
    else if (status == PGRES_TUPLES_OK) {
        PQclear(result);
        co_await drainRemainingResults();
        co_return false;
    }
    else if (status == PGRES_COMMAND_OK) {
        char *cnt = PQcmdTuples(result);
        if (cnt && *cnt) {
            size_t affected = 0;
            auto   ret      = std::from_chars(cnt, cnt + strlen(cnt), affected);
            if (ret.ec != std::errc()) {
                ILIAS_TRACE("ilias-pgsql", "Failed to parse rows affected: {}", std::make_error_code(ret.ec).message());
            }
            else {
                mRowsAffected = affected;
            }
        }
        PQclear(result);
        co_await drainRemainingResults();
        co_return false;
    }
    else {
        mLastNativeError = makePostgresNativeError(result);
        if (mPg) {
            mPg->setLastNativeError(*mLastNativeError);
        }
        auto code = registerPostgresError(result);
        PQclear(result);
        co_await drainRemainingResults();
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }
}

auto PostgresStreamingResultSet::capabilities() const -> ResultCapabilities {
    return ResultCapabilities {
        .streaming     = true,
        .exactRowCount = false,
        .rowsAffected  = mRowsAffected.has_value(),
    };
}

auto PostgresStreamingResultSet::rowsFetched() const -> size_t {
    return mRowsFetched;
}

auto PostgresStreamingResultSet::exactRowCount() const -> std::optional<size_t> {
    return std::nullopt;
}

auto PostgresStreamingResultSet::rowsAffected() const -> std::optional<size_t> {
    return mRowsAffected;
}

auto PostgresStreamingResultSet::lastNativeError() const -> std::optional<NativeSqlError> {
    if (mLastNativeError) {
        return mLastNativeError;
    }
    return mPg ? mPg->lastNativeError() : std::nullopt;
}

auto PostgresStreamingResultSet::columnCount() const -> size_t {
    return mColumnCount;
}

auto PostgresStreamingResultSet::columnName(size_t index) const -> std::string_view {
    if (index >= mColumnNames.size()) {
        return {};
    }
    return mColumnNames[index];
}

auto PostgresStreamingResultSet::getValue(size_t index) -> IoResult<SqlCellView> {
    if (!mCurrentRow || index >= static_cast<size_t>(mColumnCount)) {
        return Err(SqlError::Code::InvalidIndex);
    }
    return toValueView(static_cast<int>(index));
}

auto PostgresStreamingResultSet::getValue(std::string_view name) -> IoResult<SqlCellView> {
    if (!mCurrentRow) {
        return Err(SqlError::Code::NoMoreData);
    }

    auto it = mColumnIndex.find(std::string(name));
    if (it == mColumnIndex.end()) {
        return Err(SqlError::Code::InvalidIndex);
    }
    return toValueView(it->second);
}

auto PostgresStreamingResultSet::getResultForQuery() -> IoTask<void> {
    if (mIsResultFetched) {
        ILIAS_TRACE("ilias-pgsql", "Result already fetched.");
        co_return {};
    }
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        co_return Err(res_ptr.error());
    }
    PGresult *result = res_ptr.value();
    if (result == nullptr) {
        mEndOfResults = true;
        co_return {}; // 正常结束
    }

    auto status = PQresultStatus(result);
    if (status == PGRES_FATAL_ERROR) {
        mLastNativeError = makePostgresNativeError(result);
        if (mPg) {
            mPg->setLastNativeError(*mLastNativeError);
        }
        auto code     = registerPostgresError(result);
        mEndOfResults = true;
        PQclear(result); // 清理结果
        co_return Err(code);
    }

    // 只要执行成功，就记录当前结果，并初始化元数据
    mCurrentRow.reset(result);
    initColumnMetadata();
    if (status == PGRES_SINGLE_TUPLE) {
        mIsResultFetched = true; // 标记第一行已就绪
    }
    else {
        // 如果是 TUPLES_OK 或 COMMAND_OK，处理逻辑同 next
        if (status == PGRES_COMMAND_OK) {
            char *cnt = PQcmdTuples(result);
            if (cnt && *cnt) {
                size_t affected = 0;
                auto   ret      = std::from_chars(cnt, cnt + strlen(cnt), affected);
                if (ret.ec != std::errc()) {
                    ILIAS_TRACE("ilias-pgsql", "Failed to parse rows affected: {}",
                                std::make_error_code(ret.ec).message());
                }
                else {
                    mRowsAffected = affected;
                }
            }
        }
        co_await drainRemainingResults();
    }
    co_return {};
}

auto PostgresStreamingResultSet::toValueView(int colIndex) -> IoResult<SqlCellView> {
    if (!mCurrentRow) {
        ILIAS_TRACE("ilias-pgsql", "No current row available.");
        return Err(SqlError::Code::NoMoreData);
    }
    const int rowIndex = 0;
    if (PQgetisnull(mCurrentRow.get(), rowIndex, colIndex)) {
        return SqlCellView(mPg->valueConverterContext());
    }
    // 获取原始数据
    const char *value_str = PQgetvalue(mCurrentRow.get(), rowIndex, colIndex);
    const int   value_len = PQgetlength(mCurrentRow.get(), rowIndex, colIndex);
    const Oid   type_oid  = PQftype(mCurrentRow.get(), colIndex);
    const int   format    = PQfformat(mCurrentRow.get(), colIndex); // 0=文本，1=二进制

    // 创建PostgreSQL特定的元数据
    auto meta_storage    = std::make_shared<PostgresCellMetadata>();
    meta_storage->oid    = type_oid;
    meta_storage->data   = value_str;
    meta_storage->size   = value_len;
    meta_storage->pgconn = mPg->native();
    meta_storage->format = format;

    // 存储元数据，确保在SqlCellView使用期间有效
    bindStorage(meta_storage);

    ILIAS_TRACE("ilias-pgsql", "Converting column {} of oid {} with format {}", colIndex, type_oid, format);

    return SqlCellView(mPg->valueConverterContext(), meta_storage.get(), sizeof(PostgresCellMetadata),
                       std::type_index(typeid(PostgresCellMetadata)), colIndex);
}

// #############################################################################
// #  PostgresStatement Implementation
// #############################################################################

PostgresStatement::PostgresStatement(std::shared_ptr<Postgres> pg)
    : mPg(std::move(pg)), mParamValuesPtrs(nullptr, 0, 1, 0) {
    static std::atomic<uint64_t> counter = 0;
    mParamValuesPtrs.set_column_names({"Data Pointer", "Data Length", "Data Format"});
    mStatementName = std::shared_ptr<std::string>(new std::string("_ilias_stmt_" + std::to_string(counter++)),
                                                  [pg](std::string *name) {
                                                      deallocStatementName(*name, pg);
                                                      delete name;
                                                  });
}

void PostgresStatement::deallocStatementName(const std::string &name, std::shared_ptr<Postgres> mPg) {
    // DEALLOCATE statement on server if connection is still alive
    if (mPg && mPg->native() && PQstatus(mPg->native()) == CONNECTION_OK) {
        PGconn *conn = mPg->native();
        withBlockingConnection(conn, [&]() {
            // Drain pending async results first, then execute DEALLOCATE.
            while (auto *pending = PQgetResult(conn)) {
                PQclear(pending);
            }

            std::string dealloc_sql = "DEALLOCATE " + name;
            PGresult   *res         = PQexec(conn, dealloc_sql.c_str());
            if (res) {
                if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                    ILIAS_ERROR("ilias-pgsql", "DEALLOCATE failed: {}", PQresultErrorMessage(res));
                }
                PQclear(res);
            }
            else {
                ILIAS_ERROR("ilias-pgsql", "DEALLOCATE failed: {}", PQerrorMessage(conn));
            }
        });
    }
}

PostgresStatement::~PostgresStatement() {
}

auto PostgresStatement::prepare(std::string_view sql) -> IoTask<void> {
    ILIAS_TRACE("ilias-pgsql", "Preparing statement with SQL: {}", sql);
    mPreparedSql = parser(sql);
    mPrepared    = false; // 清除准备状态
    co_return {};
}

auto PostgresStatement::ensurePrepared() -> IoTask<void> {
    if (mPrepared) {
        co_return {}; // 已经准备好了，直接返回
    }
    // 获取参数数量
    auto paramCount = mParamValuesPtrs.column_size();
    // Asynchronously send the prepare command
    auto send_ret = co_await mPg->sendPrepare(*mStatementName, mPreparedSql, paramCount,
                                              mParamValuesPtrs.get_column<3>().data()); // Let server infer types
    if (!send_ret) {
        mLastNativeError = mPg ? mPg->lastNativeError() : std::nullopt;
        ILIAS_ERROR("ilias-pgsql", "sendPrepare failed for '{}': {}", *mStatementName, mPg->lastErrorMessage());
        co_return Err(send_ret.error());
    }
    // Wait for the result of the prepare command
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        mLastNativeError = mPg ? mPg->lastNativeError() : std::nullopt;
        ILIAS_ERROR("ilias-pgsql", "Prepare failed for '{}': {}", *mStatementName, mPg->lastErrorMessage());
        co_return Err(res_ptr.error());
    }
    std::unique_ptr<PGresult, decltype(&PQclear)> result(res_ptr.value(), &PQclear);
    if (!result || PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
        mLastNativeError = makePostgresNativeError(result.get());
        if (mPg) {
            mPg->setLastNativeError(*mLastNativeError);
        }
        auto code = registerPostgresError(result.get());
        ILIAS_ERROR("ilias-pgsql", "Postgres error {}: {}", static_cast<int>(code), std::error_code(code).message());
        co_return Err(code);
    }
    // Drain any remaining results from the connection asynchronously
    while (true) {
        auto extra_res_ptr = co_await mPg->getResult();
        if (!extra_res_ptr || extra_res_ptr.value() == nullptr) {
            break;
        }
        PQclear(extra_res_ptr.value());
    }
    mPrepared = true;
    co_return {};
}

auto PostgresStatement::reset() -> void {
    auto size = mParamValuesPtrs.column_size();
    clearBinds();
    mParamValuesPtrs.resize(size);
}

auto PostgresStatement::nativeHandle() const -> void * {
    return mPg->native();
}

auto PostgresStatement::lastNativeError() const -> std::optional<NativeSqlError> {
    if (mLastNativeError) {
        return mLastNativeError;
    }
    return mPg ? mPg->lastNativeError() : std::nullopt;
}

auto PostgresStatement::bind(std::type_index type_index, size_t index, const SqlCellView &value)
    -> Result<void, std::error_code> {
    if (index == 0 || index > mParamValuesPtrs.column_size()) {
        return Err(SqlError::Code::InvalidIndex);
    }

    // 使用PostgresValueConverterContext的绑定器
    auto pg_context = std::static_pointer_cast<PostgresValueConverterContext>(mPg->valueConverterContext());
    auto binder     = pg_context->findTypeBinder(type_index);
    if (binder) {
        // 创建新的SqlCellView，包含索引信息
        SqlCellView cell_view(pg_context, value.raw_value(), value.raw_value_size(), value.raw_type(), index);

        ILIAS_TRACE("ilias-pgsql", " bind type {} index {} size {}", type_index, index, value.raw_value_size());
        // 调用绑定器，传递this指针作为data参数
        auto store = binder(cell_view, std::any(this));
        if (store) {
            if (*store) {
                mDataGuards.emplace_back(std::move(store.value()));
            }
        }
        else {
            return Err(store.error());
        }
        return {};
    }
    ILIAS_WARN("ilias-pgsql", "type {} is not supported bind", type_index);
    return Err(SqlError::Code::UnsupportBindType);
}

auto PostgresStatement::bind(std::type_index type_index, std::string_view name, const SqlCellView &value)
    -> Result<void, std::error_code> {
    auto it = mNamedParamIndex.find(std::string(name));
    if (it == mNamedParamIndex.end()) {
        return Err(SqlError::Code::InvalidIndex);
    }
    return bind(type_index, it->second + 1, value);
}

auto PostgresStatement::query() -> IoTask<std::unique_ptr<IResultSet>> {
    auto prepare = co_await ensurePrepared();
    if (!prepare) {
        mLastNativeError = lastNativeError();
        co_return Err(prepare.error());
    }
    ILIAS_TRACE("ilias-pgsql", "Executing query on prepared statement '{}': {}", *mStatementName, mPreparedSql);
    auto send_ret = co_await mPg->sendQueryPrepared(
        *mStatementName, mParamValuesPtrs.column_size(),
        reinterpret_cast<const char *const *>(mParamValuesPtrs.get_column<0>().data()),
        mParamValuesPtrs.get_column<1>().data(), mParamValuesPtrs.get_column<2>().data(), 1); // binary format
    if (!send_ret) {
        mLastNativeError = mPg ? mPg->lastNativeError() : std::nullopt;
        ILIAS_ERROR("ilias-pgsql", "Statement sendQueryPrepared failed: {}", mPg->lastErrorMessage());
        co_return Err(send_ret.error());
    }
    // Enable single-row mode for streaming results (Requirement 4.4)
    if (!mPg->setSingleRowMode()) {
        mLastNativeError = mPg ? mPg->lastNativeError() : std::nullopt;
        ILIAS_ERROR("ilias-pgsql", "Failed to enable single-row mode for prepared statement");
        co_return Err(SqlError::Code::UnknownError);
    }
    // Return streaming result set that fetches rows one at a time
    auto result_set = std::make_unique<PostgresStreamingResultSet>(mPg);
    result_set->bindStorage(mStatementName);
    auto ret = co_await result_set->getResultForQuery();
    if (!ret) {
        mLastNativeError = result_set->lastNativeError();
        co_return Err(ret.error());
    }
    co_return std::move(result_set);
}

auto PostgresStatement::execute() -> IoTask<size_t> {
    ILIAS_TRACE("ilias-pgsql", "Executing prepared statement '{}': {}", *mStatementName, mPreparedSql);
    auto result_set_wrapper = co_await query();
    if (!result_set_wrapper) {
        co_return Err(result_set_wrapper.error());
    }
    auto result_set = std::move(result_set_wrapper.value());
    auto rset       = dynamic_cast<PostgresStreamingResultSet *>(result_set.get());
    while (rset) {
        auto next_ret = co_await rset->next();
        if (!next_ret) {
            co_return Err(next_ret.error());
        }
        if (!next_ret.value()) {
            co_return rset->rowsAffected().value_or(0);
        }
    }
    co_return 0;
}

auto PostgresStatement::parser(std::string_view sql) -> std::string {
    auto parsed = sql::detail::rewrite_sql_placeholders(sql, sql::detail::SqlPlaceholderDialect::Postgres,
                                                        sql::detail::SqlPlaceholderRewriteStyle::PostgresNumbered);
    mNamedParamIndex.clear();
    for (const auto &[name, index] : parsed.named_param_indices) {
        mNamedParamIndex[name] = static_cast<int>(index);
    }

    // 初始化参数缓冲区
    mParamValuesPtrs.clear();
    mParamValuesPtrs.resize(parsed.parameter_count);

    return parsed.sql;
}

void PostgresStatement::clearBinds() {
    mParamValuesPtrs.clear();
    mDataGuards.clear();
}

void PostgresStatement::setBindParam(size_t index, const void *data, size_t size, int format, Oid type_oid) {
    // 确保mParamData足够大
    if (index > mParamValuesPtrs.column_size()) {
        mParamValuesPtrs.resize(index);
    }

    // 存储指针和大小
    mParamValuesPtrs[index - 1] = std::tuple {data, size, format, type_oid};
}

Oid PostgresStatement::getTypeOid(std::string_view name) const {
    auto &type_map = mPg->getTypeMap();
    for (auto &type : type_map) {
        if (type.second == name) {
            return type.first;
        }
    }
    return 0; // 未知类型
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
        co_return Err(SqlError::Code::AlreadyConnected);
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
        co_return Err(result.error());
    }

    mIsConnected = true;
    co_return {};
}

auto PostgresConnection::disconnect() -> IoTask<void> {
    mIsConnected = false;
    co_await mPg->disconnectAsync();
    co_return {};
}

auto PostgresConnection::selectDatabase([[maybe_unused]] std::string_view name) -> IoTask<void> {
    ILIAS_ERROR("ilias-pgsql", "PostgreSQL does not support changing databases on an active connection.");
    co_return Err(SqlError::Code::UnsupportedApi);
}

auto PostgresConnection::prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> {
    auto stmt        = std::make_unique<PostgresStatement>(mPg);
    auto prep_result = co_await stmt->prepare(sql);
    if (!prep_result) {
        co_return Err(prep_result.error());
    }
    co_return std::move(stmt);
}

auto PostgresConnection::execute(std::string_view sql) -> IoTask<size_t> {
    auto result_set_wrapper = co_await query(sql);
    if (!result_set_wrapper) {
        co_return Err(result_set_wrapper.error());
    }
    auto result_set = std::move(result_set_wrapper.value());
    auto rset       = dynamic_cast<PostgresStreamingResultSet *>(result_set.get());
    while (rset) {
        auto next_ret = co_await rset->next();
        if (!next_ret) {
            if (auto native = rset->lastNativeError(); native && mPg) {
                mPg->setLastNativeError(*native);
            }
            co_return Err(next_ret.error());
        }
        if (!next_ret.value()) {
            co_return rset->rowsAffected().value_or(0);
        }
    }
    co_return 0;
}

auto PostgresConnection::query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> {
    ILIAS_TRACE("ilias-pgsql", "exec query {}", sql);
    auto send_result = co_await mPg->sendQuery(sql);
    if (!send_result) {
        co_return Err(send_result.error());
    }
    if (!mPg->setSingleRowMode()) {
        ILIAS_ERROR("ilias-pgsql", "Failed to enable single-row mode for prepared statement");
        co_return Err(SqlError::Code::UnknownError);
    }
    auto result = std::make_unique<PostgresStreamingResultSet>(mPg);
    auto ret    = co_await result->getResultForQuery();
    if (!ret) {
        if (auto native = result->lastNativeError(); native && mPg) {
            mPg->setLastNativeError(*native);
        }
        co_return Err(ret.error());
    }
    co_return std::move(result);
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
    if (!mPg || !mPg->native()) {
        co_return false;
    }

    // Send a simple query to verify the connection is actually working
    auto send_result = co_await mPg->sendQuery("SELECT 1");
    if (!send_result) {
        co_return false;
    }

    // Get the result and check if it succeeded
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        co_return false;
    }

    std::unique_ptr<PGresult, decltype(&PQclear)> result(res_ptr.value(), &PQclear);
    auto                                          status = PQresultStatus(result.get());

    // Drain any remaining results
    while (auto extra_res = PQgetResult(mPg->native())) {
        PQclear(extra_res);
    }

    co_return (status == PGRES_TUPLES_OK);
}

auto PostgresConnection::valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> {
    return mPg->valueConverterContext();
}

auto PostgresConnection::nativeHandle() const -> void * {
    return mPg->native();
}

auto PostgresConnection::lastNativeError() const -> std::optional<NativeSqlError> {
    return mPg ? mPg->lastNativeError() : std::nullopt;
}

ILIAS_POSTGRES_NS_END

ILIAS_SQL_REGISTER_PLUGIN(postgres) {
    return new ILIAS_POSTGRES_COMPLETE_NAMESPACE::PostgresConnection(
        std::make_shared<ILIAS_POSTGRES_COMPLETE_NAMESPACE::Postgres>(), options);
}
