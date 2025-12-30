/**
 * @file basic-constraints.cpp
 * @brief Demonstrates basic SqlTags constraint usage
 * 
 * This example shows how to use the fundamental SqlTags constraints:
 * - primary_key: Marks a field as the primary key
 * - not_null: Prevents NULL values
 * - unique: Ensures unique values across the table
 * - auto_increment: Automatically generates sequential values
 * - index: Creates an index for faster queries
 */

#include <ilias/platform.hpp>
#include <ilias/sql/sqldatabase.hpp>
#include <ilias/sql_orm/detail/orm_types.hpp>
#include <ilias/sql_orm/detail/schema_generator.hpp>
#include <iostream>

using namespace ilias;
using namespace ilias::sql;

// Example 1: Simple User model with basic constraints
struct SimpleUser {
    int64_t id;           // Primary key with auto increment
    std::string username; // Unique, not null, indexed
    std::string email;    // Unique, not null
    std::string name;     // Not null only
    std::optional<std::string> bio; // Nullable field
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<SimpleUser, void> {
    constexpr static auto value = Object(
        // Primary key with auto increment - most common pattern for ID fields
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&SimpleUser::id),
        
        // Username: unique, not null, and indexed for fast lookups
        "username", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,  // Redundant but explicit (unique implies index)
            .length = 50    // Reasonable length for usernames
        }>(&SimpleUser::username),
        
        // Email: unique and not null (common pattern)
        "email", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .length = 255   // Standard email length
        }>(&SimpleUser::email),
        
        // Display name: not null but not unique
        "name", make_tags<SqlTags{
            .not_null = true,
            .length = 100
        }>(&SimpleUser::name),
        
        // Bio: nullable field, no constraints
        "bio", &SimpleUser::bio  // No SqlTags = default behavior (nullable)
    );
};
NEKO_END_NAMESPACE

// Example 2: Product model demonstrating different constraint patterns
struct Product {
    int64_t id;
    std::string sku;        // Stock Keeping Unit - unique identifier
    std::string name;
    std::string category;
    double price;
    int stock_quantity;
    bool is_active;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<Product, void> {
    constexpr static auto value = Object(
        // Standard auto-incrementing primary key
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&Product::id),
        
        // SKU: unique business identifier, indexed for lookups
        "sku", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,
            .length = 50
        }>(&Product::sku),
        
        // Product name: not null, indexed for search
        "name", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 200
        }>(&Product::name),
        
        // Category: indexed for filtering, not unique
        "category", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 100
        }>(&Product::category),
        
        // Price: not null, no special constraints
        "price", make_tags<SqlTags{
            .not_null = true
        }>(&Product::price),
        
        // Stock quantity: not null, no special constraints
        "stock_quantity", make_tags<SqlTags{
            .not_null = true
        }>(&Product::stock_quantity),
        
        // Active flag: not null boolean
        "is_active", make_tags<SqlTags{
            .not_null = true
        }>(&Product::is_active)
    );
};
NEKO_END_NAMESPACE

// Example 3: Configuration model showing index-only fields
struct Configuration {
    int64_t id;
    std::string key;      // Unique configuration key
    std::string value;    // Configuration value
    std::string category; // Indexed for grouping
    std::string description; // No constraints
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<Configuration, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&Configuration::id),
        
        // Configuration key: unique identifier
        "key", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .length = 100
        }>(&Configuration::key),
        
        // Value: not null but not unique (multiple configs can have same value)
        "value", make_tags<SqlTags{
            .not_null = true
        }>(&Configuration::value),  // Uses TEXT (no length specified)
        
        // Category: indexed for filtering configurations by category
        "category", make_tags<SqlTags{
            .not_null = true,
            .index = true,  // Index without unique - allows multiple configs per category
            .length = 50
        }>(&Configuration::category),
        
        // Description: no constraints, nullable
        "description", &Configuration::description
    );
};
NEKO_END_NAMESPACE

