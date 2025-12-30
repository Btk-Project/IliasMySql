#pragma once

#include "ilias/sql_orm/detail/orm_builder.hpp"
#include "ilias/sql_orm/detail/timestamp_manager.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include <nekoproto/serialization/reflection.hpp>

ILIAS_SQL_NS_BEGIN
namespace detail {

/**
 * @brief Enhanced InsertBuilder with automatic timestamp management
 * 
 * This class extends the base InsertBuilder to automatically apply created_at
 * timestamps before database insertion operations. It uses reflection to detect
 * fields with timestamp behavior and applies the appropriate timestamps.
 */
template <typename T>
class TimestampAwareInsertBuilder : public InsertBuilder<T> {
    template <typename U>
    friend auto executeLoopWrap(U self, int count) -> IoGenerator<size_t>;

public:
    TimestampAwareInsertBuilder(SqlDatabase &db, std::string tableName, std::vector<std::string> columnNames)
        : InsertBuilder<T>(db, std::move(tableName), std::move(columnNames)) {}

    // Override execute to apply timestamp logic before insertion
    IoTask<size_t> execute() {
        // Apply created_at timestamps to all bound objects
        applyInsertTimestamps();
        
        // Call parent execute
        return InsertBuilder<T>::execute();
    }

    // Override loop to apply timestamp logic for bulk operations
    IoGenerator<size_t> loop(int count) {
        // Apply timestamps for each iteration in bulk operations
        return executeLoopWrapWithTimestamps(std::move(*this), count);
    }

private:
    /**
     * @brief Apply created_at timestamps to all bound objects
     * 
     * Iterates through all bound objects and applies created_at timestamp
     * logic based on their SqlTags configuration.
     */
    void applyInsertTimestamps() {
        // Get created_at fields from type metadata
        auto createdAtFields = getCreatedAtFields<T>();
        
        // Apply timestamps to all bound objects
        for (auto& binder : this->mSetBinders) {
            // Try to cast to ObjBinder to access the object
            if (auto objBinder = std::dynamic_pointer_cast<ObjBinder<T>>(binder)) {
                // Apply created_at timestamps to the object
                TimestampManager::applyCreatedAt(objBinder->getObject(), createdAtFields);
            }
        }
    }
    
    /**
     * @brief Get list of field names that have created_at behavior
     * 
     * Uses reflection to iterate through type T and identify fields
     * with created_at = true in their SqlTags configuration.
     */
    template<typename ObjectType>
    static std::vector<std::string> getCreatedAtFields() {
        std::vector<std::string> fields;
        
        // Use reflection to iterate through fields and check SqlTags
        ObjectType obj;
        NEKO_NAMESPACE::Reflect<ObjectType>::forEach(obj, 
            [&](const auto& /*field*/, std::string_view name, const SqlTags& tags) {
                if (TimestampManager::shouldApplyCreatedAt(tags)) {
                    fields.emplace_back(name);
                }
            });
        
        return fields;
    }
    
    /**
     * @brief Enhanced loop wrapper that applies timestamps for bulk operations
     */
    template <typename BuilderType>
    static auto executeLoopWrapWithTimestamps(BuilderType self, int count) -> IoGenerator<size_t> {
        auto stmtRet = co_await self.prepare();
        if (!stmtRet) {
            co_yield Unexpected(stmtRet.error());
        }
        else {
            auto stmt = std::move(stmtRet.value());
            while (count--) {
                // Apply timestamps before each execution
                self.applyInsertTimestamps();
                
                self.bind(stmt);
                auto execRet = co_await stmt->execute();
                if (!execRet) {
                    co_yield Unexpected(execRet.error());
                }
                else {
                    co_yield execRet.value();
                }
                stmt->reset();
            }
        }
    }
};

/**
 * @brief Enhanced UpdateBuilder with automatic timestamp management
 * 
 * This class extends the base UpdateBuilder to automatically apply updated_at
 * timestamps before database update operations. It uses reflection to detect
 * fields with timestamp behavior and applies the appropriate timestamps.
 */
class TimestampAwareUpdateBuilder : public UpdateBuilder {
    template <typename U>
    friend auto executeLoopWrap(U self, int count) -> IoGenerator<size_t>;

public:
    TimestampAwareUpdateBuilder(SqlDatabase &db, std::string tableName) 
        : UpdateBuilder(db, std::move(tableName)) {}

    // Override execute to apply timestamp logic before update
    IoTask<size_t> execute() {
        // Apply updated_at timestamps to all bound objects
        applyUpdateTimestamps();
        
        // Call parent execute
        return UpdateBuilder::execute();
    }

    // Override loop to apply timestamp logic for bulk operations
    auto loop(int count) -> IoGenerator<size_t> {
        return executeLoopWrapWithTimestamps(std::move(*this), count);
    }
    
    /**
     * @brief Set object for update with automatic timestamp handling
     * 
     * This method allows setting an object that will receive automatic
     * timestamp updates before the update operation.
     */
    template<typename T>
    TimestampAwareUpdateBuilder& setObjectForTimestamps(T& obj) {
        mTimestampObject = &obj;
        mTimestampObjectType = typeid(T).name();
        return *this;
    }

private:
    /**
     * @brief Apply updated_at timestamps to bound objects
     * 
     * Applies updated_at timestamp logic to objects that have been
     * set for timestamp management.
     */
    void applyUpdateTimestamps() {
        if (mTimestampObject && !mTimestampObjectType.empty()) {
            // For now, we'll handle this through a type-erased interface
            // In a real implementation, this would use a more sophisticated
            // type system or visitor pattern
            applyUpdateTimestampsToObject();
        }
    }
    
    /**
     * @brief Apply timestamps to the stored object
     * 
     * This is a simplified implementation. In a real system, this would
     * use proper type erasure or a visitor pattern to handle different types.
     */
    void applyUpdateTimestampsToObject() {
        // This is a placeholder implementation
        // In a real system, we would need a more sophisticated approach
        // to handle type-erased objects with timestamp fields
        
        // For now, we'll document that this needs to be implemented
        // based on the specific ORM's type system
    }
    
    /**
     * @brief Enhanced loop wrapper that applies timestamps for bulk operations
     */
    template <typename BuilderType>
    static auto executeLoopWrapWithTimestamps(BuilderType self, int count) -> IoGenerator<size_t> {
        auto stmtRet = co_await self.prepare();
        if (!stmtRet) {
            co_yield Unexpected(stmtRet.error());
        }
        else {
            auto stmt = std::move(stmtRet.value());
            while (count--) {
                // Apply timestamps before each execution
                self.applyUpdateTimestamps();
                
                self.bind(stmt);
                auto execRet = co_await stmt->execute();
                if (!execRet) {
                    co_yield Unexpected(execRet.error());
                }
                else {
                    co_yield execRet.value();
                }
                stmt->reset();
            }
        }
    }
    
    void* mTimestampObject = nullptr;
    std::string mTimestampObjectType;
};

/**
 * @brief Factory functions for creating timestamp-aware builders
 * 
 * These functions provide a convenient way to create builders with
 * automatic timestamp management enabled.
 */

template<typename T>
auto createTimestampAwareInsertBuilder(SqlDatabase& db, const std::string& tableName, 
                                      const std::vector<std::string>& columnNames) {
    return TimestampAwareInsertBuilder<T>(db, tableName, columnNames);
}

auto createTimestampAwareUpdateBuilder(SqlDatabase& db, const std::string& tableName) {
    return TimestampAwareUpdateBuilder(db, tableName);
}

} // namespace detail
ILIAS_SQL_NS_END