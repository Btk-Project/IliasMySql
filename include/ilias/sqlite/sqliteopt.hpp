#pragma once

#include "global.hpp"

#include <sqlite3.h>
#include <ilias/io/error.hpp>
#include <array>
#include <charconv>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "ilias/sql/sqlerror.hpp"

ILIAS_SQLITE_NS_BEGIN

// =========================================================================================
// SQLite3 Configuration Table
// =========================================================================================
// 格式: SQLITE_CONFIG_ROW(FriendlyName, SQLiteMacro, Type)
// 注意：为了区分 Global Config (进程级) 和 DB Config (连接级)，我们通过宏的值来判断。
// 通常 SQLITE_CONFIG_* < 1000, SQLITE_DBCONFIG_* > 1000
#define SQLITE_CONFIG_TABLE                                                                                            \
    /* --- Global Configurations (sqlite3_config) --- */                                                               \
    /* 线程模式 (无参数) */                                                                                            \
    SQLITE_CONFIG_ROW(SingleThread, SQLITE_CONFIG_SINGLETHREAD, int)                                                   \
    SQLITE_CONFIG_ROW(MultiThread, SQLITE_CONFIG_MULTITHREAD, int)                                                     \
    SQLITE_CONFIG_ROW(Serialized, SQLITE_CONFIG_SERIALIZED, int)                                                       \
    /* 内存与性能 */                                                                                                   \
    SQLITE_CONFIG_ROW(MemStatus, SQLITE_CONFIG_MEMSTATUS, bool)                                                        \
    SQLITE_CONFIG_ROW(SmallMalloc, SQLITE_CONFIG_SMALL_MALLOC, bool)                                                   \
    SQLITE_CONFIG_ROW(CoveringIndexScan, SQLITE_CONFIG_COVERING_INDEX_SCAN, bool)                                      \
    SQLITE_CONFIG_ROW(Uri, SQLITE_CONFIG_URI, bool)                                                                    \
    SQLITE_CONFIG_ROW(StmtJrnlSpill, SQLITE_CONFIG_STMTJRNL_SPILL, int)                                                \
    SQLITE_CONFIG_ROW(SorterRefSize, SQLITE_CONFIG_SORTERREF_SIZE, int)                                                \
    SQLITE_CONFIG_ROW(Win32HeapSize, SQLITE_CONFIG_WIN32_HEAPSIZE, int)                                                \
                                                                                                                       \
    /* --- Connection Configurations (sqlite3_db_config) --- */                                                        \
    /* 核心行为 */                                                                                                     \
    SQLITE_CONFIG_ROW(MainDbName, SQLITE_DBCONFIG_MAINDBNAME, std::string)                                             \
    SQLITE_CONFIG_ROW(EnableFKey, SQLITE_DBCONFIG_ENABLE_FKEY, bool)                                                   \
    SQLITE_CONFIG_ROW(EnableTrigger, SQLITE_DBCONFIG_ENABLE_TRIGGER, bool)                                             \
    SQLITE_CONFIG_ROW(EnableView, SQLITE_DBCONFIG_ENABLE_VIEW, bool)                                                   \
    SQLITE_CONFIG_ROW(EnableFts3Tokenizer, SQLITE_DBCONFIG_ENABLE_FTS3_TOKENIZER, bool)                                \
    SQLITE_CONFIG_ROW(EnableLoadExtension, SQLITE_DBCONFIG_ENABLE_LOAD_EXTENSION, bool)                                \
    SQLITE_CONFIG_ROW(NoCkptOnClose, SQLITE_DBCONFIG_NO_CKPT_ON_CLOSE, bool)                                           \
    SQLITE_CONFIG_ROW(EnableQpsg, SQLITE_DBCONFIG_ENABLE_QPSG, bool) /* Query Plan Stability Guarantee */              \
    /* 安全相关 */                                                                                                     \
    SQLITE_CONFIG_ROW(Defensive, SQLITE_DBCONFIG_DEFENSIVE, bool)                                                      \
    SQLITE_CONFIG_ROW(WritableSchema, SQLITE_DBCONFIG_WRITABLE_SCHEMA, bool)                                           \
    SQLITE_CONFIG_ROW(TrustedSchema, SQLITE_DBCONFIG_TRUSTED_SCHEMA, bool)                                             \
    SQLITE_CONFIG_ROW(DqsDml, SQLITE_DBCONFIG_DQS_DML, bool) /* Double-quoted string literals in DML */                \
    SQLITE_CONFIG_ROW(DqsDdl, SQLITE_DBCONFIG_DQS_DDL, bool) /* Double-quoted string literals in DDL */                \
    SQLITE_CONFIG_ROW(LegacyAlterTable, SQLITE_DBCONFIG_LEGACY_ALTER_TABLE, bool)                                      \
    SQLITE_CONFIG_ROW(LegacyFileFormat, SQLITE_DBCONFIG_LEGACY_FILE_FORMAT, bool)                                      \
    SQLITE_CONFIG_ROW(TriggerEqp, SQLITE_DBCONFIG_TRIGGER_EQP, bool)                                                   \
    SQLITE_CONFIG_ROW(ResetDatabase, SQLITE_DBCONFIG_RESET_DATABASE, bool)                                             \
    /* 统计与扫描 */                                                                                                   \
    SQLITE_CONFIG_ROW(StmtScanStatus, SQLITE_DBCONFIG_STMT_SCANSTATUS, bool)                                           \
    SQLITE_CONFIG_ROW(ReverseScanOrder, SQLITE_DBCONFIG_REVERSE_SCANORDER, bool)

