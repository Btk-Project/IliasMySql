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
 * 
 * **Validates: Requirements 7.1, 7.2, 7.3**
 */
static auto extractDetailedErrorMessage(PGresult* result) -> std::string {
    if (!result) {
        return "Unknown error (null result)";
    }
    
    std::string message;
    
    // Get the primary error message
    const char* primary = PQresultErrorField(result, PG_DIAG_MESSAGE_PRIMARY);
    if (primary) {
        message = primary;
    } else {
        // Fallback to PQresultErrorMessage
        const char* fallback = PQresultErrorMessage(result);
        if (fallback) {
            message = fallback;
        }
    }
    
    // Get SQLSTATE code (5-character error code)
    const char* sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    if (sqlstate && *sqlstate) {
        message += " [SQLSTATE: ";
        message += sqlstate;
        message += "]";
    }
    
    // Get detail message if available
    const char* detail = PQresultErrorField(result, PG_DIAG_MESSAGE_DETAIL);
    if (detail && *detail) {
        message += " Detail: ";
        message += detail;
    }
    
    // Get hint if available
    const char* hint = PQresultErrorField(result, PG_DIAG_MESSAGE_HINT);
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
 * 
 * **Validates: Requirements 7.2**
 */
static auto mapPostgresErrorToSqlError(PGresult* result) -> SqlError::Code {
    if (!result) {
        return SqlError::Code::UnknownError;
    }
    
    const char* sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE);
    if (!sqlstate || !*sqlstate) {
        // No SQLSTATE available, use generic error
        return SqlError::Code::UnknownError;
    }
    
    // SQLSTATE is a 5-character code
    // First two characters indicate the class
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
        return SqlError::Code::UnknownError;
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
        return SqlError::Code::UnknownError;
    }
    
    // Class 08: Connection Exception
    if (errorClass == "08") {
        return SqlError::Code::NotConnected;
    }
    
    // Default to unknown error
    return SqlError::Code::UnknownError;
}

/**
 * @brief Register a PostgreSQL error with the SqlErrorCategory
 * 
 * This function extracts detailed error information and registers it
 * with the error category for later retrieval.
 * 
 * @param result The PGresult containing the error
 * @return The appropriate SqlError::Code
 * 
 * **Validates: Requirements 7.1, 7.2, 7.3**
 */
static auto registerPostgresError(PGresult* result) -> SqlError::Code {
    auto errorCode = mapPostgresErrorToSqlError(result);
    auto message = extractDetailedErrorMessage(result);
    
    // Register the detailed message with the error category
    SqlErrorCategory::instance().registerMessage(static_cast<int>(errorCode), message);
    
    return errorCode;
}

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
        // First, drain any pending results from the connection synchronously
        // This is necessary to avoid leaving the connection in a bad state
        PQconsumeInput(mConn);
        while (true) {
            // Wait for the connection to be ready (not busy)
            while (PQisBusy(mConn)) {
                int sock = PQsocket(mConn);
                if (sock < 0) break;
                
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLIN;
                pfd.revents = 0;
                
                int ret = poll(&pfd, 1, 1000);  // 1 second timeout
                if (ret <= 0) break;  // Timeout or error
                
                PQconsumeInput(mConn);
            }
            
            PGresult* res = PQgetResult(mConn);
            if (res == nullptr) break;
            PQclear(res);
        }
        
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
        
        PGresult* res = PQgetResult(mConn);
        if (res == nullptr) break;
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
    
    // Use unique_ptr to ensure PQclear is called
    std::unique_ptr<PGresult, decltype(&PQclear)> result(res_ptr.value(), &PQclear);
    
    auto ret = PQresultStatus(result.get());
    if (ret != PGRES_TUPLES_OK && ret != PGRES_COMMAND_OK) {
        // Use detailed error extraction (Requirement 7.1, 7.2, 7.3)
        auto errorCode = registerPostgresError(result.get());
        auto message = extractDetailedErrorMessage(result.get());
        ILIAS_ERROR("ilias-pgsql", "Query failed: {}", message);
        co_return Unexpected(errorCode);
    }
    
    int size = PQntuples(result.get());
    for (int i = 0; i < size; i++) {
        Oid         oid     = std::stoul(PQgetvalue(result.get(), i, 0));
        std::string typname = PQgetvalue(result.get(), i, 1);
        mTypeMap[oid]       = typname;
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
    if (!mPoller) {
        co_return Unexpected(IoError::Unknown);
    }
    auto poll_ret = co_await mPoller.poll(POLLIN);
    if (!poll_ret) {
        co_return Unexpected(poll_ret.error());
    }
    co_return {};
}

