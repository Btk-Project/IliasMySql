/**
 * @file interfaces.hpp
 * @brief Abstract interfaces for SQL drivers
 */
#pragma once
#include <ilias/io/context.hpp>
#include <memory>
#include <optional>
#include <string_view>
#include <map>

#include "ilias/sql/global/global.hpp"
#include "types.hpp"

ILIAS_SQL_NS_BEGIN

struct ConnectOptions {
    std::string                        host;
    uint16_t                           port = 0;
    std::string                        user;
    std::string                        password;
    std::string                        database;
    std::string                        filename;
    std::map<std::string, std::string> extra;
};

// 前置声明
class IStatement;

struct ResultCapabilities {
    bool streaming        = false;
    bool exactRowCount    = false;
    bool rowsAffected     = false;
};

/**
 * @brief 结果集迭代器接口
 * 对应 rusqlite 的 Rows
 */
class IResultSet {
public:
    virtual ~IResultSet() = default;

    // 移动到下一行。返回 false 表示结束。
    virtual auto next() -> IoTask<bool> = 0;

    virtual auto capabilities() const -> ResultCapabilities = 0;
    virtual auto rowsFetched() const -> size_t = 0;
    virtual auto exactRowCount() const -> std::optional<size_t> = 0;
    virtual auto rowsAffected() const -> std::optional<size_t> = 0;
    virtual auto lastNativeError() const -> std::optional<NativeSqlError> { return std::nullopt; }
    // 获取列数
    virtual auto columnCount() const -> size_t                      = 0;
    virtual auto columnName(size_t index) const -> std::string_view = 0;

    virtual auto getValue(size_t index) -> IoResult<SqlCellView> = 0;

    // 按列名获取
    virtual auto getValue(std::string_view name) -> IoResult<SqlCellView> = 0;
    /**
     * @brief 返回后端原生结果句柄，仅用于高级互操作。
     *
     * 指针类型由具体驱动决定，例如 sqlite3_stmt*、MYSQL_RES*、PGresult*。
     * 调用方不拥有该指针，不得释放；其有效期不超过当前 result set，
     * 且可能在 next()/close()/析构后失效。
     */
    virtual auto nativeHandle() const -> void *                           = 0;
};

/**
 * @brief 预编译语句接口
 * 对应 rusqlite 的 Statement
 */
class IStatement {
public:
    virtual ~IStatement() = default;

    // 执行查询 (SELECT)，返回结果集
    virtual auto query() -> IoTask<std::unique_ptr<IResultSet>> = 0;

    // 执行命令 (INSERT, UPDATE, DELETE)，返回影响行数
    virtual auto execute() -> IoTask<size_t> = 0;

    // 重置状态以便复用
    virtual auto reset() -> void                = 0;
    /**
     * @brief 返回后端原生 statement 句柄，仅用于高级互操作。
     *
     * 调用方不拥有该指针，不得释放。句柄在 statement close/reset/析构后失效；
     * 对句柄做出的后端操作可能破坏本抽象层维护的绑定和结果状态。
     */
    virtual auto nativeHandle() const -> void * = 0;
    virtual auto lastNativeError() const -> std::optional<NativeSqlError> { return std::nullopt; }

    /**
     * @brief 绑定一个外部变量引用。
     *
     * 低层 IStatement 不复制 value，调用方必须保证 value 至少活到 query()/execute()
     * 消费完本次绑定。需要自动保存临时值时，使用高层 SqlStatement wrapper。
     */
    template <typename T>
    auto bind(size_t index, T &&value) -> IoResult<void>;
    template <typename T>
    auto bind(std::string_view name, T &&value) -> IoResult<void>;

protected:
    virtual auto bind(std::type_index type_index, size_t index, const SqlCellView &value) -> IoResult<void> = 0;
    virtual auto bind(std::type_index type_index, std::string_view name, const SqlCellView &value)
        -> IoResult<void> = 0;
};

/**
 * @brief 数据库连接接口
 * 对应 rusqlite 的 Connection
 */
class IConnection {
public:
    virtual ~IConnection()                                             = default;
    virtual auto sqlname() -> std::string                              = 0;
    virtual auto sqlinfo() -> std::string                              = 0;
    virtual auto connect() -> IoTask<void>                             = 0;
    virtual auto disconnect() -> IoTask<void>                          = 0;
    virtual auto selectDatabase(std::string_view name) -> IoTask<void> = 0;

    // 预编译 SQL
    virtual auto prepare(std::string_view sql) -> IoTask<std::unique_ptr<IStatement>> = 0;

    // 直接执行 SQL
    virtual auto execute(std::string_view sql) -> IoTask<size_t>                    = 0;
    virtual auto query(std::string_view sql) -> IoTask<std::unique_ptr<IResultSet>> = 0;

