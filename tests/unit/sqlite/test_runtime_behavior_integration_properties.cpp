#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <memory>
#include "ilias/sql_orm/detail/timestamp_manager.hpp"
#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql/types.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace detail;

// Mock object for testing runtime behavior integration
struct MockRuntimeObject {
    SqlDate created_at;
    SqlDate updated_at;
    SqlDate combined_timestamp; // Field with both created_at and updated_at behavior
    std::string name;
    int id = 0;
    
    MockRuntimeObject() {
        // Initialize with default/empty timestamps
        created_at.clear();
        updated_at.clear();
        combined_timestamp.clear();
    }
    
    // Constructor with manual timestamp values
    MockRuntimeObject(const SqlDate& manual_created, const SqlDate& manual_updated) 
        : created_at(manual_created), updated_at(manual_updated) {
        combined_timestamp.clear();
    }
    
    // Constructor with all fields
    MockRuntimeObject(int id_val, const std::string& name_val) 
        : name(name_val), id(id_val) {
        created_at.clear();
        updated_at.clear();
        combined_timestamp.clear();
    }
};

// Mock SqlTags configuration for fields
struct MockFieldConfig {
    std::string fieldName;
    SqlTags tags;
    
    MockFieldConfig(const std::string& name, const SqlTags& config) 
        : fieldName(name), tags(config) {}
};

// Mock ORM metadata system for testing
class MockORMMetadata {
public:
    static std::vector<MockFieldConfig> getFieldConfigs() {
        std::vector<MockFieldConfig> configs;
        
        // created_at field configuration
        SqlTags createdAtTags;
        createdAtTags.created_at = true;
        createdAtTags.updated_at = false;
        configs.emplace_back("created_at", createdAtTags);
        
        // updated_at field configuration
        SqlTags updatedAtTags;
        updatedAtTags.created_at = false;
        updatedAtTags.updated_at = true;
        configs.emplace_back("updated_at", updatedAtTags);
        
        // combined_timestamp field configuration
        SqlTags combinedTags;
        combinedTags.created_at = true;
        combinedTags.updated_at = true;
        configs.emplace_back("combined_timestamp", combinedTags);
        
        return configs;
    }
    
    static std::vector<std::string> getCreatedAtFields() {
        std::vector<std::string> fields;
        for (const auto& config : getFieldConfigs()) {
            if (config.tags.created_at) {
                fields.push_back(config.fieldName);
            }
        }
        return fields;
    }
    
    static std::vector<std::string> getUpdatedAtFields() {
        std::vector<std::string> fields;
        for (const auto& config : getFieldConfigs()) {
            if (config.tags.updated_at) {
                fields.push_back(config.fieldName);
            }
        }
        return fields;
    }
};

// Enhanced InsertBuilder simulation for timestamp integration testing
template<typename T>
class MockTimestampAwareInsertBuilder {
public:
    MockTimestampAwareInsertBuilder(const std::string& tableName, 
                                   const std::vector<std::string>& columnNames)
        : mTableName(tableName), mColumnNames(columnNames) {}
    
    // Simulate execute with timestamp logic
    size_t execute() {
        // Apply created_at timestamps before insertion
        applyInsertTimestamps();
        
        // Simulate successful insertion
        return 1;
    }
    
    // Simulate bulk operation with timestamp handling
    std::vector<size_t> loop(int count) {
        std::vector<size_t> results;
        
        // Apply timestamps for each iteration
        for (int i = 0; i < count; ++i) {
            applyInsertTimestamps();
            results.push_back(1); // Simulate successful insertion
        }
        
        return results;
    }
    
    // Set object for insertion with timestamp handling
    template<typename U>
    MockTimestampAwareInsertBuilder& setObject(U& obj) {
        mCurrentObject = &obj;
        return *this;
    }
    
private:
    void applyInsertTimestamps() {
        if (mCurrentObject) {
            auto createdAtFields = MockORMMetadata::getCreatedAtFields();
            TimestampManager::applyCreatedAt(*mCurrentObject, createdAtFields);
        }
    }
    
