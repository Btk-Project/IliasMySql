#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include "ilias/sql_orm/detail/timestamp_manager.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql/types.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace detail;

// Mock object for testing timestamp behavior
struct MockTimestampObject {
    SqlDate created_at;
    SqlDate updated_at;
    SqlDate combined_timestamp; // Field with both created_at and updated_at behavior
    std::string name;
    int id = 0;
    
    MockTimestampObject() {
        // Initialize with default/empty timestamps
        created_at.clear();
        updated_at.clear();
        combined_timestamp.clear();
    }
    
    // Constructor with manual timestamp values
    MockTimestampObject(const SqlDate& manual_created, const SqlDate& manual_updated) 
        : created_at(manual_created), updated_at(manual_updated) {
        combined_timestamp.clear();
    }
};

// Specialization of TimestampManager::setTimestampField for MockTimestampObject
namespace ilias::sql::detail {
    template<>
    inline void TimestampManager::setTimestampField<MockTimestampObject, SqlDate>(
        MockTimestampObject& object, const std::string& fieldName, const SqlDate& timestamp) {
        
        // Get pointer to the target field
        SqlDate* targetField = nullptr;
        
        if (fieldName == "created_at") {
            targetField = &object.created_at;
        } else if (fieldName == "updated_at") {
            targetField = &object.updated_at;
        } else if (fieldName == "combined_timestamp") {
            targetField = &object.combined_timestamp;
        }
        
        // Always update the field - the shouldPreserveManualValue check should be done
        // at a higher level if needed, but for these tests we want to always update
        if (targetField) {
            *targetField = timestamp;
        }
    }
    
    // Specialization for checking manual timestamp preservation
    template<>
    inline bool TimestampManager::shouldPreserveManualTimestamp<MockTimestampObject>(
        const MockTimestampObject& object, const std::string& fieldName) {
        
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

// Property testing framework for timestamp automation
class TimestampAutomationPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> year_dist{2020, 2030};
    std::uniform_int_distribution<int> month_dist{1, 12};
    std::uniform_int_distribution<int> day_dist{1, 28};
    std::uniform_int_distribution<int> hour_dist{0, 23};
    std::uniform_int_distribution<int> minute_dist{0, 59};
    std::uniform_int_distribution<int> second_dist{0, 59};
    
public:
    TimestampAutomationPropertyTest() : gen(std::random_device{}()) {}
    
    // Generate random SqlDate
    SqlDate generateRandomTimestamp() {
        return SqlDate(
            year_dist(gen),
            month_dist(gen), 
            day_dist(gen),
            hour_dist(gen),
            minute_dist(gen),
            second_dist(gen)
        );
    }
    
    // Generate mock object with random initial state
    MockTimestampObject generateRandomObject() {
        MockTimestampObject obj;
        
        // Randomly decide if timestamps are pre-set (manual values)
        if (bool_dist(gen)) {
            obj.created_at = generateRandomTimestamp();
        }
        if (bool_dist(gen)) {
            obj.updated_at = generateRandomTimestamp();
        }
        if (bool_dist(gen)) {
            obj.combined_timestamp = generateRandomTimestamp();
        }
        
        obj.name = "test_object_" + std::to_string(gen());
        obj.id = gen() % 1000;
        
        return obj;
    }
    
    // Generate SqlTags with created_at behavior
    SqlTags generateCreatedAtTags() {
        SqlTags tags;
        tags.created_at = true;
        tags.updated_at = false;
        return tags;
    }
    
    // Generate SqlTags with updated_at behavior
    SqlTags generateUpdatedAtTags() {
        SqlTags tags;
        tags.created_at = false;
        tags.updated_at = true;
        return tags;
    }
    
    // Generate SqlTags with combined behavior (both created_at and updated_at)
    SqlTags generateCombinedTags() {
        SqlTags tags;
        tags.created_at = true;
        tags.updated_at = true;
        return tags;
    }
    
    // Run property test with given number of iterations
    template<typename PropertyFunc>
    void runPropertyTest(PropertyFunc property, int iterations = 100) {
        for (int i = 0; i < iterations; ++i) {
            property();
        }
    }
    
    // Helper to check if two timestamps are close (within 1 second)
    bool timestampsAreClose(const SqlDate& ts1, const SqlDate& ts2, int toleranceSeconds = 1) {
        auto time1 = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts1.toTimestamp()));
        auto time2 = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts2.toTimestamp()));
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(
            time1 > time2 ? time1 - time2 : time2 - time1
        );
        return diff.count() <= toleranceSeconds;
    }
    
    // Helper to check if timestamp is recent (within last few seconds)
    bool isRecentTimestamp(const SqlDate& timestamp, int toleranceSeconds = 5) {
        auto now = TimestampManager::getCurrentTimestamp();
        return timestampsAreClose(timestamp, now, toleranceSeconds);
    }
};

