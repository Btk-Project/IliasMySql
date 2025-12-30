/**
 * @file timestamps.cpp
 * @brief Demonstrates automatic timestamp management with SqlTags
 * 
 * This example shows how to use SqlTags for automatic timestamp handling:
 * - created_at: Automatically set when records are inserted
 * - updated_at: Automatically updated when records are modified
 * - Combined behavior: Fields that track both creation and updates
 * - Manual override: How to override automatic behavior when needed
 */

#include <ilias/platform.hpp>
#include <ilias/sql/sqldatabase.hpp>
#include <ilias/sql_orm/detail/orm_types.hpp>
#include <ilias/sql_orm/detail/timestamp_manager.hpp>
#include <ilias/sql_orm/detail/schema_generator.hpp>
#include <iostream>
#include <chrono>

using namespace ilias;
using namespace ilias::sql;

// Example 1: Standard audit pattern with separate created_at and updated_at
struct AuditableUser {
    int64_t id;
    std::string username;
    std::string email;
    SqlDate created_at;   // Set once when record is created
    SqlDate updated_at;   // Updated every time record is modified
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<AuditableUser, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&AuditableUser::id),
        
        "username", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .length = 50
        }>(&AuditableUser::username),
        
        "email", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .length = 255
        }>(&AuditableUser::email),
        
        // Created timestamp - set once during INSERT
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true
        }>(&AuditableUser::created_at),
        
        // Updated timestamp - updated during INSERT and UPDATE
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&AuditableUser::updated_at)
    );
};
NEKO_END_NAMESPACE

// Example 2: Simple pattern with single last_modified field
struct SimpleDocument {
    int64_t id;
    std::string title;
    std::string content;
    SqlDate last_modified;  // Updated on both INSERT and UPDATE
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<SimpleDocument, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&SimpleDocument::id),
        
        "title", make_tags<SqlTags{
            .not_null = true,
            .length = 200
        }>(&SimpleDocument::title),
        
        "content", make_tags<SqlTags{
            .not_null = true
        }>(&SimpleDocument::content),
        
        // Combined behavior: acts as both created_at and updated_at
        "last_modified", make_tags<SqlTags{
            .not_null = true,
            .created_at = true,
            .updated_at = true
        }>(&SimpleDocument::last_modified)
    );
};
NEKO_END_NAMESPACE

// Example 3: Complex model with multiple timestamp patterns
struct BlogPost {
    int64_t id;
    std::string title;
    std::string content;
    std::string author;
    SqlDate created_at;      // Creation time (never changes)
    SqlDate updated_at;      // Last modification time
    SqlDate published_at;    // Publication time (manually set)
    SqlDate last_viewed;     // Last view time (manually managed)
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<BlogPost, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&BlogPost::id),
        
        "title", make_tags<SqlTags{
            .not_null = true,
            .length = 200
        }>(&BlogPost::title),
        
        "content", make_tags<SqlTags{
            .not_null = true
        }>(&BlogPost::content),
        
        "author", make_tags<SqlTags{
            .not_null = true,
            .length = 100
        }>(&BlogPost::author),
        
        // Automatic creation timestamp
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true
        }>(&BlogPost::created_at),
        
        // Automatic update timestamp
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&BlogPost::updated_at),
        
        // Manual timestamp - no automatic behavior
        "published_at", &BlogPost::published_at,  // Nullable, manually set
        
        // Manual timestamp - no automatic behavior
        "last_viewed", &BlogPost::last_viewed     // Nullable, manually set
    );
};
NEKO_END_NAMESPACE

// Example 4: Session tracking with automatic expiration
struct UserSession {
    std::string session_id;
    int64_t user_id;
    SqlDate created_at;
    SqlDate last_activity;   // Updated on each request
    SqlDate expires_at;      // Calculated based on last_activity
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<UserSession, void> {
    constexpr static auto value = Object(
        "session_id", make_tags<SqlTags{
            .primary_key = true,
            .length = 64
        }>(&UserSession::session_id),
        
        "user_id", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserSession::user_id),
        
        // Set once when session is created
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true
        }>(&UserSession::created_at),
        
        // Updated on every request
        "last_activity", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&UserSession::last_activity),
        
        // Manually calculated expiration time
        "expires_at", make_tags<SqlTags{
            .not_null = true,
            .index = true  // For cleanup queries
        }>(&UserSession::expires_at)
    );
};
NEKO_END_NAMESPACE