    std::string mTableName;
    std::vector<std::string> mColumnNames;
    T* mCurrentObject = nullptr;
};

// Enhanced UpdateBuilder simulation for timestamp integration testing
class MockTimestampAwareUpdateBuilder {
public:
    MockTimestampAwareUpdateBuilder(const std::string& tableName)
        : mTableName(tableName) {}
    
    // Simulate execute with timestamp logic
    size_t execute() {
        // Apply updated_at timestamps before update
        applyUpdateTimestamps();
        
        // Simulate successful update
        return 1;
    }
    
    // Simulate bulk operation with timestamp handling
    std::vector<size_t> loop(int count) {
        std::vector<size_t> results;
        
        // Apply timestamps for each iteration
        for (int i = 0; i < count; ++i) {
            applyUpdateTimestamps();
            results.push_back(1); // Simulate successful update
        }
        
        return results;
    }
    
    // Set object for update with timestamp handling
    template<typename T>
    MockTimestampAwareUpdateBuilder& setObject(T& obj) {
        mCurrentObject = &obj;
        return *this;
    }
    
private:
    void applyUpdateTimestamps() {
        if (mCurrentObject) {
            auto updatedAtFields = MockORMMetadata::getUpdatedAtFields();
            // Cast to MockRuntimeObject for testing
            auto* mockObj = static_cast<MockRuntimeObject*>(mCurrentObject);
            TimestampManager::applyUpdatedAt(*mockObj, updatedAtFields);
        }
    }
    
    std::string mTableName;
    void* mCurrentObject = nullptr;
};

// Specialization of TimestampManager::setTimestampField for MockRuntimeObject
namespace ilias::sql::detail {
    template<>
    inline void TimestampManager::setTimestampField<MockRuntimeObject, SqlDate>(
        MockRuntimeObject& object, const std::string& fieldName, const SqlDate& timestamp) {
        
        if (fieldName == "created_at") {
            object.created_at = timestamp;
        } else if (fieldName == "updated_at") {
            object.updated_at = timestamp;
        } else if (fieldName == "combined_timestamp") {
            object.combined_timestamp = timestamp;
        }
    }
    
