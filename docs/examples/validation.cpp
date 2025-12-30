/**
 * @file validation.cpp
 * @brief Demonstrates SqlTags validation and error handling
 * 
 * This example shows how to:
 * - Validate SqlTags configurations
 * - Handle validation errors
 * - Implement custom validation logic
 * - Test configurations before deployment
 * - Debug common configuration issues
 */

#include <ilias/platform.hpp>
#include <ilias/sql_orm/detail/orm_types.hpp>
#include <iostream>
#include <cassert>

using namespace ilias;
using namespace ilias::sql;

// Example 1: Valid configuration
struct ValidUser {
    int64_t id;
    std::string username;
    std::string email;
    SqlDate created_at;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<ValidUser, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&ValidUser::id),
        
        "username", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .length = 50
        }>(&ValidUser::username),
        
        "email", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .length = 255
        }>(&ValidUser::email),
        
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true
        }>(&ValidUser::created_at)
    );
};
NEKO_END_NAMESPACE

// Example 2: Configuration with validation errors
struct InvalidUser {
    int64_t id;
    std::string username;
    std::string description;
    int32_t count;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<InvalidUser, void> {
    constexpr static auto value = Object(
        // Error: Conflicting constraints (primary_key implies unique)
        "id", make_tags<SqlTags{
            .primary_key = true,
            .unique = false  // This conflicts with primary_key
        }>(&InvalidUser::id),
        
        // Error: Negative length
        "username", make_tags<SqlTags{
            .not_null = true,
            .length = -10  // Invalid negative length
        }>(&InvalidUser::username),
        
        // Error: Auto increment on string field (would be caught at compile time)
        // This example shows what the error would be if it were possible
        "description", make_tags<SqlTags{
            .not_null = true
            // .auto_increment = true  // Would be invalid for string type
        }>(&InvalidUser::description),
        
        // Valid field for comparison
        "count", make_tags<SqlTags{
            .not_null = true,
            .unsigned_type = true
        }>(&InvalidUser::count)
    );
};
NEKO_END_NAMESPACE

// Validation testing framework
class ValidationTester {
public:
    static void testValidConfiguration() {
        std::cout << "=== Testing Valid Configuration ===" << std::endl;
        
        // Test individual valid tags
        SqlTags validTags{
            .primary_key = true,
            .auto_increment = true
        };
        
        std::cout << "Valid tags configuration:" << std::endl;
        std::cout << "  primary_key: " << validTags.primary_key << std::endl;
        std::cout << "  auto_increment: " << validTags.auto_increment << std::endl;
        std::cout << "  Is valid: " << (validTags.isValid() ? "✓" : "✗") << std::endl;
        
        if (validTags.isValid()) {
            std::cout << "  No validation errors found." << std::endl;
        } else {
            auto errors = validTags.getValidationErrors();
            std::cout << "  Unexpected errors:" << std::endl;
            for (const auto& error : errors) {
                std::cout << "    - " << error << std::endl;
            }
        }
    }
    
    static void testInvalidConfigurations() {
        std::cout << "\n=== Testing Invalid Configurations ===" << std::endl;
        
        // Test 1: Conflicting constraints
        std::cout << "\n1. Testing conflicting constraints:" << std::endl;
        SqlTags conflictingTags{
            .primary_key = true,
            .unique = false  // Conflicts with primary_key
        };
        
        std::cout << "Configuration: primary_key=true, unique=false" << std::endl;
        std::cout << "Is valid: " << (conflictingTags.isValid() ? "✓" : "✗") << std::endl;
        
        if (!conflictingTags.isValid()) {
            auto errors = conflictingTags.getValidationErrors();
            std::cout << "Validation errors:" << std::endl;
            for (const auto& error : errors) {
                std::cout << "  - " << error << std::endl;
            }
        }
        
        // Test 2: Invalid length
        std::cout << "\n2. Testing invalid length:" << std::endl;
        SqlTags invalidLengthTags{
            .not_null = true,
            .length = -5  // Invalid negative length
        };
        
        std::cout << "Configuration: length=-5" << std::endl;
        std::cout << "Is valid: " << (invalidLengthTags.isValid() ? "✓" : "✗") << std::endl;
        
        if (!invalidLengthTags.isValid()) {
            auto errors = invalidLengthTags.getValidationErrors();
            std::cout << "Validation errors:" << std::endl;
            for (const auto& error : errors) {
                std::cout << "  - " << error << std::endl;
            }
        }
        
        // Test 3: Multiple errors
        std::cout << "\n3. Testing multiple validation errors:" << std::endl;
        SqlTags multipleErrorTags{
            .primary_key = true,
            .unique = false,  // Error 1: conflicts with primary_key
            .length = -10     // Error 2: negative length
        };
        
        std::cout << "Configuration: primary_key=true, unique=false, length=-10" << std::endl;
        std::cout << "Is valid: " << (multipleErrorTags.isValid() ? "✓" : "✗") << std::endl;
        
        if (!multipleErrorTags.isValid()) {
            auto errors = multipleErrorTags.getValidationErrors();
            std::cout << "Validation errors (" << errors.size() << " found):" << std::endl;
            for (size_t i = 0; i < errors.size(); ++i) {
                std::cout << "  " << (i + 1) << ". " << errors[i] << std::endl;
            }
        }
    }
    