// Demonstration functions
IoTask<void> demonstrate_timestamp_schemas() {
    std::cout << "=== Timestamp Management Schema Demo ===" << std::endl;
    
    // Show how timestamp fields are handled in different databases
    std::cout << "\n1. AuditableUser schema comparison:" << std::endl;
    
    auto sqliteSchema = SchemaGenerator<SqliteTag>::generateCreateTable<AuditableUser>("users");
    std::cout << "SQLite:\n" << sqliteSchema << std::endl;
    
    auto mysqlSchema = SchemaGenerator<MysqlTag>::generateCreateTable<AuditableUser>("users");
    std::cout << "\nMySQL:\n" << mysqlSchema << std::endl;
    
    auto postgresSchema = SchemaGenerator<PostgresTag>::generateCreateTable<AuditableUser>("users");
    std::cout << "\nPostgreSQL:\n" << postgresSchema << std::endl;
    
    // Show combined timestamp behavior
    std::cout << "\n2. SimpleDocument with combined timestamp behavior:" << std::endl;
    
    auto mysqlDocSchema = SchemaGenerator<MysqlTag>::generateCreateTable<SimpleDocument>("documents");
    std::cout << "MySQL (with ON UPDATE):\n" << mysqlDocSchema << std::endl;
    
    auto sqliteDocSchema = SchemaGenerator<SqliteTag>::generateCreateTable<SimpleDocument>("documents");
    std::cout << "\nSQLite (application-level updates):\n" << sqliteDocSchema << std::endl;
}

IoTask<void> demonstrate_timestamp_behavior() {
    std::cout << "\n=== Timestamp Behavior Demo ===" << std::endl;
    
    // Example 1: Creating a new user (created_at and updated_at both set)
    std::cout << "\n1. Creating new user:" << std::endl;
    
    AuditableUser newUser;
    newUser.id = 0;  // Will be auto-generated
    newUser.username = "alice";
    newUser.email = "alice@example.com";
    // created_at and updated_at will be automatically set
    
    std::cout << "Before INSERT:" << std::endl;
    std::cout << "  created_at: " << newUser.created_at.toString() << std::endl;
    std::cout << "  updated_at: " << newUser.updated_at.toString() << std::endl;
    
    // Simulate timestamp application (normally done by ORM)
    auto now = SqlDate(std::chrono::system_clock::now());
    newUser.created_at = now;
    newUser.updated_at = now;
    
    std::cout << "After automatic timestamp application:" << std::endl;
    std::cout << "  created_at: " << newUser.created_at.toString() << std::endl;
    std::cout << "  updated_at: " << newUser.updated_at.toString() << std::endl;
    
    // Example 2: Updating existing user (only updated_at changes)
    std::cout << "\n2. Updating existing user:" << std::endl;
    
    auto originalCreatedAt = newUser.created_at;
    auto originalUpdatedAt = newUser.updated_at;
    
    // Simulate some time passing
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    newUser.email = "alice.smith@example.com";  // Change email
    // Only updated_at should change, created_at should remain the same
    
    std::cout << "Before UPDATE:" << std::endl;
    std::cout << "  created_at: " << newUser.created_at.toString() << " (should not change)" << std::endl;
    std::cout << "  updated_at: " << newUser.updated_at.toString() << " (should be updated)" << std::endl;
    
    // Simulate UPDATE timestamp application
    newUser.updated_at = SqlDate(std::chrono::system_clock::now());
    // created_at remains unchanged
    
    std::cout << "After UPDATE timestamp application:" << std::endl;
    std::cout << "  created_at: " << newUser.created_at.toString() << " (unchanged)" << std::endl;
    std::cout << "  updated_at: " << newUser.updated_at.toString() << " (updated)" << std::endl;
    
    // Example 3: Manual timestamp override
    std::cout << "\n3. Manual timestamp override:" << std::endl;
    
    BlogPost post;
    post.id = 0;
    post.title = "My First Post";
    post.content = "This is the content of my first blog post.";
    post.author = "Alice";
    
    // Manually set publication time to a specific date
    post.published_at = SqlDate(2023, 6, 15, 10, 30, 0);  // June 15, 2023, 10:30 AM
    
    std::cout << "Manual publication time: " << post.published_at.value().toString() << std::endl;
    
    // Automatic timestamps will still be applied
    post.created_at = SqlDate(std::chrono::system_clock::now());
    post.updated_at = post.created_at;
    
    std::cout << "Automatic created_at: " << post.created_at.toString() << std::endl;
    std::cout << "Automatic updated_at: " << post.updated_at.toString() << std::endl;
}