    template<>
    inline bool TimestampManager::shouldPreserveManualTimestamp<MockRuntimeObject>(
        const MockRuntimeObject& object, const std::string& fieldName) {
        
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

// Property testing framework for runtime behavior integration
class RuntimeBehaviorIntegrationPropertyTest {
private:
    std::mt19937 gen;
    std::uniform_int_distribution<int> bool_dist{0, 1};
    std::uniform_int_distribution<int> id_dist{1, 10000};
    std::uniform_int_distribution<int> count_dist{1, 10};
    
public:
    RuntimeBehaviorIntegrationPropertyTest() : gen(std::random_device{}()) {}
    
    // Generate random MockRuntimeObject
    MockRuntimeObject generateRandomObject() {
        MockRuntimeObject obj;
        obj.id = id_dist(gen);
        obj.name = "test_object_" + std::to_string(obj.id);
        
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
        
        return obj;
    }
    
    // Generate random SqlDate
    SqlDate generateRandomTimestamp() {
        std::uniform_int_distribution<int> year_dist{2020, 2030};
        std::uniform_int_distribution<int> month_dist{1, 12};
        std::uniform_int_distribution<int> day_dist{1, 28};
        std::uniform_int_distribution<int> hour_dist{0, 23};
        std::uniform_int_distribution<int> minute_dist{0, 59};
        std::uniform_int_distribution<int> second_dist{0, 59};
        
        return SqlDate(
            year_dist(gen),
            month_dist(gen), 
            day_dist(gen),
            hour_dist(gen),
            minute_dist(gen),
            second_dist(gen)
        );
    }
    
    // Generate random bulk operation count
    int generateBulkCount() {
        return count_dist(gen);
    }
    
    // Run property test with given number of iterations
    template<typename PropertyFunc>
    void runPropertyTest(PropertyFunc property, int iterations = 100) {
        for (int i = 0; i < iterations; ++i) {
            property();
        }
    }
    
    // Helper to check if timestamp is recent (within last few seconds)
    bool isRecentTimestamp(const SqlDate& timestamp, int toleranceSeconds = 5) {
        auto now = TimestampManager::getCurrentTimestamp();
        auto time1 = std::chrono::system_clock::time_point(std::chrono::microseconds(timestamp.toTimestamp()));
        auto time2 = std::chrono::system_clock::time_point(std::chrono::microseconds(now.toTimestamp()));
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(
            time1 > time2 ? time1 - time2 : time2 - time1
        );
        return diff.count() <= toleranceSeconds;
    }
    
    // Helper to check if two timestamps are close (within tolerance)
    bool timestampsAreClose(const SqlDate& ts1, const SqlDate& ts2, int toleranceSeconds = 1) {
        auto time1 = std::chrono::system_clock::time_point(std::chrono::microseconds(ts1.toTimestamp()));
        auto time2 = std::chrono::system_clock::time_point(std::chrono::microseconds(ts2.toTimestamp()));
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(
            time1 > time2 ? time1 - time2 : time2 - time1
        );
        return diff.count() <= toleranceSeconds;
    }
};

// Test fixture for runtime behavior integration property tests
class RuntimeBehaviorIntegrationPropertyTestFixture : public ::testing::Test {
protected:
    RuntimeBehaviorIntegrationPropertyTest propertyTester;
};

// **Property 27: Insert timestamp population**
// *For any* insert operation on objects with created_at fields, timestamp values should be automatically populated before database insertion
// **Validates: Requirements 7.1**
TEST_F(RuntimeBehaviorIntegrationPropertyTestFixture, Property27_InsertTimestampPopulation) {
    // Feature: sql-tags-enhancement, Property 27: Insert timestamp population
    propertyTester.runPropertyTest([&]() {
        // Generate object with empty created_at timestamps
        MockRuntimeObject obj = propertyTester.generateRandomObject();
        obj.created_at.clear(); // Ensure created_at starts empty
        obj.combined_timestamp.clear(); // Ensure combined field starts empty
        
        // Store original updated_at (should not be affected by insert)
        auto originalUpdatedAt = obj.updated_at;
        
        // Record time before insert operation
        auto beforeTime = TimestampManager::getCurrentTimestamp();
        
        // Simulate insert operation with timestamp integration
        auto createdAtFields = MockORMMetadata::getCreatedAtFields();
        TimestampManager::applyCreatedAt(obj, createdAtFields);
        
        // Record time after insert operation
        auto afterTime = TimestampManager::getCurrentTimestamp();
        
        // Verify created_at field was populated
        EXPECT_NE(obj.created_at.type, SqlDate::kErrorTime) 
            << "Created at timestamp should be populated during insert";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.created_at))
            << "Created at timestamp should be recent";
        EXPECT_GE(obj.created_at.toTimestamp(), beforeTime.toTimestamp())
            << "Created at should be after or equal to before time";
        EXPECT_LE(obj.created_at.toTimestamp(), afterTime.toTimestamp())
            << "Created at should be before or equal to after time";
        
        // Verify combined_timestamp field was also populated (has created_at behavior)
        EXPECT_NE(obj.combined_timestamp.type, SqlDate::kErrorTime)
            << "Combined timestamp should be populated during insert";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.combined_timestamp))
            << "Combined timestamp should be recent";
        
        // Verify updated_at field was NOT affected by insert operation
        EXPECT_EQ(obj.updated_at.toTimestamp(), originalUpdatedAt.toTimestamp())
            << "Updated at should not be modified during insert operation";
        
        // Test with object that has manual created_at value (should be preserved)
        MockRuntimeObject manualObj = propertyTester.generateRandomObject();
        auto manualCreatedAt = propertyTester.generateRandomTimestamp();
        manualObj.created_at = manualCreatedAt;
        
        TimestampManager::applyCreatedAt(manualObj, createdAtFields);
        
