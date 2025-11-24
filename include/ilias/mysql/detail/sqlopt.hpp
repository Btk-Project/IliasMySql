#pragma once

#include "global.hpp"
#include "../sqlerror.hpp"

#include <mariadb/mysql.h>
#include <ilias/io/error.hpp>

ILIAS_MYSQL_NS_BEGIN
#define MYSQL_OPTION_TABLE                                                                                             \
    MYSQL_OPTION_ROW(InitCommand, MYSQL_INIT_COMMAND, std::string)                                                     \
    MYSQL_OPTION_ROW(ConnectTimeout, MYSQL_OPT_CONNECT_TIMEOUT, unsigned int *)                                        \
    MYSQL_OPTION_ROW(ProgressCallback, MYSQL_PROGRESS_CALLBACK,                                                        \
                     void (*)(const MYSQL *, unsigned int, unsigned int, double, const char *, unsigned int))          \
    MYSQL_OPTION_ROW(Reconnect, MYSQL_OPT_RECONNECT, bool *)                                                           \
    MYSQL_OPTION_ROW(ReadTimeout, MYSQL_OPT_READ_TIMEOUT, unsigned int *)   /* s */                                    \
    MYSQL_OPTION_ROW(WriteTimeout, MYSQL_OPT_WRITE_TIMEOUT, unsigned int *) /* s */                                    \
    MYSQL_OPTION_ROW(ReportDataTruncation, MYSQL_REPORT_DATA_TRUNCATION, std::string)                                  \
    MYSQL_OPTION_ROW(Compress, MYSQL_OPT_COMPRESS, std::string)                                                        \
    MYSQL_OPTION_ROW(NamedPipe, MYSQL_OPT_NAMED_PIPE, std::string)                                                     \
    MYSQL_OPTION_ROW(SetCharsetDir, MYSQL_SET_CHARSET_DIR, std::string)                                                \
    MYSQL_OPTION_ROW(SetCharsetName, MYSQL_SET_CHARSET_NAME, std::string)                                              \
    MYSQL_OPTION_ROW(LocalInfile, MYSQL_OPT_LOCAL_INFILE, unsigned int *) /* pointer */                                \
    MYSQL_OPTION_ROW(Protocol, MYSQL_OPT_PROTOCOL, mysql_protocol_type)                                                \
    MYSQL_OPTION_ROW(SharedMemoryBaseName, MYSQL_SHARED_MEMORY_BASE_NAME, std::string)                                 \
    MYSQL_OPTION_ROW(SslVerifyServerCert, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, bool *)                                    \
    MYSQL_OPTION_ROW(Bind, MYSQL_OPT_BIND, std::string)                                                                \
    MYSQL_OPTION_ROW(SslKey, MYSQL_OPT_SSL_KEY, std::string)                                                           \
    MYSQL_OPTION_ROW(SslCert, MYSQL_OPT_SSL_CERT, std::string)                                                         \
    MYSQL_OPTION_ROW(SslCa, MYSQL_OPT_SSL_CA, std::string)                                                             \
    MYSQL_OPTION_ROW(SslCapath, MYSQL_OPT_SSL_CAPATH, std::string)                                                     \
    MYSQL_OPTION_ROW(SslCipher, MYSQL_OPT_SSL_CIPHER, std::string)                                                     \
    MYSQL_OPTION_ROW(SslCrl, MYSQL_OPT_SSL_CRL, std::string)                                                           \
    MYSQL_OPTION_ROW(SslCrlPath, MYSQL_OPT_SSL_CRLPATH, std::string)                                                   \
    MYSQL_OPTION_ROW(CanHandleExpiredPasswords, MYSQL_OPT_CAN_HANDLE_EXPIRED_PASSWORDS, bool *)                        \
    MYSQL_OPTION_ROW(SslEnforce, MYSQL_OPT_SSL_ENFORCE, bool *)                                                        \
    MYSQL_OPTION_ROW(MaxAllowedPacket, MYSQL_OPT_MAX_ALLOWED_PACKET, unsigned long *)                                  \
    MYSQL_OPTION_ROW(NetBufferLength, MYSQL_OPT_NET_BUFFER_LENGTH, unsigned long *)                                    \
    MYSQL_OPTION_ROW(Nonblock, MYSQL_OPT_NONBLOCK, int)                                                                \
    MYSQL_OPTION_ROW(SslFp, MARIADB_OPT_SSL_FP, std::string)                                                           \
    MYSQL_OPTION_ROW(SslFpList, MARIADB_OPT_SSL_FP_LIST, std::string)                                                  \
    MYSQL_OPTION_ROW(TlsPassphrase, MARIADB_OPT_TLS_PASSPHRASE, std::string)                                           \
    MYSQL_OPTION_ROW(TlsCipherStrength, MARIADB_OPT_TLS_CIPHER_STRENGTH, unsigned int *)                               \
    MYSQL_OPTION_ROW(MariadbTlsVersion, MARIADB_OPT_TLS_VERSION, std::string)                                          \
    MYSQL_OPTION_ROW(TlsPeerFp, MARIADB_OPT_TLS_PEER_FP, std::string)                                                  \
    MYSQL_OPTION_ROW(TlsPeerFpList, MARIADB_OPT_TLS_PEER_FP_LIST, std::string)                                         \
    MYSQL_OPTION_ROW(Port, MARIADB_OPT_PORT, int)                                                                      \
    MYSQL_OPTION_ROW(Unixsocket, MARIADB_OPT_UNIXSOCKET, std::string)                                                  \
    MYSQL_OPTION_ROW(Password, MARIADB_OPT_PASSWORD, std::string)                                                      \
    MYSQL_OPTION_ROW(Host, MARIADB_OPT_HOST, std::string)                                                              \
    MYSQL_OPTION_ROW(User, MARIADB_OPT_USER, std::string)                                                              \
    MYSQL_OPTION_ROW(Schema, MARIADB_OPT_SCHEMA, std::string)                                                          \
    MYSQL_OPTION_ROW(FoundRows, MARIADB_OPT_FOUND_ROWS, int)                                                           \
    MYSQL_OPTION_ROW(MultiREsults, MARIADB_OPT_MULTI_RESULTS, int)                                                     \
    MYSQL_OPTION_ROW(MultiStatements, MARIADB_OPT_MULTI_STATEMENTS, std::string)

