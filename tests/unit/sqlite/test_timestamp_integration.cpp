#include <gtest/gtest.h>
#include <string>
#include <chrono>
#include <thread>
#include "ilias/sql_orm/detail/orm_builder.hpp"
#include "ilias/sql_orm/detail/timestamp_manager.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql/types.hpp"
#include "ilias/sql/sqldatabase.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace detail;

// Test object with timestamp fields
struct TestTimestampObject {
    int id = 0;
    std::string name;
    SqlDate created_at;
    SqlDate updated_at;
    SqlDate combined_timestamp; // Field with both created_at and updated_at behavior
    
    TestTimestampObject() {
        created_at.clear();
        updated_at.clear();
        combined_timestamp.clear();
    }
    
    TestTimestampObject(int id_val, const std::string& name_val) 
        : id(id_val), name(name_val) {
        created_at.clear();
        updated_at.clear();
        combined_timestamp.clear();
    }
};

// Metadata for TestTimestampObject
NEKO_BEGIN_NAMESPACE
template <>
struct Meta<TestTimestampObject, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true, .not_null = true}>(&TestTimestampObject::id),
        "name", make_tags<SqlTags{.not_null = true}>(&TestTimestampObject::name),
        "created_at", make_tags<SqlTags{.created_at = true}>(&TestTimestampObject::created_at),
        "updated_at", make_tags<SqlTags{.updated_at = true}>(&TestTimestampObject::updated_at),
        "combined_timestamp", make_tags<SqlTags{.created_at = true, .updated_at = true}>(&TestTimestampObject::combined_timestamp)
    );
};
NEKO_END_NAMESPACE

// Specialization of TimestampManager::setTimestampField for TestTimestampObject
namespace ilias::sql::detail {
    template<>
    inline void TimestampManager::setTimestampField<TestTimestampObject, SqlDate>(
        TestTimestampObject& object, const std::string& fieldName, const SqlDate& timestamp) {
        
        if (fieldName == "created_at") {
            object.created_at = timestamp;
        } else if (fieldName == "updated_at") {
            object.updated_at = timestamp;
        } else if (fieldName == "combined_timestamp") {
            object.combined_timestamp = timestamp;
        }
    }
    
    template<>
    inline bool TimestampManager::shouldPreserveManualTimestamp<TestTimestampObject>(
        const TestTimestampObject& object, const std::string& fieldName) {
        
        if (fieldName == "created_at") {
            return shouldPreserveManualValue(object.created_at);
        } else if (fieldName == "updated_at") {
            return shouldPreserveManualValue(object.updated_at);
        } else if (fieldName == "combined_timestamp") {
            return shouldPreserveManualValue(object.combined_timestamp);
        }
        
        return false;
    }
}

class TimestampIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Helper to check if timestamp is recent (within last few seconds)
    }
    
    bool isRecentTimestamp(const SqlDate& timestamp, int toleranceSeconds = 5) {
        auto now = TimestampManager::getCurrentTimestamp();
        auto time1 = std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp.toTimestamp()));
        auto time2 = std::chrono::system_clock::time_point(std::chrono::microseconds(now.toTimestamp()));
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(
            time1 > time2 ? time1 - time2 : time2 - time1
        );
        return diff.count() <= toleranceSeconds;
    }
};

// Test InsertBuilder timestamp integration
TEST_F(TimestampIntegrationTest, InsertBuilderAppliesCreatedAtTimestamps) {
    // Create test object with empty timestamps
    TestTimestampObject obj(1, "test_object");
    EXPECT_EQ(obj.created_at.type, SqlDate::kErrorTime) << "Initial created_at should be empty";
    EXPECT_EQ(obj.combined_timestamp.type, SqlDate::kErrorTime) << "Initial combined_timestamp should be empty";
    
    // Create InsertBuilder (we don't need a real database for this test)
    // We'll test the timestamp application logic directly
    std::vector<std::string> columns = {"id", "name", "created_at", "updated_at", "combined_timestamp"};
    
    // Simulate what InsertBuilder::set() does with timestamp integration
    // We can't easily test the full InsertBuilder without a database, so we'll test the core logic
    
    // Test the timestamp field detection
    auto createdAtFields = InsertBuilder<TestTimestampObject>::getCreatedAtFields<TestTimestampObject>();
    
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "created_at") != createdAtFields.end())
        << "Should detect created_at field";
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "combined_timestamp") != createdAtFields.end())
        << "Should detect combined_timestamp field for created_at";
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "updated_at") == createdAtFields.end())
        << "Should NOT detect updated_at field for created_at";
    
    // Apply timestamps manually (simulating what InsertBuilder does)
    TimestampManager::applyCreatedAt(obj, createdAtFields);
    
    // Verify timestamps were applied
    EXPECT_TRUE(isRecentTimestamp(obj.created_at)) << "created_at should be set to current time";
    EXPECT_TRUE(isRecentTimestamp(obj.combined_timestamp)) << "combined_timestamp should be set to current time";
    EXPECT_EQ(obj.updated_at.type, SqlDate::kErrorTime) << "updated_at should remain empty for insert";
}