// Demonstration function
IoTask<void> demonstrate_basic_constraints() {
    std::cout << "=== Basic SqlTags Constraints Demo ===" << std::endl;
    
    // Show generated SQL for different databases
    std::cout << "\n1. SimpleUser table schemas:" << std::endl;
    
    auto sqliteSchema = SchemaGenerator<SqliteTag>::generateCreateTable<SimpleUser>("users");
    std::cout << "SQLite:\n" << sqliteSchema << std::endl;
    
    auto mysqlSchema = SchemaGenerator<MysqlTag>::generateCreateTable<SimpleUser>("users");
    std::cout << "\nMySQL:\n" << mysqlSchema << std::endl;
    
    auto postgresSchema = SchemaGenerator<PostgresTag>::generateCreateTable<SimpleUser>("users");
    std::cout << "\nPostgreSQL:\n" << postgresSchema << std::endl;
    
    // Show index generation
    std::cout << "\n2. Index statements for Product table:" << std::endl;
    
    std::vector<std::pair<std::string, SqlTags>> productColumns = {
        {"name", SqlTags{.not_null = true, .index = true, .length = 200}},
        {"category", SqlTags{.not_null = true, .index = true, .length = 100}},
        {"sku", SqlTags{.not_null = true, .unique = true, .length = 50}}  // Won't generate index (unique creates its own)
    };
    
    auto sqliteIndexes = SchemaGenerator<SqliteTag>::generateIndexStatements("products", productColumns);
    std::cout << "SQLite indexes:" << std::endl;
    for (const auto& index : sqliteIndexes) {
        std::cout << "  " << index << std::endl;
    }
    
    // Demonstrate validation
    std::cout << "\n3. Constraint validation examples:" << std::endl;
    
    // Valid configuration
    SqlTags validTags{.primary_key = true, .auto_increment = true};
    std::cout << "Valid tags: " << (validTags.isValid() ? "✓" : "✗") << std::endl;
    
    // Invalid configuration - conflicting constraints
    SqlTags invalidTags{.primary_key = true, .unique = false};  // primary_key implies unique
    std::cout << "Invalid tags: " << (invalidTags.isValid() ? "✓" : "✗") << std::endl;
    if (!invalidTags.isValid()) {
        auto errors = invalidTags.getValidationErrors();
        for (const auto& error : errors) {
            std::cout << "  Error: " << error << std::endl;
        }
    }
    
    // Show helper methods
    std::cout << "\n4. Helper method examples:" << std::endl;
    
    SqlTags indexedTags{.unique = true};
    std::cout << "Unique field requires index: " << (indexedTags.requiresIndex() ? "Yes" : "No") << std::endl;
    
    SqlTags plainTags{.not_null = true};
    std::cout << "Plain not_null requires index: " << (plainTags.requiresIndex() ? "Yes" : "No") << std::endl;
}

// Example usage with actual database operations
IoTask<void> basic_database_operations() {
    // This would be used with actual database connection
    // auto db = co_await SqlDatabase::open("sqlite", options);
    
    std::cout << "\n=== Database Operations Example ===" << std::endl;
    std::cout << "This example shows how the constraints work in practice:\n" << std::endl;
    
    // Example 1: Creating a user (auto_increment in action)
    SimpleUser newUser;
    newUser.id = 0;  // Will be auto-generated
    newUser.username = "john_doe";
    newUser.email = "john@example.com";
    newUser.name = "John Doe";
    newUser.bio = "Software developer";
    
    std::cout << "1. Creating user with auto_increment ID:" << std::endl;
    std::cout << "   Input ID: " << newUser.id << " (will be auto-generated)" << std::endl;
    std::cout << "   Username: " << newUser.username << " (must be unique)" << std::endl;
    std::cout << "   Email: " << newUser.email << " (must be unique)" << std::endl;
    
    // In real usage:
    // auto stmt = co_await db.prepare<SimpleUser>("INSERT INTO users VALUES (:id, :username, :email, :name, :bio)");
    // stmt.bind(newUser);
    // auto result = co_await stmt.execute();
    // newUser.id would now contain the auto-generated ID
    
    // Example 2: Constraint violations (would fail in real database)
    std::cout << "\n2. Constraint violation examples (would fail in database):" << std::endl;
    
    SimpleUser duplicateUser;
    duplicateUser.username = "john_doe";  // Duplicate username - would violate unique constraint
    duplicateUser.email = "different@example.com";
    duplicateUser.name = "Another John";
    std::cout << "   Duplicate username '" << duplicateUser.username << "' would violate unique constraint" << std::endl;
    
    SimpleUser nullUser;
    nullUser.username = "valid_user";
    nullUser.email = "valid@example.com";
    // nullUser.name is empty - would violate not_null constraint
    std::cout << "   Empty name would violate not_null constraint" << std::endl;
    
    // Example 3: Using indexes for fast queries
    std::cout << "\n3. Index usage for fast queries:" << std::endl;
    std::cout << "   SELECT * FROM users WHERE username = 'john_doe'  -- Uses unique index" << std::endl;
    std::cout << "   SELECT * FROM products WHERE category = 'electronics'  -- Uses category index" << std::endl;
    std::cout << "   SELECT * FROM products WHERE name LIKE '%laptop%'  -- Uses name index" << std::endl;
}

// Main function to run all examples
IoTask<void> run_basic_constraints_examples() {
    try {
        co_await demonstrate_basic_constraints();
        co_await basic_database_operations();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "Basic constraints provide:" << std::endl;
        std::cout << "• primary_key: Unique identifier with automatic indexing" << std::endl;
        std::cout << "• not_null: Prevents NULL values, ensures data integrity" << std::endl;
        std::cout << "• unique: Ensures uniqueness, automatically creates index" << std::endl;
        std::cout << "• auto_increment: Automatic ID generation for primary keys" << std::endl;
        std::cout << "• index: Fast lookups for frequently queried fields" << std::endl;
        std::cout << "\nThese constraints are automatically translated to appropriate SQL" << std::endl;
        std::cout << "for different database systems (SQLite, MySQL, PostgreSQL)." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Uncomment to run as standalone example
// int main() {
//     return ilias::run(run_basic_constraints_examples());
// }