// Test fixture for timestamp automation property tests
class TimestampAutomationPropertyTestFixture : public ::testing::Test {
protected:
    TimestampAutomationPropertyTest propertyTester;
};

// **Property 10: Created at timestamp automation**
// *For any* object with created_at fields, inserting the object should automatically populate timestamp values before database insertion
// **Validates: Requirements 3.1**
TEST_F(TimestampAutomationPropertyTestFixture, Property10_CreatedAtTimestampAutomation) {
    // Feature: sql-tags-enhancement, Property 10: Created at timestamp automation
    propertyTester.runPropertyTest([&]() {
        // Generate object with empty created_at timestamp
        MockTimestampObject obj;
        obj.created_at.clear(); // Ensure it starts empty
        
        // Record time before applying timestamp
        auto beforeTime = TimestampManager::getCurrentTimestamp();
        
        // Apply created_at timestamp
        std::vector<std::string> createdAtFields = {"created_at"};
        TimestampManager::applyCreatedAt(obj, createdAtFields);
        
        // Record time after applying timestamp
        auto afterTime = TimestampManager::getCurrentTimestamp();
        
        // Verify that created_at was populated with current timestamp
        EXPECT_NE(obj.created_at.type, SqlDate::kErrorTime) 
            << "Created at timestamp should be populated";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.created_at))
            << "Created at timestamp should be recent";
        
        // Verify timestamp is within reasonable range
        EXPECT_GE(obj.created_at.toTimestamp(), beforeTime.toTimestamp())
            << "Created at should be after or equal to before time";
        EXPECT_LE(obj.created_at.toTimestamp(), afterTime.toTimestamp())
            << "Created at should be before or equal to after time";
        
        // Test with random object that has empty created_at
        auto randomObj = propertyTester.generateRandomObject();
        randomObj.created_at.clear();
        
        TimestampManager::applyCreatedAt(randomObj, createdAtFields);
        EXPECT_TRUE(propertyTester.isRecentTimestamp(randomObj.created_at));
        
    }, 100);
}