// Test UpdateBuilder timestamp integration
TEST_F(TimestampIntegrationTest, UpdateBuilderAppliesUpdatedAtTimestamps) {
    // Create test object with empty timestamps
    TestTimestampObject obj(1, "test_object");
    EXPECT_EQ(obj.updated_at.type, SqlDate::kErrorTime) << "Initial updated_at should be empty";
    EXPECT_EQ(obj.combined_timestamp.type, SqlDate::kErrorTime) << "Initial combined_timestamp should be empty";
    
    // Test the timestamp field detection
    auto updatedAtFields = UpdateBuilder::getUpdatedAtFields<TestTimestampObject>();
    
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "updated_at") != updatedAtFields.end())
        << "Should detect updated_at field";
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "combined_timestamp") != updatedAtFields.end())
        << "Should detect combined_timestamp field for updated_at";
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "created_at") == updatedAtFields.end())
        << "Should NOT detect created_at field for updated_at";
    
    // Apply timestamps manually (simulating what UpdateBuilder does)
    TimestampManager::applyUpdatedAt(obj, updatedAtFields);
    
    // Verify timestamps were applied
    EXPECT_TRUE(isRecentTimestamp(obj.updated_at)) << "updated_at should be set to current time";
    EXPECT_TRUE(isRecentTimestamp(obj.combined_timestamp)) << "combined_timestamp should be set to current time";
    EXPECT_EQ(obj.created_at.type, SqlDate::kErrorTime) << "created_at should remain empty for update";
}

// Test combined timestamp behavior
TEST_F(TimestampIntegrationTest, CombinedTimestampBehavior) {
    TestTimestampObject obj(1, "test_object");
    
    // Test created_at behavior on combined field
    auto createdAtFields = InsertBuilder<TestTimestampObject>::getCreatedAtFields<TestTimestampObject>();
    TimestampManager::applyCreatedAt(obj, createdAtFields);
    
    auto insertTime = obj.combined_timestamp;
    EXPECT_TRUE(isRecentTimestamp(insertTime)) << "Combined field should be set during insert";
    
    // Small delay to ensure different timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Clear the timestamp to simulate it being automatically managed (not manual)
    obj.combined_timestamp.clear();
    
    // Test updated_at behavior on combined field
    auto updatedAtFields = UpdateBuilder::getUpdatedAtFields<TestTimestampObject>();
    TimestampManager::applyUpdatedAt(obj, updatedAtFields);
    
    EXPECT_GT(obj.combined_timestamp.toTimestamp(), insertTime.toTimestamp())
        << "Combined field should be updated during update operation";
}

// Test manual timestamp preservation
TEST_F(TimestampIntegrationTest, ManualTimestampPreservation) {
    TestTimestampObject obj(1, "test_object");
    
    // Set manual timestamps
    obj.created_at = SqlDate(2023, 1, 1, 12, 0, 0);
    obj.updated_at = SqlDate(2023, 6, 15, 14, 30, 0);
    
    auto originalCreated = obj.created_at;
    auto originalUpdated = obj.updated_at;
    
    // Apply automatic timestamps
    auto createdAtFields = InsertBuilder<TestTimestampObject>::getCreatedAtFields<TestTimestampObject>();
    auto updatedAtFields = UpdateBuilder::getUpdatedAtFields<TestTimestampObject>();
    
    TimestampManager::applyCreatedAt(obj, createdAtFields);
    TimestampManager::applyUpdatedAt(obj, updatedAtFields);
    
    // Verify manual values were preserved
    EXPECT_EQ(obj.created_at.toTimestamp(), originalCreated.toTimestamp())
        << "Manual created_at should be preserved";
    EXPECT_EQ(obj.updated_at.toTimestamp(), originalUpdated.toTimestamp())
        << "Manual updated_at should be preserved";
}

// Test field detection accuracy
TEST_F(TimestampIntegrationTest, FieldDetectionAccuracy) {
    // Test created_at field detection
    auto createdAtFields = InsertBuilder<TestTimestampObject>::getCreatedAtFields<TestTimestampObject>();
    EXPECT_EQ(createdAtFields.size(), 2) << "Should detect exactly 2 created_at fields";
    
    // Test updated_at field detection
    auto updatedAtFields = UpdateBuilder::getUpdatedAtFields<TestTimestampObject>();
    EXPECT_EQ(updatedAtFields.size(), 2) << "Should detect exactly 2 updated_at fields";
    
    // Verify specific fields
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "created_at") != createdAtFields.end());
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "combined_timestamp") != createdAtFields.end());
    
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "updated_at") != updatedAtFields.end());
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "combined_timestamp") != updatedAtFields.end());
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}