auto Postgres::waitForWritable() -> IoTask<void> {
    if (!mPoller) {
        co_return Unexpected(IoError::Unknown);
    }
    auto poll_ret = co_await mPoller.poll(POLLOUT);
    if (!poll_ret) {
        co_return Unexpected(poll_ret.error());
    }
    co_return {};
}

auto Postgres::flushOutput() -> IoTask<void> {
    if (!mConn) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    
    // PQflush returns:
    // 0 if successful (or if the send queue is empty)
    // -1 if it failed for some reason
    // 1 if it was unable to send all the data in the send queue yet
    //   (this case can only occur if the connection is nonblocking)
    while (true) {
        int ret = PQflush(mConn);
        if (ret == 0) {
            // All data flushed successfully
            co_return {};
        }
        if (ret == -1) {
            // Error occurred
            auto message = PQerrorMessage(mConn);
            ILIAS_ERROR("ilias-pgsql", "PQflush failed: {}", message);
            co_return Unexpected(SqlError::Code::UnknownError);
        }
        // ret == 1: Need to wait for socket to be writable and try again
        auto wait_ret = co_await waitForWritable();
        if (!wait_ret) {
            co_return Unexpected(wait_ret.error());
        }
    }
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
    if (!mConn) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    
    auto ret = PQsendQuery(mConn, std::string(sql).c_str());
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendQuery failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    
    // Flush the output buffer to ensure the query is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Unexpected(flush_ret.error());
    }
    
    co_return {};
}

auto Postgres::sendQueryParams(std::string_view command, int nParams, const Oid *paramTypes,
                               const char *const *paramValues, const int *paramLengths, const int *paramFormats,
                               int resultFormat) -> IoTask<void> {
    if (!mConn) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    
    auto ret = PQsendQueryParams(mConn, std::string(command).c_str(), nParams, paramTypes, paramValues, paramLengths,
                                 paramFormats, resultFormat);
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendQueryParams failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    
    // Flush the output buffer to ensure the query is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Unexpected(flush_ret.error());
    }
    
    co_return {};
}

auto Postgres::sendPrepare(std::string_view stmtName, std::string_view query, int nParams, const Oid *paramTypes)
    -> IoTask<void> {
    if (!mConn) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    
    auto ret = PQsendPrepare(mConn, std::string(stmtName).c_str(), std::string(query).c_str(), nParams, paramTypes);
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendPrepare failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    
    // Flush the output buffer to ensure the prepare command is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Unexpected(flush_ret.error());
    }
    
    co_return {};
}