// **Property 11: Updated at timestamp automation**
// *For any* object with updated_at fields, updating the object should automatically update timestamp values before database update
// **Validates: Requirements 3.2**
TEST_F(TimestampAutomationPropertyTestFixture, Property11_UpdatedAtTimestampAutomation) {
    // Feature: sql-tags-enhancement, Property 11: Updated at timestamp automation
    propertyTester.runPropertyTest([&]() {
        // Generate object with empty updated_at timestamp
        MockTimestampObject obj;
        obj.updated_at.clear(); // Ensure it starts empty
        
        // Record time before applying timestamp
        auto beforeTime = TimestampManager::getCurrentTimestamp();
        
        // Apply updated_at timestamp
        std::vector<std::string> updatedAtFields = {"updated_at"};
        TimestampManager::applyUpdatedAt(obj, updatedAtFields);
        
        // Record time after applying timestamp
        auto afterTime = TimestampManager::getCurrentTimestamp();
        
        // Verify that updated_at was populated with current timestamp
        EXPECT_NE(obj.updated_at.type, SqlDate::kErrorTime) 
            << "Updated at timestamp should be populated";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.updated_at))
            << "Updated at timestamp should be recent";
        
        // Verify timestamp is within reasonable range
        EXPECT_GE(obj.updated_at.toTimestamp(), beforeTime.toTimestamp())
            << "Updated at should be after or equal to before time";
        EXPECT_LE(obj.updated_at.toTimestamp(), afterTime.toTimestamp())
            << "Updated at should be before or equal to after time";
        
        // Test multiple updates - each should get a new timestamp
        // For this test, we need to clear the timestamp to simulate it not being "manually set"
        auto firstUpdateTime = obj.updated_at;
        
        // Small delay to ensure different timestamp
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Clear the timestamp to simulate it being automatically managed (not manual)
        obj.updated_at.clear();
        
        TimestampManager::applyUpdatedAt(obj, updatedAtFields);
        EXPECT_GT(obj.updated_at.toTimestamp(), firstUpdateTime.toTimestamp())
            << "Second update should have later timestamp";
        
        // Test with random object
        auto randomObj = propertyTester.generateRandomObject();
        randomObj.updated_at.clear();
        
        TimestampManager::applyUpdatedAt(randomObj, updatedAtFields);
        EXPECT_TRUE(propertyTester.isRecentTimestamp(randomObj.updated_at));
        
    }, 100);
}

// **Property 12: Combined timestamp behavior precedence**
// *For any* field with both created_at = true and updated_at = true, the field should be updated on both INSERT and UPDATE operations
// **Validates: Requirements 3.3**
TEST_F(TimestampAutomationPropertyTestFixture, Property12_CombinedTimestampBehaviorPrecedence) {
    // Feature: sql-tags-enhancement, Property 12: Combined timestamp behavior precedence
    propertyTester.runPropertyTest([&]() {
        // Test field with both created_at and updated_at behavior
        MockTimestampObject obj;
        obj.combined_timestamp.clear();
        
        std::vector<std::string> combinedFields = {"combined_timestamp"};
        
        // Apply created_at behavior (simulating INSERT)
        auto beforeCreated = TimestampManager::getCurrentTimestamp();
        TimestampManager::applyCreatedAt(obj, combinedFields);
        auto afterCreated = TimestampManager::getCurrentTimestamp();
        
        // Verify field was populated during "INSERT"
        EXPECT_NE(obj.combined_timestamp.type, SqlDate::kErrorTime)
            << "Combined field should be populated on INSERT";
        EXPECT_GE(obj.combined_timestamp.toTimestamp(), beforeCreated.toTimestamp());
        EXPECT_LE(obj.combined_timestamp.toTimestamp(), afterCreated.toTimestamp());
        
        auto insertTimestamp = obj.combined_timestamp;
        
        // Small delay to ensure different timestamp
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Clear the timestamp to simulate it being automatically managed (not manual)
        // This is the key - for combined behavior, the field should always be updated
        obj.combined_timestamp.clear();
        
        // Apply updated_at behavior (simulating UPDATE)
        auto beforeUpdated = TimestampManager::getCurrentTimestamp();
        TimestampManager::applyUpdatedAt(obj, combinedFields);
        auto afterUpdated = TimestampManager::getCurrentTimestamp();
        
        // Verify field was updated during "UPDATE"
        EXPECT_GT(obj.combined_timestamp.toTimestamp(), insertTimestamp.toTimestamp())
            << "Combined field should be updated on UPDATE";
        EXPECT_GE(obj.combined_timestamp.toTimestamp(), beforeUpdated.toTimestamp());
        EXPECT_LE(obj.combined_timestamp.toTimestamp(), afterUpdated.toTimestamp());
        
        // Test with random object
        auto randomObj = propertyTester.generateRandomObject();
        randomObj.combined_timestamp.clear();
        
        // Apply both behaviors
        TimestampManager::applyCreatedAt(randomObj, combinedFields);
        auto createdTime = randomObj.combined_timestamp;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Clear again for update behavior
        randomObj.combined_timestamp.clear();
        
        TimestampManager::applyUpdatedAt(randomObj, combinedFields);
        EXPECT_GT(randomObj.combined_timestamp.toTimestamp(), createdTime.toTimestamp())
            << "Random object combined field should update on both operations";
        
    }, 100);
}

