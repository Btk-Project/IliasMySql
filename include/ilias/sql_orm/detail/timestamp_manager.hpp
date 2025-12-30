#pragma once

#include "ilias/sql/global/global.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include <chrono>
#include <vector>
#include <string>
#include <type_traits>

ILIAS_SQL_NS_BEGIN
namespace detail {

/**
 * @brief TimestampManager handles automatic timestamp field management for ORM operations
 * 
 * This class provides functionality to automatically populate created_at and updated_at
 * timestamp fields during INSERT and UPDATE operations. It supports various timestamp
 * types including SqlDate and std::chrono types.
 */
class ILIAS_SQL_API TimestampManager {
public:
    /**
     * @brief Apply created_at timestamp to object fields
     * 
     * Sets the current timestamp on all fields marked with created_at = true.
     * This method should be called before INSERT operations.
     * 
     * @tparam T The object type
     * @param object The object to modify
     * @param createdAtFields List of field names that should receive created_at timestamps
     */
    template<typename T>
    static void applyCreatedAt(T& object, const std::vector<std::string>& createdAtFields) {
        auto currentTime = getCurrentTimestamp();
        
        for (const auto& fieldName : createdAtFields) {
            // Check if we should preserve manual values before setting
            if (!shouldPreserveManualTimestamp<T>(object, fieldName)) {
                setTimestampField<T, SqlDate>(object, fieldName, currentTime);
            }
        }
    }
    
    /**
     * @brief Apply updated_at timestamp to object fields
     * 
     * Sets the current timestamp on all fields marked with updated_at = true.
     * This method should be called before UPDATE operations.
     * 
     * @tparam T The object type
     * @param object The object to modify
     * @param updatedAtFields List of field names that should receive updated_at timestamps
     */
    template<typename T>
    static void applyUpdatedAt(T& object, const std::vector<std::string>& updatedAtFields) {
        auto currentTime = getCurrentTimestamp();
        
        for (const auto& fieldName : updatedAtFields) {
            // Check if we should preserve manual values before setting
            if (!shouldPreserveManualTimestamp<T>(object, fieldName)) {
                setTimestampField<T, SqlDate>(object, fieldName, currentTime);
            }
        }
    }
    
    /**
     * @brief Get list of timestamp field names from SqlTags configuration
     * 
     * Extracts field names that have timestamp behavior (created_at or updated_at flags).
     * 
     * @tparam T The object type (used for reflection/metadata)
     * @param createdAt If true, include fields marked with created_at = true
     * @param updatedAt If true, include fields marked with updated_at = true
     * @return Vector of field names that have the specified timestamp behavior
     */
    template<typename T>
    static std::vector<std::string> getTimestampFields(bool createdAt, bool updatedAt) {
        std::vector<std::string> fields;
        
        // In a real implementation, this would use reflection/metadata to iterate
        // through all fields of type T and check their SqlTags configuration
        
        // Placeholder implementation - would need actual reflection system
        // This is where the ORM's metadata system would be integrated
        
        return fields;
    }
    
    /**
     * @brief Get current timestamp as SqlDate
     * 
     * @return Current timestamp in SqlDate format
     */
    static SqlDate getCurrentTimestamp();
    
    /**
     * @brief Get current timestamp as std::chrono::system_clock::time_point
     * 
     * @return Current timestamp as chrono time_point
     */
    static std::chrono::system_clock::time_point getCurrentTimePoint();
    
    /**
     * @brief Check if a field should receive created_at timestamp
     * 
     * @param tags The SqlTags configuration for the field
     * @return true if field should receive created_at timestamp
     */
    static bool shouldApplyCreatedAt(const SqlTags& tags);
    
    /**
     * @brief Check if a field should receive updated_at timestamp
     * 
     * @param tags The SqlTags configuration for the field
     * @return true if field should receive updated_at timestamp
     */
    static bool shouldApplyUpdatedAt(const SqlTags& tags);
    
    /**
     * @brief Check if manual timestamp value should be preserved
     * 
     * Determines if a field already has a manually set timestamp value
     * that should not be overridden by automatic timestamp behavior.
     * 
     * @tparam TimestampType The type of the timestamp field
     * @param value The current value of the timestamp field
     * @return true if the manual value should be preserved
     */
    template<typename TimestampType>
    static bool shouldPreserveManualValue(const TimestampType& value);
    
    /**
     * @brief Check if manual timestamp should be preserved for a specific field
     * 
     * @tparam T The object type
     * @param object The object to check
     * @param fieldName The name of the field to check
     * @return true if the manual value should be preserved
     */
    template<typename T>
    static bool shouldPreserveManualTimestamp(const T& object, const std::string& fieldName) {
        // Default implementation - would use reflection in a real system
        return false;
    }

private:
    /**
     * @brief Set timestamp value on a field using reflection/metadata
     * 
     * @tparam T The object type
     * @tparam TimestampType The timestamp field type
     * @param object The object to modify
     * @param fieldName The name of the field to set
     * @param timestamp The timestamp value to set
     */
    template<typename T, typename TimestampType>
    static void setTimestampField(T& object, const std::string& fieldName, const TimestampType& timestamp) {
        // Default implementation - would use reflection in a real system
        // For now, we provide a basic implementation that can be specialized
        
        // This is a placeholder that would be replaced by actual reflection/metadata system
        // In a real ORM, this would use the metadata system to set fields by name
    }
    
    /**
     * @brief Get timestamp value from a field using reflection/metadata
     * 
     * @tparam T The object type
     * @tparam TimestampType The timestamp field type
     * @param object The object to read from
     * @param fieldName The name of the field to read
     * @return The current timestamp value of the field
     */
    template<typename T, typename TimestampType>
    static TimestampType getTimestampField(const T& object, const std::string& fieldName) {
        // Placeholder for reflection-based field getting
        // In a real implementation, this would use the ORM's reflection system
        // to dynamically get the field value by name
        
        return TimestampType{};
    }
};

// Template specializations for different timestamp types

/**
 * @brief Specialization for SqlDate timestamp type
 */
template<>
inline bool TimestampManager::shouldPreserveManualValue<SqlDate>(const SqlDate& value) {
    // Consider a SqlDate as manually set if it's not the default/error time
    return value.type != SqlDate::kErrorTime && 
           (value.year != 0 || value.month != 0 || value.day != 0 ||
            value.hour != 0 || value.minute != 0 || value.second != 0);
}

/**
 * @brief Specialization for std::chrono::system_clock::time_point
 */
template<>
inline bool TimestampManager::shouldPreserveManualValue<std::chrono::system_clock::time_point>(
    const std::chrono::system_clock::time_point& value) {
    // Consider a time_point as manually set if it's not the epoch (default constructed)
    return value != std::chrono::system_clock::time_point{};
}

/**
 * @brief Specialization for std::chrono::milliseconds
 */
template<>
inline bool TimestampManager::shouldPreserveManualValue<std::chrono::milliseconds>(
    const std::chrono::milliseconds& value) {
    // Consider milliseconds as manually set if it's not zero
    return value.count() != 0;
}

} // namespace detail
ILIAS_SQL_NS_END