        // Manual value should be preserved
        EXPECT_EQ(manualObj.created_at.toTimestamp(), manualCreatedAt.toTimestamp())
            << "Manual created_at value should be preserved during insert";
        
    }, 100);
}

// **Property 28: Update timestamp population**
// *For any* update operation on objects with updated_at fields, timestamp values should be automatically updated before database update
// **Validates: Requirements 7.2**
TEST_F(RuntimeBehaviorIntegrationPropertyTestFixture, Property28_UpdateTimestampPopulation) {
    // Feature: sql-tags-enhancement, Property 28: Update timestamp population
    propertyTester.runPropertyTest([&]() {
        // Generate object with empty updated_at timestamps
        MockRuntimeObject obj = propertyTester.generateRandomObject();
        obj.updated_at.clear(); // Ensure updated_at starts empty
        obj.combined_timestamp.clear(); // Ensure combined field starts empty
        
        // Store original created_at (should not be affected by update)
        auto originalCreatedAt = obj.created_at;
        
        // Record time before update operation
        auto beforeTime = TimestampManager::getCurrentTimestamp();
        
        // Simulate update operation with timestamp integration
        auto updatedAtFields = MockORMMetadata::getUpdatedAtFields();
        TimestampManager::applyUpdatedAt(obj, updatedAtFields);
        
        // Record time after update operation
        auto afterTime = TimestampManager::getCurrentTimestamp();
        
        // Verify updated_at field was populated
        EXPECT_NE(obj.updated_at.type, SqlDate::kErrorTime) 
            << "Updated at timestamp should be populated during update";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.updated_at))
            << "Updated at timestamp should be recent";
        EXPECT_GE(obj.updated_at.toTimestamp(), beforeTime.toTimestamp())
            << "Updated at should be after or equal to before time";
        EXPECT_LE(obj.updated_at.toTimestamp(), afterTime.toTimestamp())
            << "Updated at should be before or equal to after time";
        
        // Verify combined_timestamp field was also populated (has updated_at behavior)
        EXPECT_NE(obj.combined_timestamp.type, SqlDate::kErrorTime)
            << "Combined timestamp should be populated during update";
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.combined_timestamp))
            << "Combined timestamp should be recent";
        
        // Verify created_at field was NOT affected by update operation
        EXPECT_EQ(obj.created_at.toTimestamp(), originalCreatedAt.toTimestamp())
            << "Created at should not be modified during update operation";
        
        // Test multiple updates - each should get a new timestamp
        auto firstUpdateTime = obj.updated_at;
        
        // Small delay to ensure different timestamp
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Clear timestamp to simulate automatic management (not manual)
        obj.updated_at.clear();
        obj.combined_timestamp.clear();
        
        TimestampManager::applyUpdatedAt(obj, updatedAtFields);
        
        EXPECT_GT(obj.updated_at.toTimestamp(), firstUpdateTime.toTimestamp())
            << "Second update should have later timestamp";
        
        // Test with object that has manual updated_at value (should be preserved)
        MockRuntimeObject manualObj = propertyTester.generateRandomObject();
        auto manualUpdatedAt = propertyTester.generateRandomTimestamp();
        manualObj.updated_at = manualUpdatedAt;
        
        TimestampManager::applyUpdatedAt(manualObj, updatedAtFields);
        
        // Manual value should be preserved
        EXPECT_EQ(manualObj.updated_at.toTimestamp(), manualUpdatedAt.toTimestamp())
            << "Manual updated_at value should be preserved during update";
        
    }, 100);
}