    // 事务控制
    virtual auto beginTransaction() -> IoTask<bool> = 0;
    virtual auto commit() -> IoTask<bool>           = 0;
    virtual auto rollback() -> IoTask<bool>         = 0;
    virtual auto syncRollback() -> bool             = 0;

    // 获取最后一次插入的 ID
    virtual auto lastInsertId() const -> int64_t = 0;

    // 类型转换上下文
    virtual auto valueConverterContext() const -> std::shared_ptr<SqlValueConverterContext> = 0;

    // 连通性检测
    virtual auto ping() -> IoTask<bool>         = 0;
    /**
     * @brief 返回后端原生连接句柄，仅用于高级互操作。
     *
     * 指针类型由具体驱动决定，例如 sqlite3*、MYSQL*、PGconn*。
     * 调用方不拥有该指针，不得释放；其有效期不超过当前 connection。
     * 直接调用后端 API 可能绕过本库的异步状态机、事务状态和错误快照。
     */
    virtual auto nativeHandle() const -> void * = 0;
    virtual auto lastNativeError() const -> std::optional<NativeSqlError> { return std::nullopt; }
};

template <typename T>
auto IStatement::bind(std::string_view name, T &&value) -> IoResult<void> {
    static_assert(std::is_lvalue_reference<T>::value, "bind() only accepts lvalue reference");
    using value_type = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<value_type, std::nullptr_t> || std::is_same_v<value_type, std::nullopt_t>) {
        return bind(std::type_index(typeid(SqlNull)), name, SqlCellView {});
    }
    else if constexpr (OptionalLikeType<value_type>::value) {
        if (OptionalLikeType<value_type>::has_value(value)) {
            return bind(name, *value);
        }
        else {
            return bind(std::type_index(typeid(const SqlNull)), name, SqlCellView {});
        }
    }
    else if constexpr (std::is_same_v<value_type, SqlCellView>) {
        return bind(value.raw_type(), name, value);
    }
    else if constexpr (std::is_same_v<value_type, std::string_view> || std::is_same_v<value_type, std::string> ||
                       std::is_constructible_v<std::string_view, T>) {
        auto strView = std::string_view(value);
        return bind(std::type_index(typeid(const char *)), name,
                    SqlCellView(nullptr, strView.data(), strView.size(), typeid(const char *), -1));
    }
    else if constexpr (std::is_convertible_v<value_type, std::span<const std::byte>> ||
                       std::is_convertible_v<value_type, std::span<const char>>) {
        auto span = std::span(value);
        return bind(std::type_index(typeid(const std::byte *)), name,
                    SqlCellView(nullptr, span.data(), span.size(), typeid(const std::byte *), -1));
    }
    else {
        return bind(std::type_index(typeid(const value_type)), name,
                    SqlCellView(nullptr, &value, sizeof(value_type), typeid(const value_type), -1));
    }
}

template <typename T>
auto IStatement::bind(size_t index, T &&value) -> IoResult<void> {
    static_assert(std::is_lvalue_reference<T>::value, "bind() only accepts lvalue reference");
    using value_type = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<value_type, std::nullptr_t> || std::is_same_v<value_type, std::nullopt_t>) {
        // 空值不携带类型信息，因此需要显式指定SqlNull类型
        return bind(std::type_index(typeid(SqlNull)), index, SqlCellView {});
    }
    else if constexpr (OptionalLikeType<value_type>::value) {
        // 类似于std::optional的值类型，需要判断是否有值
        if (OptionalLikeType<value_type>::has_value(value)) {
            return bind(index, *value);
        }
        else {
            return bind(std::type_index(typeid(const SqlNull)), index, SqlCellView {});
        }
    }
    else if constexpr (std::is_same_v<value_type, SqlCellView>) {
        // 特殊类型，直接绑定
        return bind(value.raw_type(), index, value);
    }
    else if constexpr (std::is_same_v<value_type, std::string_view> || std::is_same_v<value_type, std::string> ||
                       std::is_constructible_v<std::string_view, T>) {
        // 统一处理字符串类型
        auto strView = std::string_view(value);
        return bind(std::type_index(typeid(const char *)), index,
                    SqlCellView(nullptr, strView.data(), strView.size(), typeid(const char *), -1));
    }
    else if constexpr (std::is_same_v<value_type, std::span<const std::byte>> ||
                       std::is_same_v<value_type, std::span<const char>> ||
                       std::is_constructible_v<std::span<const std::byte>, T>) {
        // 统一处理字节数组类型
        auto span = std::span(value);
        return bind(std::type_index(typeid(const std::byte *)), index,
                    SqlCellView(nullptr, span.data(), span.size(), typeid(const std::byte *), -1));
    }
    else {
        // 其他类型
        return bind(std::type_index(typeid(const value_type)), index,
                    SqlCellView(nullptr, &value, sizeof(value_type), typeid(const value_type), -1));
    }
}

ILIAS_SQL_NS_END
