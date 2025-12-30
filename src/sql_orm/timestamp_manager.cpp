#include "ilias/sql_orm/detail/timestamp_manager.hpp"
#include <chrono>

ILIAS_SQL_NS_BEGIN
namespace detail {

SqlDate TimestampManager::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return SqlDate(now);
}

std::chrono::system_clock::time_point TimestampManager::getCurrentTimePoint() {
    return std::chrono::system_clock::now();
}

bool TimestampManager::shouldApplyCreatedAt(const SqlTags& tags) {
    return tags.created_at;
}

bool TimestampManager::shouldApplyUpdatedAt(const SqlTags& tags) {
    return tags.updated_at;
}

// Template implementations - these would typically be in the header for templates,
// but for demonstration purposes, we'll provide basic implementations here.
// In a real implementation, these would use reflection/metadata systems to access fields.

// Explicit template instantiations for common types
// These would be expanded based on the actual ORM types used in the system

// Note: Template implementations are now in the header file

} // namespace detail
ILIAS_SQL_NS_END