// **Property 30: Bulk operation timestamp consistency**
// *For any* bulk operation on objects with timestamp fields, timestamp logic should be applied consistently across all affected records
// **Validates: Requirements 7.4**
TEST_F(RuntimeBehaviorIntegrationPropertyTestFixture, Property30_BulkOperationTimestampConsistency) {
    // Feature: sql-tags-enhancement, Property 30: Bulk operation timestamp consistency
    propertyTester.runPropertyTest([&]() {
        // Generate multiple objects for bulk operation
        int bulkCount = propertyTester.generateBulkCount();
        std::vector<MockRuntimeObject> objects;
        
        for (int i = 0; i < bulkCount; ++i) {
            MockRuntimeObject obj = propertyTester.generateRandomObject();
            obj.created_at.clear(); // Ensure consistent starting state
            obj.updated_at.clear();
            obj.combined_timestamp.clear();
            objects.push_back(obj);
        }
        
        // Record time before bulk insert operation
        auto beforeInsertTime = TimestampManager::getCurrentTimestamp();
        
        // Apply created_at timestamps to all objects (simulating bulk insert)
        auto createdAtFields = MockORMMetadata::getCreatedAtFields();
        for (auto& obj : objects) {
            TimestampManager::applyCreatedAt(obj, createdAtFields);
        }
        
        // Record time after bulk insert operation
        auto afterInsertTime = TimestampManager::getCurrentTimestamp();
        
        // Verify all objects received created_at timestamps
        for (size_t i = 0; i < objects.size(); ++i) {
            EXPECT_NE(objects[i].created_at.type, SqlDate::kErrorTime)
                << "Object " << i << " should have created_at timestamp";
            EXPECT_GE(objects[i].created_at.toTimestamp(), beforeInsertTime.toTimestamp())
                << "Object " << i << " created_at should be after before time";
            EXPECT_LE(objects[i].created_at.toTimestamp(), afterInsertTime.toTimestamp())
                << "Object " << i << " created_at should be before after time";
            
            // Combined timestamp should also be set (has created_at behavior)
            EXPECT_NE(objects[i].combined_timestamp.type, SqlDate::kErrorTime)
                << "Object " << i << " should have combined timestamp";
        }
        
        // Verify timestamp consistency - all timestamps should be very close
        if (objects.size() > 1) {
            auto referenceTimestamp = objects[0].created_at;
            for (size_t i = 1; i < objects.size(); ++i) {
                EXPECT_TRUE(propertyTester.timestampsAreClose(
                    objects[i].created_at, referenceTimestamp, 2))
                    << "Object " << i << " created_at should be close to reference timestamp";
            }
        }
        
        // Small delay before bulk update
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Clear updated_at fields for bulk update test
        for (auto& obj : objects) {
            obj.updated_at.clear();
            obj.combined_timestamp.clear(); // Will be updated again
        }
        
        // Record time before bulk update operation
        auto beforeUpdateTime = TimestampManager::getCurrentTimestamp();
        
        // Apply updated_at timestamps to all objects (simulating bulk update)
        auto updatedAtFields = MockORMMetadata::getUpdatedAtFields();
        for (auto& obj : objects) {
            TimestampManager::applyUpdatedAt(obj, updatedAtFields);
        }
        
        // Record time after bulk update operation
        auto afterUpdateTime = TimestampManager::getCurrentTimestamp();
        
        // Verify all objects received updated_at timestamps
        for (size_t i = 0; i < objects.size(); ++i) {
            EXPECT_NE(objects[i].updated_at.type, SqlDate::kErrorTime)
                << "Object " << i << " should have updated_at timestamp";
            EXPECT_GE(objects[i].updated_at.toTimestamp(), beforeUpdateTime.toTimestamp())
                << "Object " << i << " updated_at should be after before time";
            EXPECT_LE(objects[i].updated_at.toTimestamp(), afterUpdateTime.toTimestamp())
                << "Object " << i << " updated_at should be before after time";
            
            // Updated_at should be later than created_at
            EXPECT_GT(objects[i].updated_at.toTimestamp(), objects[i].created_at.toTimestamp())
                << "Object " << i << " updated_at should be later than created_at";
            
            // Combined timestamp should also be updated (has updated_at behavior)
            EXPECT_NE(objects[i].combined_timestamp.type, SqlDate::kErrorTime)
                << "Object " << i << " should have updated combined timestamp";
            EXPECT_GT(objects[i].combined_timestamp.toTimestamp(), objects[i].created_at.toTimestamp())
                << "Object " << i << " combined timestamp should be updated";
        }
        
        // Verify update timestamp consistency - all update timestamps should be very close
        if (objects.size() > 1) {
            auto referenceUpdateTimestamp = objects[0].updated_at;
            for (size_t i = 1; i < objects.size(); ++i) {
                EXPECT_TRUE(propertyTester.timestampsAreClose(
                    objects[i].updated_at, referenceUpdateTimestamp, 2))
                    << "Object " << i << " updated_at should be close to reference timestamp";
            }
        }
        
        // Test mixed manual/automatic timestamps in bulk operation
        std::vector<MockRuntimeObject> mixedObjects;
        for (int i = 0; i < bulkCount; ++i) {
            MockRuntimeObject obj = propertyTester.generateRandomObject();
            
            // Randomly set some objects to have manual timestamps
            if (propertyTester.generateBulkCount() % 2 == 0) {
                obj.created_at = propertyTester.generateRandomTimestamp();
            } else {
                obj.created_at.clear();
            }
            
            mixedObjects.push_back(obj);
        }
        
        // Apply timestamps to mixed objects
        for (auto& obj : mixedObjects) {
            bool hadManualTimestamp = TimestampManager::shouldPreserveManualValue(obj.created_at);
            auto originalTimestamp = obj.created_at;
            
            TimestampManager::applyCreatedAt(obj, createdAtFields);
            
            if (hadManualTimestamp) {
                EXPECT_EQ(obj.created_at.toTimestamp(), originalTimestamp.toTimestamp())
                    << "Manual timestamp should be preserved in bulk operation";
            } else {
                EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.created_at))
                    << "Empty timestamp should be set in bulk operation";
            }
        }
        
    }, 100);
}