namespace sqlopt {
namespace detail {
// 获取枚举字符串名称
inline static const char *getMySqlOptName(int opt) {
    switch (opt) {
#define SQLITE_CONFIG_ROW(_, enum_val, _1)                                                                             \
    case enum_val:                                                                                                     \
        return #enum_val;
        SQLITE_CONFIG_TABLE
#undef SQLITE_CONFIG_ROW
        default:
            return "unknown";
    }
}

inline constexpr auto enumMySqlOptNames() {
    return std::array {
#define SQLITE_CONFIG_ROW(enum_name, _1, _2) #enum_name,
        SQLITE_CONFIG_TABLE
#undef SQLITE_CONFIG_ROW
    };
}

inline constexpr auto enumMySqlOptValues() {
    return std::array {
#define SQLITE_CONFIG_ROW(_1, enum_val, _2) enum_val,
        SQLITE_CONFIG_TABLE
#undef SQLITE_CONFIG_ROW
    };
}

inline static int getMySqlOptEnum(const std::string &name) {
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
    return -1;
}

template <typename EnumT, class enable = void>
struct StringToMySqlEnumHelper {
    static_assert(sizeof(EnumT) == sizeof(int), "EnumT must be int");
    static_assert(std::is_enum_v<EnumT>, "EnumT must be enum");
    // 用户可特化此结构体以支持自定义 Enum 解析
};
} // namespace detail

class ILIAS_SQL_API OptionBase {
public:
    virtual ~OptionBase()                          = default;
    virtual auto setopt(sqlite3 &sql) const -> int = 0;
    virtual auto getopt(sqlite3 &sql) -> int       = 0;
};

// --------------------------------------------------------------------------------
// Numeric Option Template (int, long, enum)
// --------------------------------------------------------------------------------
template <int Optname, typename T, class enable = void>
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
            // 简单的强转 fallback，复杂 enum 请特化 StringToMySqlEnumHelper
            mValue = static_cast<T>(std::atoi(value.data()));
        }
        else {
            ILIAS_ERROR("sql", "option({}) unknow type.", detail::getMySqlOptName(Optname));
        }
    }

    auto setopt(sqlite3 &sql) const -> int override {
        int ret = SQLITE_OK;
        // 区分 sqlite3_config (Global, < 1000) 和 sqlite3_db_config (Connection, >= 1000)
        if constexpr (Optname < 1000) {
            // 特殊处理无参配置
            if constexpr (Optname == SQLITE_CONFIG_SINGLETHREAD || Optname == SQLITE_CONFIG_MULTITHREAD ||
                          Optname == SQLITE_CONFIG_SERIALIZED) {
                ret = sqlite3_config(Optname);
            }
            else {
                // sqlite3_config 传递的是值
                ret = sqlite3_config(Optname, static_cast<int>(mValue));
            }
        }
        else {
            // sqlite3_db_config(db, op, val, result*)
            ret = sqlite3_db_config(&sql, Optname, static_cast<int>(mValue), nullptr);
        }

        if (ret != SQLITE_OK) {
            ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), (int)mValue);
        }
        return ret;
    }

    auto getopt(sqlite3 &sql) -> int override {
        int ret    = SQLITE_OK;
        int outVal = 0;

        if constexpr (Optname >= 1000) {
            // -1 表示查询当前值
            ret = sqlite3_db_config(&sql, Optname, -1, &outVal);
            if (ret == SQLITE_OK) {
                mValue = static_cast<T>(outVal);
            }
        }
        else {
            // sqlite3_config 通常没有统一的 getter (除了特定的 GETMALLOC 等)，这里返回错误或忽略
            // 暂不支持 Global Config 的通用 Get
            ret = SQLITE_ERROR;
        }

        if (ret != SQLITE_OK) {
            ILIAS_ERROR("sql", "{} get error({}).", detail::getMySqlOptName(Optname), ret);
        }
        return ret;
    }

    auto setValue(T value) -> void { mValue = value; }
    auto value() const noexcept { return mValue; }
    operator T() const noexcept { return mValue; }