IoTask<void> demonstrate_timestamp_helpers() {
    std::cout << "\n=== Timestamp Helper Methods Demo ===" << std::endl;
    
    // Test helper methods
    std::cout << "\n1. Testing hasTimestampBehavior():" << std::endl;
    
    SqlTags createdAtTags{.created_at = true};
    std::cout << "created_at only: " << (createdAtTags.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
    
    SqlTags updatedAtTags{.updated_at = true};
    std::cout << "updated_at only: " << (updatedAtTags.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
    
    SqlTags bothTags{.created_at = true, .updated_at = true};
    std::cout << "Both timestamps: " << (bothTags.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
    
    SqlTags noTimestamps{.not_null = true};
    std::cout << "No timestamps: " << (noTimestamps.hasTimestampBehavior() ? "Yes" : "No") << std::endl;
    
    // Show timestamp field extraction
    std::cout << "\n2. Timestamp field extraction:" << std::endl;
    
    // This would be used internally by the ORM system
    std::cout << "Fields with created_at behavior in AuditableUser:" << std::endl;
    std::cout << "  - created_at" << std::endl;
    
    std::cout << "Fields with updated_at behavior in AuditableUser:" << std::endl;
    std::cout << "  - updated_at" << std::endl;
    
    std::cout << "Fields with both behaviors in SimpleDocument:" << std::endl;
    std::cout << "  - last_modified" << std::endl;
}

IoTask<void> demonstrate_session_management() {
    std::cout << "\n=== Session Management Example ===" << std::endl;
    
    // Example of using timestamps for session management
    UserSession session;
    session.session_id = "abc123def456ghi789jkl012mno345pqr678stu901vwx234yzab567cdef890";
    session.user_id = 12345;
    
    // Automatic timestamps
    auto now = SqlDate(std::chrono::system_clock::now());
    session.created_at = now;
    session.last_activity = now;
    
    // Calculate expiration (e.g., 24 hours from creation)
    auto expiration_time = std::chrono::system_clock::now() + std::chrono::hours(24);
    session.expires_at = SqlDate(expiration_time);
    
    std::cout << "New session created:" << std::endl;
    std::cout << "  Session ID: " << session.session_id << std::endl;
    std::cout << "  User ID: " << session.user_id << std::endl;
    std::cout << "  Created: " << session.created_at.toString() << std::endl;
    std::cout << "  Last Activity: " << session.last_activity.toString() << std::endl;
    std::cout << "  Expires: " << session.expires_at.toString() << std::endl;
    
    // Simulate session activity update
    std::cout << "\nSimulating session activity update..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Only last_activity gets updated (updated_at behavior)
    session.last_activity = SqlDate(std::chrono::system_clock::now());
    
    // Recalculate expiration based on new activity
    auto new_expiration = std::chrono::system_clock::now() + std::chrono::hours(24);
    session.expires_at = SqlDate(new_expiration);
    
    std::cout << "After activity update:" << std::endl;
    std::cout << "  Created: " << session.created_at.toString() << " (unchanged)" << std::endl;
    std::cout << "  Last Activity: " << session.last_activity.toString() << " (updated)" << std::endl;
    std::cout << "  Expires: " << session.expires_at.toString() << " (recalculated)" << std::endl;
}

IoTask<void> demonstrate_database_differences() {
    std::cout << "\n=== Database-Specific Timestamp Handling ===" << std::endl;
    
    std::cout << "\n1. MySQL - Full automatic support:" << std::endl;
    std::cout << "   • created_at: DEFAULT CURRENT_TIMESTAMP" << std::endl;
    std::cout << "   • updated_at: ON UPDATE CURRENT_TIMESTAMP" << std::endl;
    std::cout << "   • Database handles updates automatically" << std::endl;
    
    std::cout << "\n2. SQLite - Application-level handling:" << std::endl;
    std::cout << "   • created_at: DEFAULT CURRENT_TIMESTAMP" << std::endl;
    std::cout << "   • updated_at: No ON UPDATE support" << std::endl;
    std::cout << "   • Application must update timestamps manually" << std::endl;
    
    std::cout << "\n3. PostgreSQL - Mixed approach:" << std::endl;
    std::cout << "   • created_at: DEFAULT CURRENT_TIMESTAMP" << std::endl;
    std::cout << "   • updated_at: No ON UPDATE support (use triggers or application)" << std::endl;
    std::cout << "   • Application handles updates or use database triggers" << std::endl;
    
    std::cout << "\n4. Cross-database compatibility:" << std::endl;
    std::cout << "   • SqlTags provides consistent interface" << std::endl;
    std::cout << "   • ORM handles database-specific differences" << std::endl;
    std::cout << "   • Application code remains the same" << std::endl;
}

// Main function to run all timestamp examples
IoTask<void> run_timestamp_examples() {
    try {
        co_await demonstrate_timestamp_schemas();
        co_await demonstrate_timestamp_behavior();
        co_await demonstrate_timestamp_helpers();
        co_await demonstrate_session_management();
        co_await demonstrate_database_differences();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "Timestamp management with SqlTags provides:" << std::endl;
        std::cout << "• created_at: Automatic creation timestamp (set once)" << std::endl;
        std::cout << "• updated_at: Automatic modification timestamp (updated on changes)" << std::endl;
        std::cout << "• Combined behavior: Single field for both creation and updates" << std::endl;
        std::cout << "• Manual override: Ability to set timestamps manually when needed" << std::endl;
        std::cout << "• Cross-database compatibility: Works consistently across database systems" << std::endl;
        std::cout << "\nThe ORM automatically handles timestamp population based on SqlTags configuration," << std::endl;
        std::cout << "reducing boilerplate code and ensuring consistent audit trails." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Uncomment to run as standalone example
// int main() {
//     return ilias::run(run_timestamp_examples());
// }