namespace sqlopt {
namespace detail {
// 获取枚举字符串名称
inline static const char *getMySqlOptName(mysql_option opt) {
    switch (opt) {
#define MYSQL_OPTION_ROW(_, enum, _1)                                                                                  \
    case enum:                                                                                                         \
        return #enum;
        MYSQL_OPTION_TABLE
#undef MYSQL_OPTION_ROW
        default:
            return "unknown";
    }
}
} // namespace detail

class OptionBase {
public:
    virtual auto setopt(MYSQL &sql) const -> int = 0;
    virtual auto getopt(MYSQL &sql) -> int       = 0;
};

template <mysql_option Optname, typename T, class enable = void>
class OptionT : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(T value) : mValue(value) {}
    auto setopt(MYSQL &sql) const -> int override {
        auto ret = mysql_optionsv(&sql, Optname, &mValue);
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), mValue);
        }
        return ret;
    }

    auto getopt(MYSQL &sql) -> int override {
        auto ret = mysql_get_optionv(&sql, Optname, &mValue);
        if (ret != 0) {
            ILIAS_ERROR("sql", "{} get error({}).", detail::getMySqlOptName(Optname), ret);
        }
        return ret;
    }
    auto setValue(T value) -> void { mValue = value; }
    /**
     * @brief Get the value of the option
     *
     */
    auto value() const noexcept { return mValue; }

    /**
     * @brief Directly get the value of the option
     *
     * @return T
     */
    operator T() const noexcept { return mValue; }

private:
    T mValue {};
};

template <mysql_option Optname, typename T>
class OptionT<Optname, T *, void> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(T value) : mValue(value) {}
    auto setopt(MYSQL &sql) const -> int override {
        auto ret = mysql_optionsv(&sql, Optname, &mValue);
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) set error({})", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), mValue);
        }
        return ret;
    }

    auto getopt(MYSQL &sql) -> int override {
        auto ret = mysql_get_optionv(&sql, Optname, &mValue);
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) get error({}).", detail::getMySqlOptName(Optname), ret);
        }
        return ret;
    }

    auto setValue(T value) -> void { mValue = value; }
    /**
     * @brief Get the value of the option
     *
     */
    constexpr auto value() const noexcept { return mValue; }

    /**
     * @brief Directly get the value of the option
     *
     * @return T
     */
    constexpr operator T() const noexcept { return mValue; }

private:
    T mValue {};
};

template <mysql_option Optname>
class OptionT<Optname, std::string, void> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(const std::string &value) : mValue(value) {}
    auto setopt(MYSQL &sql) const -> int override {
        auto ret = mysql_optionsv(&sql, Optname, mValue == "" ? nullptr : (void *)mValue.c_str());
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), mValue);
        }
        return ret;
    }

    auto getopt(MYSQL &sql) -> int override {
        const char *value = nullptr;
        auto        ret   = mysql_get_optionv(&sql, Optname, &value);
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) get error({}).", detail::getMySqlOptName(Optname), ret);
        }
        else {
            mValue = std::string(value);
        }
        return ret;
    }

    auto setValue(std::string_view value) -> void { mValue = value; }
    /**
     * @brief Get the value of the option
     *
     */
    constexpr auto value() const noexcept { return mValue; }

    /**
     * @brief Directly get the value of the option
     *
     * @return T
     */
    constexpr operator std::string() const noexcept { return mValue; }

private:
    std::string mValue {};
};

#define MYSQL_OPTION_ROW(Name, EnumValue, Type) using Name = OptionT<EnumValue, Type>;
MYSQL_OPTION_TABLE
#undef MYSQL_OPTION_ROW

} // namespace sqlopt
#undef MYSQL_OPTION_TABLE
ILIAS_MYSQL_NS_END