// Additional test for builder integration
TEST_F(RuntimeBehaviorIntegrationPropertyTestFixture, BuilderIntegrationTest) {
    propertyTester.runPropertyTest([&]() {
        // Test that builders properly integrate timestamp logic
        MockRuntimeObject obj = propertyTester.generateRandomObject();
        obj.created_at.clear();
        obj.updated_at.clear();
        
        // Test insert builder integration (simulation - no actual database)
        MockTimestampAwareInsertBuilder<MockRuntimeObject> insertBuilder("test_table", {"id", "name", "created_at", "updated_at"});
        insertBuilder.setObject(obj);
        
        auto result = insertBuilder.execute();
        EXPECT_EQ(result, 1) << "Insert builder should return success";
        
        // The execute() method should have applied timestamps to the object
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.created_at))
            << "Insert builder should apply created_at timestamps";
        
        // Test update builder integration (simulation - no actual database)
        obj.updated_at.clear();
        MockTimestampAwareUpdateBuilder updateBuilder("test_table");
        updateBuilder.setObject(obj);
        
        result = updateBuilder.execute();
        EXPECT_EQ(result, 1) << "Update builder should return success";
        
        // The execute() method should have applied timestamps to the object
        EXPECT_TRUE(propertyTester.isRecentTimestamp(obj.updated_at))
            << "Update builder should apply updated_at timestamps";
        
    }, 100);
}

// Test field detection from SqlTags
TEST_F(RuntimeBehaviorIntegrationPropertyTestFixture, FieldDetectionTest) {
    // Test that field detection works correctly
    auto createdAtFields = MockORMMetadata::getCreatedAtFields();
    auto updatedAtFields = MockORMMetadata::getUpdatedAtFields();
    
    // Should include fields with created_at behavior
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "created_at") != createdAtFields.end())
        << "Should detect created_at field";
    EXPECT_TRUE(std::find(createdAtFields.begin(), createdAtFields.end(), "combined_timestamp") != createdAtFields.end())
        << "Should detect combined_timestamp field for created_at";
    
    // Should include fields with updated_at behavior
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "updated_at") != updatedAtFields.end())
        << "Should detect updated_at field";
    EXPECT_TRUE(std::find(updatedAtFields.begin(), updatedAtFields.end(), "combined_timestamp") != updatedAtFields.end())
        << "Should detect combined_timestamp field for updated_at";
}

// Main function for the test
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}