private:
    T mValue {};
};

// --------------------------------------------------------------------------------
// Boolean Specialization
// --------------------------------------------------------------------------------
template <int Optname>
class ILIAS_SQL_API OptionT<Optname, bool, void> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(bool value) : mValue(value) {}
    explicit OptionT(std::string_view value) {
        if (value == "true" || value == "1" || value == "yes" || value == "on") {
            mValue = true;
        }
        else {
            mValue = false;
        }
    }

    auto setopt(sqlite3 &sql) const -> int override {
        int ret    = SQLITE_OK;
        int intVal = mValue ? 1 : 0;

        if constexpr (Optname < 1000) {
            ret = sqlite3_config(Optname, intVal);
        }
        else {
            ret = sqlite3_db_config(&sql, Optname, intVal, nullptr);
        }

        if (ret != SQLITE_OK) {
            ILIAS_ERROR("sql", "option({}) set error({})", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), mValue);
        }
        return ret;
    }

    auto getopt(sqlite3 &sql) -> int override {
        int ret    = SQLITE_OK;
        int outVal = 0;
        if constexpr (Optname >= 1000) {
            ret = sqlite3_db_config(&sql, Optname, -1, &outVal);
            if (ret == SQLITE_OK)
                mValue = (outVal != 0);
        }
        else {
            ret = SQLITE_ERROR; // Global config getter unsupported via generic interface
        }

        if (ret != SQLITE_OK) {
            ILIAS_ERROR("sql", "option({}) get error({}).", detail::getMySqlOptName(Optname), ret);
        }
        return ret;
    }

    auto           setValue(bool value) -> void { mValue = value; }
    constexpr auto value() const noexcept { return mValue; }
    constexpr      operator bool() const noexcept { return mValue; }

private:
    bool mValue {};
};

// --------------------------------------------------------------------------------
// String Specialization
// --------------------------------------------------------------------------------
template <int Optname>
class ILIAS_SQL_API OptionT<Optname, std::string, void> : public OptionBase {
public:
    constexpr OptionT() = default;
    constexpr OptionT(const std::string &value) : mValue(value) {}
    constexpr OptionT(std::string_view value) : mValue(value) {}

    auto setopt(sqlite3 &sql) const -> int override {
        int ret = SQLITE_OK;

        if constexpr (Optname < 1000) {
            // Global string configs (rarely used simply, mostly callbacks, but supporting char*)
            ret = sqlite3_config(Optname, mValue.c_str());
        }
        else {
            // DB Config string: e.g. SQLITE_DBCONFIG_MAINDBNAME
            // The signature for string db_config is usually (db, op, const char*)
            ret = sqlite3_db_config(&sql, Optname, mValue.c_str());
        }

        if (ret != SQLITE_OK) {
            ILIAS_ERROR("sql", "option({}) set error({}).", detail::getMySqlOptName(Optname), ret);
        }
        else {
            ILIAS_TRACE("sql", "option({}) set value({}).", detail::getMySqlOptName(Optname), mValue);
        }
        return ret;
    }