    static void testHelperMethods() {
        std::cout << "\n=== Testing Helper Methods ===" << std::endl;
        
        // Test hasTimestampBehavior
        std::cout << "\n1. Testing hasTimestampBehavior():" << std::endl;
        
        SqlTags noTimestamp{.not_null = true};
        SqlTags createdAt{.created_at = true};
        SqlTags updatedAt{.updated_at = true};
        SqlTags bothTimestamps{.created_at = true, .updated_at = true};
        
        std::cout << "  No timestamp flags: " << (noTimestamp.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
        std::cout << "  created_at only: " << (createdAt.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
        std::cout << "  updated_at only: " << (updatedAt.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
        std::cout << "  Both timestamps: " << (bothTimestamps.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
        
        // Test requiresIndex
        std::cout << "\n2. Testing requiresIndex():" << std::endl;
        
        SqlTags noIndex{.not_null = true};
        SqlTags primaryKey{.primary_key = true};
        SqlTags unique{.unique = true};
        SqlTags explicitIndex{.index = true};
        
        std::cout << "  No index flags: " << (noIndex.requiresIndex() ? "Yes" : "No") << std::endl;
        std::cout << "  Primary key: " << (primaryKey.requiresIndex() ? "Yes" : "No") << std::endl;
        std::cout << "  Unique constraint: " << (unique.requiresIndex() ? "Yes" : "No") << std::endl;
        std::cout << "  Explicit index: " << (explicitIndex.requiresIndex() ? "Yes" : "No") << std::endl;
    }
    
    static void testEdgeCases() {
        std::cout << "\n=== Testing Edge Cases ===" << std::endl;
        
        // Test 1: Empty configuration
        std::cout << "\n1. Testing empty configuration:" << std::endl;
        SqlTags emptyTags{};
        std::cout << "Empty SqlTags is valid: " << (emptyTags.isValid() ? "✓" : "✗") << std::endl;
        
        // Test 2: Maximum valid length
        std::cout << "\n2. Testing maximum valid length:" << std::endl;
        SqlTags maxLengthTags{.length = 65535};  // MySQL VARCHAR max
        std::cout << "Max length (65535) is valid: " << (maxLengthTags.isValid() ? "✓" : "✗") << std::endl;
        
        // Test 3: Zero length (should be valid)
        std::cout << "\n3. Testing zero length:" << std::endl;
        SqlTags zeroLengthTags{.length = 0};
        std::cout << "Zero length is valid: " << (zeroLengthTags.isValid() ? "✓" : "✗") << std::endl;
        
        // Test 4: All constraints enabled (should be valid)
        std::cout << "\n4. Testing all compatible constraints:" << std::endl;
        SqlTags allConstraints{
            .primary_key = true,
            .not_null = true,     // Implied by primary_key, but explicit is OK
            .unique = true,       // Implied by primary_key, but explicit is OK
            .auto_increment = true,
            .index = true,        // Redundant with primary_key, but OK
            .length = 100,
            .created_at = true
        };
        std::cout << "All constraints enabled is valid: " << (allConstraints.isValid() ? "✓" : "✗") << std::endl;
        if (!allConstraints.isValid()) {
            auto errors = allConstraints.getValidationErrors();
            for (const auto& error : errors) {
                std::cout << "  Error: " << error << std::endl;
            }
        }
    }
};

// Custom validation framework for application-specific rules
class CustomValidator {
public:
    // Example: Validate that email fields have appropriate length
    static bool validateEmailField(const SqlTags& tags) {
        // If it's marked as an email field (by convention), ensure proper length
        if (tags.length > 0 && tags.length < 100) {
            std::cout << "Warning: Email field length (" << tags.length 
                      << ") might be too short for some email addresses" << std::endl;
            return false;
        }
        return true;
    }
    
    // Example: Validate that indexed string fields have reasonable length
    static bool validateIndexedStringField(const SqlTags& tags) {
        if (tags.requiresIndex() && tags.length == 0) {
            std::cout << "Warning: Indexed string field without explicit length will use default (255)" << std::endl;
        }
        if (tags.requiresIndex() && tags.length > 1000) {
            std::cout << "Warning: Very long indexed field (" << tags.length 
                      << ") may impact performance" << std::endl;
            return false;
        }
        return true;
    }
    
    // Example: Validate timestamp field combinations
    static bool validateTimestampFields(const SqlTags& tags) {
        if (tags.created_at && tags.updated_at) {
            std::cout << "Info: Field has both created_at and updated_at behavior (will act as updated_at)" << std::endl;
        }
        return true;
    }
};

// Comprehensive validation test suite
IoTask<void> runValidationTestSuite() {
    std::cout << "=== SqlTags Validation Test Suite ===" << std::endl;
    
    ValidationTester::testValidConfiguration();
    ValidationTester::testInvalidConfigurations();
    ValidationTester::testHelperMethods();
    ValidationTester::testEdgeCases();
    
    std::cout << "\n=== Custom Validation Examples ===" << std::endl;
    
    // Test custom validation rules
    std::cout << "\n1. Testing email field validation:" << std::endl;
    SqlTags shortEmailTags{.unique = true, .length = 50};
    CustomValidator::validateEmailField(shortEmailTags);
    
    SqlTags properEmailTags{.unique = true, .length = 255};
    CustomValidator::validateEmailField(properEmailTags);
    
    std::cout << "\n2. Testing indexed field validation:" << std::endl;
    SqlTags indexedNoLength{.index = true, .length = 0};
    CustomValidator::validateIndexedStringField(indexedNoLength);
    
    SqlTags indexedLongField{.index = true, .length = 2000};
    CustomValidator::validateIndexedStringField(indexedLongField);
    
    std::cout << "\n3. Testing timestamp field validation:" << std::endl;
    SqlTags combinedTimestamp{.created_at = true, .updated_at = true};
    CustomValidator::validateTimestampFields(combinedTimestamp);
}

// Practical validation examples for real-world usage
IoTask<void> demonstratePracticalValidation() {
    std::cout << "\n=== Practical Validation Examples ===" << std::endl;
    
    // Example 1: Validate table configuration during development
    std::cout << "\n1. Development-time validation:" << std::endl;
    
    auto validateField = [](const std::string& fieldName, const SqlTags& tags) {
        std::cout << "Validating field '" << fieldName << "':" << std::endl;
        
        if (!tags.isValid()) {
            std::cout << "  ❌ INVALID CONFIGURATION" << std::endl;
            auto errors = tags.getValidationErrors();
            for (const auto& error : errors) {
                std::cout << "     Error: " << error << std::endl;
            }
            return false;
        } else {
            std::cout << "  ✅ Valid configuration" << std::endl;
            
            // Additional checks
            if (tags.hasTimestampBehavior()) {
                std::cout << "     Info: Has automatic timestamp behavior" << std::endl;
            }
            if (tags.requiresIndex()) {
                std::cout << "     Info: Will create database index" << std::endl;
            }
            return true;
        }
    };
    
    // Validate some example fields
    validateField("id", SqlTags{.primary_key = true, .auto_increment = true});
    validateField("email", SqlTags{.not_null = true, .unique = true, .length = 255});
    validateField("invalid_field", SqlTags{.primary_key = true, .unique = false});
    
    // Example 2: Runtime validation checks
    std::cout << "\n2. Runtime validation checks:" << std::endl;
    
    auto performRuntimeCheck = [](const std::string& tableName) {
        std::cout << "Performing runtime checks for table '" << tableName << "':" << std::endl;
        
        // In a real application, you would iterate through all fields
        // and validate their SqlTags configurations
        
        std::vector<std::pair<std::string, SqlTags>> fields = {
            {"id", SqlTags{.primary_key = true, .auto_increment = true}},
            {"username", SqlTags{.not_null = true, .unique = true, .length = 50}},
            {"created_at", SqlTags{.not_null = true, .created_at = true}}
        };
        
        bool allValid = true;
        for (const auto& [fieldName, tags] : fields) {
            if (!tags.isValid()) {
                std::cout << "  ❌ Field '" << fieldName << "' has invalid configuration" << std::endl;
                allValid = false;
            }
        }
        
        if (allValid) {
            std::cout << "  ✅ All fields have valid configurations" << std::endl;
        }
        
        return allValid;
    };
    
    performRuntimeCheck("users");
    
    // Example 3: CI/CD validation
    std::cout << "\n3. CI/CD validation example:" << std::endl;
    std::cout << "In a CI/CD pipeline, you could:" << std::endl;
    std::cout << "  • Validate all SqlTags configurations during build" << std::endl;
    std::cout << "  • Generate schema and validate SQL syntax" << std::endl;
    std::cout << "  • Check for performance anti-patterns" << std::endl;
    std::cout << "  • Ensure consistent naming conventions" << std::endl;
    std::cout << "  • Verify cross-database compatibility" << std::endl;
}

// Error handling patterns
IoTask<void> demonstrateErrorHandling() {
    std::cout << "\n=== Error Handling Patterns ===" << std::endl;
    
    // Pattern 1: Assertion-based validation (development)
    std::cout << "\n1. Assertion-based validation (development):" << std::endl;
    
    auto assertValidTags = [](const SqlTags& tags, const std::string& context) {
        if (!tags.isValid()) {
            auto errors = tags.getValidationErrors();
            std::cout << "ASSERTION FAILED in " << context << ":" << std::endl;
            for (const auto& error : errors) {
                std::cout << "  " << error << std::endl;
            }
            // In real code: assert(tags.isValid());
            std::cout << "  (Would assert here in debug build)" << std::endl;
        } else {
            std::cout << "Assertion passed for " << context << std::endl;
        }
    };
    
    assertValidTags(SqlTags{.primary_key = true}, "user.id field");
    assertValidTags(SqlTags{.length = -1}, "invalid field");
    
    // Pattern 2: Exception-based validation (production)
    std::cout << "\n2. Exception-based validation (production):" << std::endl;
    
    auto validateOrThrow = [](const SqlTags& tags, const std::string& fieldName) {
        if (!tags.isValid()) {
            auto errors = tags.getValidationErrors();
            std::string errorMsg = "Invalid SqlTags configuration for field '" + fieldName + "': ";
            for (size_t i = 0; i < errors.size(); ++i) {
                if (i > 0) errorMsg += ", ";
                errorMsg += errors[i];
            }
            std::cout << "Would throw exception: " << errorMsg << std::endl;
            // In real code: throw std::invalid_argument(errorMsg);
        } else {
            std::cout << "Field '" << fieldName << "' validation passed" << std::endl;
        }
    };
    
    validateOrThrow(SqlTags{.not_null = true, .length = 100}, "username");
    validateOrThrow(SqlTags{.primary_key = true, .unique = false}, "id");
    
    // Pattern 3: Result-based validation (functional style)
    std::cout << "\n3. Result-based validation (functional style):" << std::endl;
    
    struct ValidationResult {
        bool isValid;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    auto validateWithResult = [](const SqlTags& tags) -> ValidationResult {
        ValidationResult result;
        result.isValid = tags.isValid();
        result.errors = tags.getValidationErrors();
        
        // Add custom warnings
        if (tags.requiresIndex() && tags.length > 500) {
            result.warnings.push_back("Long indexed field may impact performance");
        }
        if (tags.hasTimestampBehavior() && !tags.not_null) {
            result.warnings.push_back("Timestamp field should typically be NOT NULL");
        }
        
        return result;
    };
    
    auto result1 = validateWithResult(SqlTags{.unique = true, .length = 1000});
    std::cout << "Validation result: " << (result1.isValid ? "Valid" : "Invalid") << std::endl;
    for (const auto& warning : result1.warnings) {
        std::cout << "  Warning: " << warning << std::endl;
    }
    
    auto result2 = validateWithResult(SqlTags{.created_at = true});
    std::cout << "Validation result: " << (result2.isValid ? "Valid" : "Invalid") << std::endl;
    for (const auto& warning : result2.warnings) {
        std::cout << "  Warning: " << warning << std::endl;
    }
}

// Main function to run all validation examples
IoTask<void> run_validation_examples() {
    try {
        co_await runValidationTestSuite();
        co_await demonstratePracticalValidation();
        co_await demonstrateErrorHandling();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "SqlTags validation provides:" << std::endl;
        std::cout << "• Compile-time and runtime configuration validation" << std::endl;
        std::cout << "• Detailed error messages for debugging" << std::endl;
        std::cout << "• Helper methods for common validation checks" << std::endl;
        std::cout << "• Flexible error handling patterns for different use cases" << std::endl;
        std::cout << "• Custom validation rules for application-specific requirements" << std::endl;
        std::cout << "\nProper validation ensures database schemas are correct and" << std::endl;
        std::cout << "optimized before deployment, preventing runtime errors and" << std::endl;
        std::cout << "performance issues." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Uncomment to run as standalone example
// int main() {
//     return ilias::run(run_validation_examples());
// }