#pragma once

#include "ilias/postgres/global.hpp"
#include "ilias/sql/types.hpp"
#include <libpq-fe.h>
#include <functional>

ILIAS_POSTGRES_NS_BEGIN
ILIAS_SQL_USE_NAMESPACE

/**
 * @brief PostgreSQL特定的单元格元数据
 *
 * 携带PostgreSQL解析数据所需的上下文信息：
 * - OID: PostgreSQL类型标识符
 * - data: 原始数据指针
 * - size: 数据大小
 * - pgconn: PostgreSQL连接指针（用于获取类型名称）
 */
struct PostgresCellMetadata {
    Oid oid = 0;                      // PostgreSQL类型OID
    const void* data = nullptr;        // 原始数据指针
    int size = 0;                     // 数据大小
    PGconn* pgconn = nullptr;          // PostgreSQL连接指针
    int format = 0;                   // 数据格式（0=文本，1=二进制）
};

/**
 * @brief PostgreSQL特定的值转换器上下文
 *
 * 继承自SqlValueConverterContext，添加PostgreSQL特定的功能：
 * - OID到类型名称的映射
 * - 基于OID的类型解析器注册
 * - 基于OID的类型绑定器注册
 */
class ILIAS_SQL_API PostgresValueConverterContext : public SqlValueConverterContext {
public:
    using OidParserFunc = std::function<SqlParserResult(const SqlCellView&, const PostgresCellMetadata&)>;
    using OidBinderFunc = std::function<SqlBinderResult(const SqlCellView&, const std::any&, const PostgresCellMetadata&)>;

    PostgresValueConverterContext() = default;
    ~PostgresValueConverterContext() override = default;

    /**
     * @brief 注册基于OID的类型解析器
     * @param oid PostgreSQL类型OID
     * @param func 解析器函数
     */
    void registerOidParser(Oid oid, OidParserFunc func) {
        mOidParsers[oid] = std::move(func);
    }

    /**
     * @brief 注册基于OID的类型绑定器
     * @param oid PostgreSQL类型OID
     * @param func 绑定器函数
     */
    void registerOidBinder(Oid oid, OidBinderFunc func) {
        mOidBinders[oid] = std::move(func);
    }

    /**
     * @brief 查找基于OID的类型解析器
     * @param oid PostgreSQL类型OID
     * @return 解析器函数，如果未找到则返回nullptr
     */
    auto findOidParser(Oid oid) const -> OidParserFunc {
        if (mOidParsers.count(oid) > 0) {
            return mOidParsers.at(oid);
        }
        return nullptr;
    }

    /**
     * @brief 查找基于OID的类型绑定器
     * @param oid PostgreSQL类型OID
     * @return 绑定器函数，如果未找到则返回nullptr
     */
    auto findOidBinder(Oid oid) const -> OidBinderFunc {
        auto it = mOidBinders.find(oid);
        if (it != mOidBinders.end()) {
            return it->second;
        }
        return nullptr;
    }

    /**
     * @brief 设置类型映射（OID到类型名称）
     * @param typeMap 类型映射表
     */
    void setTypeMap(const std::map<Oid, std::string>& typeMap) {
        mTypeMap = typeMap;
    }

    /**
     * @brief 获取类型映射
     * @return 类型映射表
     */
    auto getTypeMap() const -> const std::map<Oid, std::string>& {
        return mTypeMap;
    }

    /**
     * @brief 根据OID获取类型名称
     * @param oid PostgreSQL类型OID
     * @return 类型名称，如果未找到则返回"unknown"
     */
    auto getTypeName(Oid oid) const -> std::string_view {
        auto it = mTypeMap.find(oid);
        if (it != mTypeMap.end()) {
            return it->second;
        }
        return "unknown";
    }

private:
    std::map<Oid, OidParserFunc> mOidParsers;
    std::map<Oid, OidBinderFunc> mOidBinders;
    std::map<Oid, std::string> mTypeMap;
};

ILIAS_POSTGRES_NS_END