    auto getopt([[maybe_unused]] sqlite3 &sql) -> int override {
        // String getters are not standard in sqlite3_config/db_config
        return SQLITE_ERROR;
    }

    auto           setValue(std::string_view value) -> void { mValue = value; }
    constexpr auto value() const noexcept { return mValue; }
    operator std::string() const noexcept { return mValue; }

private:
    std::string mValue {};
};

// --------------------------------------------------------------------------------
// Factory & Types
// --------------------------------------------------------------------------------

#define SQLITE_CONFIG_ROW(Name, EnumValue, Type) using Name = OptionT<EnumValue, Type>;
SQLITE_CONFIG_TABLE
#undef SQLITE_CONFIG_ROW

inline OptionBase *createOption(int opt, std::string_view value) {
    switch (opt) {
#define SQLITE_CONFIG_ROW(Name, EnumValue, Type)                                                                       \
    case EnumValue:                                                                                                    \
        return new Name(value);
        SQLITE_CONFIG_TABLE
#undef SQLITE_CONFIG_ROW
        default:
            return nullptr;
    }
}

// =========================================================================================
// SQLite3 Open Flags Helper
// =========================================================================================
// 定义支持的 Open Flags
#define SQLITE_OPEN_FLAG_TABLE                                                                                         \
    SQLITE_CONFIG_ROW(ReadOnly, SQLITE_OPEN_READONLY, int)                                                             \
    SQLITE_CONFIG_ROW(ReadWrite, SQLITE_OPEN_READWRITE, int)                                                           \
    SQLITE_CONFIG_ROW(Create, SQLITE_OPEN_CREATE, int)                                                                 \
    SQLITE_CONFIG_ROW(Uri, SQLITE_OPEN_URI, int)                                                                       \
    SQLITE_CONFIG_ROW(Memory, SQLITE_OPEN_MEMORY, int)                                                                 \
    SQLITE_CONFIG_ROW(NoMutex, SQLITE_OPEN_NOMUTEX, int)                                                               \
    SQLITE_CONFIG_ROW(FullMutex, SQLITE_OPEN_FULLMUTEX, int)                                                           \
    SQLITE_CONFIG_ROW(SharedCache, SQLITE_OPEN_SHAREDCACHE, int)                                                       \
    SQLITE_CONFIG_ROW(PrivateCache, SQLITE_OPEN_PRIVATECACHE, int)

namespace detail {
inline int getOpenFlagValue(const std::string &name) {
    // 复用之前的逻辑，这里手动展开以便解析
    static const std::pair<const char *, int> map[] = {
#define SQLITE_CONFIG_ROW(Name, Val, _Type) {#Name, Val}, {#Val, Val},
        SQLITE_OPEN_FLAG_TABLE
#undef SQLITE_CONFIG_ROW
    };

    for (const auto &[k, v] : map) {
        // 忽略大小写比较
#if defined(_MSC_VER)
        if (_stricmp(name.c_str(), k) == 0)
            return v;
#else
        if (strcasecmp(name.c_str(), k) == 0)
            return v;
#endif
    }
    return 0;
}
} // namespace detail

// 解析 "READWRITE|CREATE|NOMUTEX" 格式的字符串
inline int parseOpenFlags(std::string_view input) {
    int         flags = 0;
    std::string temp(input);
    char       *token = std::strtok(temp.data(), "|, "); // 支持 | 或 , 分割
    while (token != nullptr) {
        flags |= detail::getOpenFlagValue(token);
        token = std::strtok(nullptr, "|, ");
    }
    return flags;
}

} // namespace sqlopt
#undef SQLITE_CONFIG_TABLE
ILIAS_SQLITE_NS_END