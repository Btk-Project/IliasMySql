#pragma once

#include "global.hpp"

#include <mariadb/mysql.h>
#include <ilias/io/error.hpp>

#include "ilias/sql/sqlerror.hpp"

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
inline constexpr auto enumMySqlOptNames() {
    std::array names = {
#define MYSQL_OPTION_ROW(enum_name, _1, _2) #enum_name,
        MYSQL_OPTION_TABLE
#undef MYSQL_OPTION_ROW
    };
    return names;
}

inline constexpr auto enumMySqlOptValues() {
    std::array values = {
#define MYSQL_OPTION_ROW(_1, enum, _2) enum,
        MYSQL_OPTION_TABLE
#undef MYSQL_OPTION_ROW
    };
    return values;
}

inline static mysql_option getMySqlOptEnum(const std::string &name) {
    auto names  = enumMySqlOptNames();
    auto values = enumMySqlOptValues();
    for (size_t i = 0; i < names.size(); ++i) {
        // 忽略大小写
#if defined(_MSC_VER)
        if (_stricmp(name.c_str(), names[i]) == 0) {
            return values[i];
        }
#else
        if (strcasecmp(name.c_str(), names[i]) == 0) {
            return values[i];
        }
#endif
    }
    return (mysql_option)-1;
}
//     MYSQL_PROTOCOL_DEFAULT, MYSQL_PROTOCOL_TCP, MYSQL_PROTOCOL_SOCKET, MYSQL_PROTOCOL_PIPE, MYSQL_PROTOCOL_MEMORY
template <typename EnumT, class enable = void>
struct StringToMySqlEnumHelper {
    static_assert(sizeof(EnumT) == sizeof(int), "EnumT must be int");
    static_assert(std::is_enum_v<EnumT>, "EnumT must be enum");
    static_assert(!std::is_enum_v<EnumT>, "Unknown enum type");
};

template <>
struct StringToMySqlEnumHelper<mysql_protocol_type, void> {
    static mysql_protocol_type operator()(const std::string &name) {
        if (name == "MYSQL_PROTOCOL_DEFAULT") {
            return MYSQL_PROTOCOL_DEFAULT;
        }
        else if (name == "MYSQL_PROTOCOL_TCP") {
            return MYSQL_PROTOCOL_TCP;
        }
        else if (name == "MYSQL_PROTOCOL_SOCKET") {
            return MYSQL_PROTOCOL_SOCKET;
        }
        else if (name == "MYSQL_PROTOCOL_PIPE") {
            return MYSQL_PROTOCOL_PIPE;
        }
        else if (name == "MYSQL_PROTOCOL_MEMORY") {
            return MYSQL_PROTOCOL_MEMORY;
        }
        else {
            return (mysql_protocol_type)-1;
        }
    }
};

template <>
struct StringToMySqlEnumHelper<mysql_option> {
    static mysql_option operator()(const std::string &name) { return getMySqlOptEnum(name); }
};

} // namespace detail

class ILIAS_SQL_API OptionBase {
public:
    virtual ~OptionBase()                        = default;
    virtual auto setopt(MYSQL &sql) const -> int = 0;
    virtual auto getopt(MYSQL &sql) -> int       = 0;
};

template <mysql_option Optname, typename T, class enable = void>
class ILIAS_SQL_API OptionT : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(T value) : mValue(value) {}
    explicit OptionT(std::string_view value) {
        if constexpr (std::is_arithmetic_v<T> && !std::is_enum_v<T>) {
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), mValue);
            if (ec != std::errc()) {
                ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), (int)ec);
            }
        }
        else if constexpr (std::is_enum_v<T>) {
            mValue = static_cast<T>(detail::StringToMySqlEnumHelper<T>()(std::string(value)));
        }
        else {
            ILIAS_ERROR("sql", "option({}) unknow.", detail::getMySqlOptName(Optname));
        }
    }
    auto setopt(MYSQL &sql) const -> int override {
        auto ret = mysql_optionsv(&sql, Optname, &mValue);
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), (int)mValue);
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
class OptionT<Optname, T *, std::enable_if_t<!std::is_function_v<T>>> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(T value) : mValue(value) {}
    explicit OptionT(std::string_view value) {
        if constexpr (std::is_arithmetic_v<T> && !std::is_same_v<T, bool>) {
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), mValue);
            if (ec != std::errc()) {
                ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), (int)ec);
            }
        }
        else if constexpr (std::is_same_v<T, bool>) {
            mValue = static_cast<T>(value == "true" || value == "1" || value == "yes" || value == "on");
        }
        else {
            ILIAS_ERROR("sql", "option({}) unknow.", detail::getMySqlOptName(Optname));
        }
    }
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

template <mysql_option Optname, typename T>
class OptionT<Optname, T *, std::enable_if_t<std::is_function_v<T>>> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(T value) : mValue(value) {}
    explicit OptionT(std::string_view value) {
        if constexpr (std::is_function_v<T>) {
            uint64_t fptr;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), fptr);
            if (ec != std::errc()) {
                ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), (int)ec);
            }
            mValue = reinterpret_cast<T *>(fptr);
        }
        else {
            ILIAS_ERROR("sql", "option({}) unknow.", detail::getMySqlOptName(Optname));
        }
    }
    auto setopt(MYSQL &sql) const -> int override {
        auto ret = mysql_optionsv(&sql, Optname, mValue);
        if (ret != 0) {
            ILIAS_ERROR("sql", "option({}) set error({})", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), (void *)mValue);
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
    constexpr operator T *() const noexcept { return mValue; }

private:
    T *mValue {};
};

template <mysql_option Optname>
class ILIAS_SQL_API OptionT<Optname, std::string, void> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(const std::string &value) : mValue(value) {}
    constexpr OptionT(std::string_view value) : mValue(value) {}
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

inline OptionBase *createOption(mysql_option opt, std::string_view value) {
    switch (opt) {
#define MYSQL_OPTION_ROW(Name, EnumValue, Type)                                                                        \
    case EnumValue:                                                                                                    \
        return new Name(value);
        MYSQL_OPTION_TABLE
#undef MYSQL_OPTION_ROW
        default:
            return nullptr;
    }
}

} // namespace sqlopt
#undef MYSQL_OPTION_TABLE
ILIAS_MYSQL_NS_END