auto Postgres::sendQueryPrepared(std::string_view stmtName, int nParams, const char *const *paramValues,
                                 const int *paramLengths, const int *paramFormats, int resultFormat) -> IoTask<void> {
    if (!mConn) {
        co_return Unexpected(SqlError::Code::NotConnected);
    }
    
    auto ret = PQsendQueryPrepared(mConn, std::string(stmtName).c_str(), nParams, paramValues, paramLengths,
                                   paramFormats, resultFormat);
    if (ret == 0) {
        auto message = PQerrorMessage(mConn);
        ILIAS_ERROR("ilias-pgsql", "PQsendQueryPrepared failed: {}", message);
        SqlErrorCategory::instance().registerMessage(ret, message);
        co_return Unexpected(SqlError::Code(ret));
    }
    
    // Flush the output buffer to ensure the query is sent to the server
    auto flush_ret = co_await flushOutput();
    if (!flush_ret) {
        co_return Unexpected(flush_ret.error());
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

auto PostgresResultSet::getValue(size_t index) -> IoResult<SqlValueView> {
    if (!mResult || mCurrentRow < 0 || mCurrentRow >= mTotalRows || index >= columnCount()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return toValueView(mResult.get(), mCurrentRow, index);
}

auto PostgresResultSet::getValue(std::string_view name) -> IoResult<SqlValueView> {
    if (!mResult) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    int colIndex = PQfnumber(mResult.get(), std::string(name).c_str());
    if (colIndex == -1) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return getValue(colIndex);
}

// Helper function to decode PostgreSQL bytea hex format (\xDEADBEEF)
static auto decodeByteaHex(const char* value_str, int len) -> std::pair<const std::byte*, size_t> {
    // PostgreSQL bytea in hex format starts with \x
    // The actual binary data follows after the \x prefix
    // Note: PQgetvalue returns the raw text representation, and PQgetlength returns its length
    // For hex format, we need to decode it, but since we're returning a view,
    // we can't decode in place. The caller should use PQunescapeBytea for owned data.
    // For now, return the raw bytes as-is (the hex-encoded string)
    return {reinterpret_cast<const std::byte*>(value_str), static_cast<size_t>(len)};
}

// Helper function to parse SqlDate with appropriate TimeType
static auto parseSqlDate(std::string_view value_str, std::string_view type_name) -> SqlDate {
    SqlDate date(value_str);
    
    // Set the appropriate TimeType based on PostgreSQL type
    if (type_name == "date") {
        date.setTimeType(SqlDate::kDate);
    }
    else if (type_name == "time" || type_name == "timetz") {
        date.setTimeType(SqlDate::kTime);
    }
    else if (type_name == "timestamp" || type_name == "timestamptz") {
        date.setTimeType(SqlDate::kDateTime);
    }
    
    return date;
}

auto PostgresResultSet::toValueView(const PGresult *res, int rowIndex, int colIndex) -> IoResult<SqlValueView> {
    // Handle NULL values first (Requirement 2.7)
    if (PQgetisnull(res, rowIndex, colIndex)) {
        return SqlValueView(SqlNull());
    }

    // Get raw string value and type OID
    const char *value_str = PQgetvalue(res, rowIndex, colIndex);
    const Oid   type_oid  = PQftype(res, colIndex);

    auto it = mPg->getTypeMap().find(type_oid);
    const std::string_view type_name = (it != mPg->getTypeMap().end()) ? std::string_view(it->second) : "unknown";

    // --- Type conversion based on type name ---

    // Category 1: Boolean type (Requirement 2.4)
    if (type_name == "bool") {
        // PostgreSQL boolean strings are 't' or 'f'
        return SqlValueView(*value_str == 't' ? true : false);
    }

    // Category 2: Integer types (Requirement 2.2)
    if (type_name == "int2") { // smallint
        int32_t val = 0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }
    if (type_name == "int4" || type_name == "oid") { // integer, oid
        int32_t val = 0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }
    if (type_name == "int8") { // bigint
        int64_t val = 0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }

    // Category 3: Floating point types (Requirement 2.3)
    if (type_name == "float4") { // real
        float val = 0.0f;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }
    if (type_name == "float8") { // double precision
        double val = 0.0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }

    // Category 4: Date and time types (Requirement 2.6)
    if (type_name == "date" || type_name == "time" || type_name == "timetz" || 
        type_name == "timestamp" || type_name == "timestamptz") {
        return SqlValueView(parseSqlDate(value_str, type_name));
    }

    // Category 5: Binary data (Requirement 2.5)
    if (type_name == "bytea") {
        int len = PQgetlength(res, rowIndex, colIndex);
        // PostgreSQL returns bytea in hex format (\xDEADBEEF) or escape format
        // We return the raw bytes as a span - the data is owned by PGresult
        // Note: For proper hex decoding, use PQunescapeBytea, but that requires
        // memory allocation. For view semantics, we return the raw representation.
        auto [data, size] = decodeByteaHex(value_str, len);
        return SqlValueView(std::span<const std::byte>(data, size));
    }

    // Category 6: Default handling - all other types returned as string view
    // This includes: text, varchar, char(n), numeric, decimal, json, xml, uuid,
    // and all custom or unknown types.
    // Return string_view - data is owned by PGresult
    return SqlValueView(std::string_view(value_str));
}

// #############################################################################
// #  PostgresStreamingResultSet Implementation (Streaming mode)
// #############################################################################
PostgresStreamingResultSet::PostgresStreamingResultSet(std::shared_ptr<Postgres> pg)
    : mPg(std::move(pg)) {
}

PostgresStreamingResultSet::~PostgresStreamingResultSet() {
    // If we haven't finished iterating, drain remaining results to clean up the connection
    // This is critical to avoid "another command is already in progress" errors
    if (mPg && mPg->native() && PQstatus(mPg->native()) == CONNECTION_OK) {
        PGconn* conn = mPg->native();
        
        // First, consume any pending input
        PQconsumeInput(conn);
        
        // Then drain all results - we must do this even if mEndOfResults is true
        // because there might be a final NULL result we haven't fetched yet
        while (true) {
            // Wait for the connection to be ready (not busy)
            while (PQisBusy(conn)) {
                // In destructor, we can't use async I/O, so we do a blocking wait
                // by polling the socket
                int sock = PQsocket(conn);
                if (sock < 0) break;
                
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLIN;
                pfd.revents = 0;
                
                int ret = poll(&pfd, 1, 1000);  // 1 second timeout
                if (ret <= 0) break;  // Timeout or error
                
                PQconsumeInput(conn);
            }
            
            PGresult* res = PQgetResult(conn);
            if (res == nullptr) break;
            PQclear(res);
        }
    }
}

auto PostgresStreamingResultSet::drainRemainingResults() -> IoTask<void> {
    while (!mEndOfResults) {
        auto res_ptr = co_await mPg->getResult();
        if (!res_ptr) {
            mEndOfResults = true;
            break;
        }
        
        PGresult* result = res_ptr.value();
        if (result == nullptr) {
            mEndOfResults = true;
            break;
        }
        
        auto status = PQresultStatus(result);
        PQclear(result);
        
        if (status == PGRES_TUPLES_OK || status == PGRES_COMMAND_OK) {
            mEndOfResults = true;
            break;
        }
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
        const char* name = PQfname(mCurrentRow.get(), i);
        mColumnNames.emplace_back(name ? name : "");
        mColumnIndex[mColumnNames.back()] = i;
    }
    
    mMetadataInitialized = true;
}

auto PostgresStreamingResultSet::next() -> IoTask<bool> {
    if (mEndOfResults) {
        co_return false;
    }
    
    // Clear the previous row's result
    mCurrentRow.reset();
    
    // Fetch the next row
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        mEndOfResults = true;
        co_return false;
    }
    
    PGresult* result = res_ptr.value();
    if (result == nullptr) {
        // No more results
        mEndOfResults = true;
        co_return false;
    }
    
    auto status = PQresultStatus(result);
    
    if (status == PGRES_SINGLE_TUPLE) {
        // We have a row
        mCurrentRow.reset(result);
        mRowsFetched++;
        
        // Initialize column metadata on first row
        if (!mMetadataInitialized) {
            initColumnMetadata();
        }
        
        co_return true;
    }
    else if (status == PGRES_TUPLES_OK) {
        // End of results marker (no more rows)
        // In single-row mode, PGRES_TUPLES_OK signals the end of the result set
        // We need to fetch the final NULL result to fully drain the connection
        PQclear(result);
        
        // Drain the final NULL result
        auto final_res = co_await mPg->getResult();
        if (final_res && final_res.value() != nullptr) {
            PQclear(final_res.value());
        }
        
        mEndOfResults = true;
        co_return false;
    }
    else if (status == PGRES_COMMAND_OK) {
        // Command completed (for non-SELECT queries)
        // We need to fetch the final NULL result to fully drain the connection
        PQclear(result);
        
        // Drain the final NULL result
        auto final_res = co_await mPg->getResult();
        if (final_res && final_res.value() != nullptr) {
            PQclear(final_res.value());
        }
        
        mEndOfResults = true;
        co_return false;
    }
    else if (status == PGRES_FATAL_ERROR) {
        // Error occurred - use detailed error extraction (Requirement 7.1, 7.2, 7.3)
        auto message = extractDetailedErrorMessage(result);
        ILIAS_ERROR("ilias-pgsql", "Streaming query error: {}", message);
        PQclear(result);
        mEndOfResults = true;
        co_return false;
    }
    else {
        // Unexpected status
        ILIAS_ERROR("ilias-pgsql", "Unexpected result status in streaming mode: {}", static_cast<int>(status));
        PQclear(result);
        mEndOfResults = true;
        co_return false;
    }
}

auto PostgresStreamingResultSet::rowCount() const -> size_t {
    // In streaming mode, we don't know the total row count upfront
    // Return the number of rows fetched so far
    return mRowsFetched;
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

auto PostgresStreamingResultSet::getValue(size_t index) -> IoResult<SqlValueView> {
    if (!mCurrentRow || index >= static_cast<size_t>(mColumnCount)) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return toValueView(static_cast<int>(index));
}

auto PostgresStreamingResultSet::getValue(std::string_view name) -> IoResult<SqlValueView> {
    if (!mCurrentRow) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    
    auto it = mColumnIndex.find(std::string(name));
    if (it == mColumnIndex.end()) {
        return Unexpected(SqlError::Code::InvalidIndex);
    }
    return toValueView(it->second);
}

auto PostgresStreamingResultSet::toValueView(int colIndex) -> IoResult<SqlValueView> {
    if (!mCurrentRow) {
        return Unexpected(SqlError::Code::NoMoreData);
    }
    
    // Row index is always 0 in single-row mode since each PGresult contains exactly one row
    const int rowIndex = 0;
    
    // Handle NULL values first (Requirement 2.7)
    if (PQgetisnull(mCurrentRow.get(), rowIndex, colIndex)) {
        return SqlValueView(SqlNull());
    }

    // Get raw string value and type OID
    const char *value_str = PQgetvalue(mCurrentRow.get(), rowIndex, colIndex);
    const Oid   type_oid  = PQftype(mCurrentRow.get(), colIndex);

    auto it = mPg->getTypeMap().find(type_oid);
    const std::string_view type_name = (it != mPg->getTypeMap().end()) ? std::string_view(it->second) : "unknown";

    // --- Type conversion based on type name ---

    // Category 1: Boolean type (Requirement 2.4)
    if (type_name == "bool") {
        // PostgreSQL boolean strings are 't' or 'f'
        return SqlValueView(*value_str == 't' ? true : false);
    }

    // Category 2: Integer types (Requirement 2.2)
    if (type_name == "int2") { // smallint
        int32_t val = 0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }
    if (type_name == "int4" || type_name == "oid") { // integer, oid
        int32_t val = 0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }
    if (type_name == "int8") { // bigint
        int64_t val = 0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }

    // Category 3: Floating point types (Requirement 2.3)
    if (type_name == "float4") { // real
        float val = 0.0f;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }
    if (type_name == "float8") { // double precision
        double val = 0.0;
        auto [p, ec] = std::from_chars(value_str, value_str + strlen(value_str), val);
        if (ec == std::errc()) {
            return SqlValueView(val);
        }
        // Fallback to string if parsing fails
        return SqlValueView(std::string_view(value_str));
    }

    // Category 4: Date and time types (Requirement 2.6)
    if (type_name == "date" || type_name == "time" || type_name == "timetz" || 
        type_name == "timestamp" || type_name == "timestamptz") {
        return SqlValueView(parseSqlDate(value_str, type_name));
    }

    // Category 5: Binary data (Requirement 2.5)
    if (type_name == "bytea") {
        int len = PQgetlength(mCurrentRow.get(), rowIndex, colIndex);
        // PostgreSQL returns bytea in hex format (\xDEADBEEF) or escape format
        // We return the raw bytes as a span - the data is owned by PGresult
        auto [data, size] = decodeByteaHex(value_str, len);
        return SqlValueView(std::span<const std::byte>(data, size));
    }

    // Category 6: Default handling - all other types returned as string view
    // This includes: text, varchar, char(n), numeric, decimal, json, xml, uuid,
    // and all custom or unknown types.
    // Return string_view - data is owned by PGresult
    return SqlValueView(std::string_view(value_str));
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
        PGconn* conn = mPg->native();
        
        // First, drain any pending results from the connection
        // This is necessary because PQexec doesn't work when there are pending async results
        PQconsumeInput(conn);
        while (true) {
            while (PQisBusy(conn)) {
                int sock = PQsocket(conn);
                if (sock < 0) break;
                
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLIN;
                pfd.revents = 0;
                
                int ret = poll(&pfd, 1, 1000);  // 1 second timeout
                if (ret <= 0) break;
                
                PQconsumeInput(conn);
            }
            
            PGresult* res = PQgetResult(conn);
            if (res == nullptr) break;
            PQclear(res);
        }
        
        // Now we can safely execute the DEALLOCATE command
        std::string dealloc_sql = "DEALLOCATE " + mStatementName;
        PGresult* res = PQexec(conn, dealloc_sql.c_str());
        if (res) {
            if (PQresultStatus(res) != PGRES_COMMAND_OK) {
                ILIAS_ERROR("ilias-pgsql", "DEALLOCATE failed: {}", PQresultErrorMessage(res));
            }
            PQclear(res);
        } else {
            ILIAS_ERROR("ilias-pgsql", "DEALLOCATE failed: {}", PQerrorMessage(conn));
        }
    }
}

auto PostgresStatement::prepare(std::string_view sql) -> IoTask<void> {
    mPreparedSql    = parser(sql);
    auto paramCount = mBindValues.size();

    // Asynchronously send the prepare command
    auto send_ret = co_await mPg->sendPrepare(mStatementName, mPreparedSql, paramCount, nullptr); // Let server infer types
    if (!send_ret) {
        ILIAS_ERROR("ilias-pgsql", "sendPrepare failed for '{}': {}", mStatementName, mPg->lastErrorMessage());
        co_return Unexpected(send_ret.error());
    }

    // Wait for the result of the prepare command
    auto res_ptr = co_await mPg->getResult();
    if (!res_ptr) {
        ILIAS_ERROR("ilias-pgsql", "Prepare failed for '{}': {}", mStatementName, mPg->lastErrorMessage());
        co_return Unexpected(res_ptr.error());
    }
    std::unique_ptr<PGresult, decltype(&PQclear)> result(res_ptr.value(), &PQclear);

    if (!result || PQresultStatus(result.get()) != PGRES_COMMAND_OK) {
        // Use detailed error extraction (Requirement 7.1, 7.2, 7.3)
        auto errorCode = registerPostgresError(result.get());
        auto message = extractDetailedErrorMessage(result.get());
        ILIAS_ERROR("ilias-pgsql", "Prepare failed for '{}': {}", mStatementName, message);
        co_return Unexpected(errorCode);
    }
    
    // Drain any remaining results from the connection asynchronously
    while (true) {
        auto extra_res_ptr = co_await mPg->getResult();
        if (!extra_res_ptr || extra_res_ptr.value() == nullptr) {
            break;
        }
        PQclear(extra_res_ptr.value());
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

    auto send_ret = co_await mPg->sendQueryPrepared(mStatementName, mParamValuesPtrs.size(), mParamValuesPtrs.data(),
                                    mParamLengths.data(), nullptr, 0); // text format
    if (!send_ret) {
        ILIAS_ERROR("ilias-pgsql", "Statement sendQueryPrepared failed: {}", mPg->lastErrorMessage());
        co_return Unexpected(send_ret.error());
    }

    // Enable single-row mode for streaming results (Requirement 4.4)
    if (!mPg->setSingleRowMode()) {
        ILIAS_ERROR("ilias-pgsql", "Failed to enable single-row mode for prepared statement");
        co_return Unexpected(SqlError::Code::UnknownError);
    }

    // Return streaming result set that fetches rows one at a time
    co_return std::make_unique<PostgresStreamingResultSet>(mPg);
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

    enum class State {
        Normal,
        InString,           // Inside '...', "...", or `...`
        InLineComment,      // Inside -- comment
        InBlockComment,     // Inside /* ... */
        InDollarQuote       // Inside $$...$$ or $tag$...$tag$
    };

    State state = State::Normal;
    char quote_char = 0;
    std::string dollar_tag;  // For dollar-quoted strings

    for (size_t i = 0; i < sql.size(); ++i) {
        char c = sql[i];

        switch (state) {
        case State::Normal:
            // Check for line comment start (--)
            if (c == '-' && i + 1 < sql.size() && sql[i + 1] == '-') {
                ret += c;
                ret += sql[i + 1];
                i++;
                state = State::InLineComment;
                continue;
            }
            // Check for block comment start (/*)
            if (c == '/' && i + 1 < sql.size() && sql[i + 1] == '*') {
                ret += c;
                ret += sql[i + 1];
                i++;
                state = State::InBlockComment;
                continue;
            }
            // Check for dollar-quoted string start ($$ or $tag$)
            if (c == '$') {
                // Look for the closing $ to determine the tag
                size_t j = i + 1;
                while (j < sql.size() && (std::isalnum(sql[j]) || sql[j] == '_')) {
                    j++;
                }
                if (j < sql.size() && sql[j] == '$') {
                    // Found a dollar quote: $tag$ or $$
                    dollar_tag = std::string(sql.substr(i, j - i + 1));
                    ret += dollar_tag;
                    i = j;
                    state = State::InDollarQuote;
                    continue;
                }
                // Not a dollar quote, just a regular $ character
                ret += c;
                continue;
            }
            // Check for regular string start
            if (c == '\'' || c == '"' || c == '`') {
                state = State::InString;
                quote_char = c;
                ret += c;
                continue;
            }
            // Check for ? placeholder
            if (c == '?') {
                ret += '$';
                ret += std::to_string(++param_counter);
                continue;
            }
            // Check for :name placeholder (but not :: type cast)
            if (c == ':') {
                // Check for PostgreSQL type cast syntax (::)
                if (i + 1 < sql.size() && sql[i + 1] == ':') {
                    // This is a type cast (::), pass through both colons
                    ret += c;
                    ret += sql[i + 1];
                    i++;
                    continue;
                }
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
            break;

        case State::InString:
            ret += c;
            if (c == quote_char) {
                // Check for escaped quote (doubled quote character)
                if (i + 1 < sql.size() && sql[i + 1] == quote_char) {
                    ret += sql[i + 1];
                    i++;
                }
                else {
                    state = State::Normal;
                }
            }
            break;

        case State::InLineComment:
            ret += c;
            // Line comment ends at newline
            if (c == '\n') {
                state = State::Normal;
            }
            break;

        case State::InBlockComment:
            ret += c;
            // Block comment ends at */
            if (c == '*' && i + 1 < sql.size() && sql[i + 1] == '/') {
                ret += sql[i + 1];
                i++;
                state = State::Normal;
            }
            break;

        case State::InDollarQuote:
            ret += c;
            // Check if we're at the end of the dollar-quoted string
            if (c == '$') {
                // Check if this matches our opening tag
                size_t tag_len = dollar_tag.size();
                if (i + 1 >= tag_len) {
                    std::string_view potential_end = sql.substr(i - tag_len + 1, tag_len);
                    if (potential_end == dollar_tag) {
                        state = State::Normal;
                        dollar_tag.clear();
                    }
                }
            }
            break;
        }
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
                    mParamData.push_back(arg->toUTCString());
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
    co_await mPg->disconnectAsync();
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
        
        // Use detailed error extraction (Requirement 7.1, 7.2, 7.3)
        auto errorCode = registerPostgresError(err_res.get());
        auto message = extractDetailedErrorMessage(err_res.get());
        ILIAS_ERROR("ilias-pgsql", "Query failed: {}", message);
        
        co_return Unexpected(errorCode);
    }

    // Check for RETURNING clause to get last insert id
    // Only attempt to parse if the first column is an integer type
    auto tuples_result  = PQntuples(res_ptr.value());
    auto defines_result = PQnfields(res_ptr.value());
    if (ret == PGRES_TUPLES_OK && tuples_result > 0 && defines_result > 0) {
        // Check if first column is NULL
        if (!PQgetisnull(res_ptr.value(), 0, 0)) {
            // Check if first column is an integer type (int2=21, int4=23, int8=20, oid=26)
            Oid first_col_type = PQftype(res_ptr.value(), 0);
            if (first_col_type == 20 || first_col_type == 21 || first_col_type == 23 || first_col_type == 26) {
                const char *val = PQgetvalue(res_ptr.value(), 0, 0);
                if (val && *val) {
                    mLastInsertId = std::stoll(val);
                }
            }
        }
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
    auto status = PQresultStatus(result.get());
    
    // Drain any remaining results
    while (auto extra_res = PQgetResult(mPg->native())) {
        PQclear(extra_res);
    }
    
    co_return (status == PGRES_TUPLES_OK);
}

ILIAS_POSTGRES_NS_END

ILIAS_SQL_REGISTER_PLUGIN(postgres) {
    return new ILIAS_POSTGRES_COMPLETE_NAMESPACE::PostgresConnection(
        std::make_shared<ILIAS_POSTGRES_COMPLETE_NAMESPACE::Postgres>(), options);
}