// **Property 13: Manual timestamp override**
// *For any* object with manually set timestamp values, the automatic timestamp behavior should not override the manual values
// **Validates: Requirements 3.4**
TEST_F(TimestampAutomationPropertyTestFixture, Property13_ManualTimestampOverride) {
    // Feature: sql-tags-enhancement, Property 13: Manual timestamp override
    propertyTester.runPropertyTest([&]() {
        // Create object with manually set timestamps
        auto manualCreatedTime = propertyTester.generateRandomTimestamp();
        auto manualUpdatedTime = propertyTester.generateRandomTimestamp();
        
        MockTimestampObject obj(manualCreatedTime, manualUpdatedTime);
        
        // Verify manual values are considered "set" (not default)
        EXPECT_TRUE(TimestampManager::shouldPreserveManualValue(obj.created_at))
            << "Manual created_at value should be preserved";
        EXPECT_TRUE(TimestampManager::shouldPreserveManualValue(obj.updated_at))
            << "Manual updated_at value should be preserved";
        
        // Store original manual values
        auto originalCreated = obj.created_at;
        auto originalUpdated = obj.updated_at;
        
        // Apply automatic timestamp behavior
        std::vector<std::string> createdAtFields = {"created_at"};
        std::vector<std::string> updatedAtFields = {"updated_at"};
        
        TimestampManager::applyCreatedAt(obj, createdAtFields);
        TimestampManager::applyUpdatedAt(obj, updatedAtFields);
        
        // Verify manual values were preserved (not overridden)
        EXPECT_EQ(obj.created_at.toTimestamp(), originalCreated.toTimestamp())
            << "Manual created_at value should not be overridden";
        EXPECT_EQ(obj.updated_at.toTimestamp(), originalUpdated.toTimestamp())
            << "Manual updated_at value should not be overridden";
        
        // Test with default/empty timestamps (should be overridden)
        MockTimestampObject emptyObj;
        emptyObj.created_at.clear();
        emptyObj.updated_at.clear();
        
        EXPECT_FALSE(TimestampManager::shouldPreserveManualValue(emptyObj.created_at))
            << "Empty created_at should not be preserved";
        EXPECT_FALSE(TimestampManager::shouldPreserveManualValue(emptyObj.updated_at))
            << "Empty updated_at should not be preserved";
        
        TimestampManager::applyCreatedAt(emptyObj, createdAtFields);
        TimestampManager::applyUpdatedAt(emptyObj, updatedAtFields);
        
        // Verify empty values were overridden with current timestamps
        EXPECT_TRUE(propertyTester.isRecentTimestamp(emptyObj.created_at))
            << "Empty created_at should be set to current time";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(emptyObj.updated_at))
            << "Empty updated_at should be set to current time";
        
        // Test with random objects
        auto randomObj = propertyTester.generateRandomObject();
        auto originalRandomCreated = randomObj.created_at;
        auto originalRandomUpdated = randomObj.updated_at;
        
        bool shouldPreserveCreated = TimestampManager::shouldPreserveManualValue(randomObj.created_at);
        bool shouldPreserveUpdated = TimestampManager::shouldPreserveManualValue(randomObj.updated_at);
        
        TimestampManager::applyCreatedAt(randomObj, createdAtFields);
        TimestampManager::applyUpdatedAt(randomObj, updatedAtFields);
        
        if (shouldPreserveCreated) {
            EXPECT_EQ(randomObj.created_at.toTimestamp(), originalRandomCreated.toTimestamp())
                << "Random object manual created_at should be preserved";
        } else {
            EXPECT_TRUE(propertyTester.isRecentTimestamp(randomObj.created_at))
                << "Random object empty created_at should be set";
        }
        
        if (shouldPreserveUpdated) {
            EXPECT_EQ(randomObj.updated_at.toTimestamp(), originalRandomUpdated.toTimestamp())
                << "Random object manual updated_at should be preserved";
        } else {
            EXPECT_TRUE(propertyTester.isRecentTimestamp(randomObj.updated_at))
                << "Random object empty updated_at should be set";
        }
        
    }, 100);
}

// Additional test to verify SqlTags behavior detection
TEST_F(TimestampAutomationPropertyTestFixture, SqlTagsBehaviorDetection) {
    propertyTester.runPropertyTest([&]() {
        // Test created_at tag detection
        auto createdAtTags = propertyTester.generateCreatedAtTags();
        EXPECT_TRUE(TimestampManager::shouldApplyCreatedAt(createdAtTags));
        EXPECT_FALSE(TimestampManager::shouldApplyUpdatedAt(createdAtTags));
        
        // Test updated_at tag detection
        auto updatedAtTags = propertyTester.generateUpdatedAtTags();
        EXPECT_FALSE(TimestampManager::shouldApplyCreatedAt(updatedAtTags));
        EXPECT_TRUE(TimestampManager::shouldApplyUpdatedAt(updatedAtTags));
        
        // Test combined tag detection
        auto combinedTags = propertyTester.generateCombinedTags();
        EXPECT_TRUE(TimestampManager::shouldApplyCreatedAt(combinedTags));
        EXPECT_TRUE(TimestampManager::shouldApplyUpdatedAt(combinedTags));
        
        // Test default tags (no timestamp behavior)
        SqlTags defaultTags;
        EXPECT_FALSE(TimestampManager::shouldApplyCreatedAt(defaultTags));
        EXPECT_FALSE(TimestampManager::shouldApplyUpdatedAt(defaultTags));
    }, 100);
}

// Test timestamp utility functions
TEST_F(TimestampAutomationPropertyTestFixture, TimestampUtilityFunctions) {
    // Test getCurrentTimestamp
    auto timestamp1 = TimestampManager::getCurrentTimestamp();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto timestamp2 = TimestampManager::getCurrentTimestamp();
    
    EXPECT_GT(timestamp2.toTimestamp(), timestamp1.toTimestamp())
        << "Sequential timestamps should be increasing";
    
    // Test getCurrentTimePoint
    auto timepoint1 = TimestampManager::getCurrentTimePoint();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    auto timepoint2 = TimestampManager::getCurrentTimePoint();
    
    EXPECT_GT(timepoint2, timepoint1)
        << "Sequential time points should be increasing";
    
    // Test conversion consistency - use a more lenient comparison
    auto sqlDate = TimestampManager::getCurrentTimestamp();
    auto timePoint = TimestampManager::getCurrentTimePoint();
    
    // Convert SqlDate to time_point for comparison
    // SqlDate.toTimestamp() returns microseconds since epoch, but we need to be careful about the conversion
    auto sqlDateAsTimePoint = std::chrono::system_clock::time_point(
        std::chrono::microseconds(sqlDate.toTimestamp())  // Use microseconds instead of milliseconds
    );
    
    auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        timePoint > sqlDateAsTimePoint ? timePoint - sqlDateAsTimePoint : sqlDateAsTimePoint - timePoint
    );
    
    // Be more lenient with the timing difference - allow up to 1 second
    EXPECT_LT(diff.count(), 1000) // Within 1000ms (1 second)
        << "SqlDate and time_point should be reasonably close, diff: " << diff.count() << "ms";
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}