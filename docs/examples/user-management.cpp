/**
 * @file user-management.cpp
 * @brief Complete user management system demonstrating real-world SqlTags usage
 * 
 * This example shows a comprehensive user management system with:
 * - User accounts with authentication
 * - User profiles with extended information
 * - Role-based access control
 * - Session management
 * - Audit logging
 * - Password reset tokens
 */

#include <ilias/platform.hpp>
#include <ilias/sql/sqldatabase.hpp>
#include <ilias/sql_orm/detail/orm_types.hpp>
#include <ilias/sql_orm/detail/schema_generator.hpp>
#include <iostream>
#include <optional>

using namespace ilias;
using namespace ilias::sql;

// Core user account
struct User {
    int64_t id;
    std::string username;
    std::string email;
    std::string password_hash;
    bool is_active;
    bool email_verified;
    SqlDate created_at;
    SqlDate updated_at;
    SqlDate last_login;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<User, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&User::id),
        
        // Username: unique identifier, indexed for fast lookups
        "username", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,
            .length = 50
        }>(&User::username),
        
        // Email: unique, indexed for authentication and lookups
        "email", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,
            .length = 255
        }>(&User::email),
        
        // Password hash: not null, reasonable length for bcrypt/argon2
        "password_hash", make_tags<SqlTags{
            .not_null = true,
            .length = 255
        }>(&User::password_hash),
        
        // Status flags
        "is_active", make_tags<SqlTags{
            .not_null = true,
            .index = true  // For filtering active users
        }>(&User::is_active),
        
        "email_verified", make_tags<SqlTags{
            .not_null = true,
            .index = true  // For filtering verified users
        }>(&User::email_verified),
        
        // Audit timestamps
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true,
            .index = true  // For sorting by registration date
        }>(&User::created_at),
        
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&User::updated_at),
        
        // Last login: nullable, indexed for analytics
        "last_login", make_tags<SqlTags{
            .index = true
        }>(&User::last_login)
    );
};
NEKO_END_NAMESPACE

// Extended user profile information
struct UserProfile {
    int64_t user_id;  // Foreign key to User
    std::string first_name;
    std::string last_name;
    std::string display_name;
    std::string bio;
    std::string avatar_url;
    std::string phone;
    SqlDate birth_date;
    std::string timezone;
    std::string language;
    SqlDate updated_at;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<UserProfile, void> {
    constexpr static auto value = Object(
        // Primary key is the user_id (one profile per user)
        "user_id", make_tags<SqlTags{
            .primary_key = true
        }>(&UserProfile::user_id),
        
        // Name fields: not null, indexed for search
        "first_name", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 100
        }>(&UserProfile::first_name),
        
        "last_name", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 100
        }>(&UserProfile::last_name),
        
        // Display name: indexed for search and mentions
        "display_name", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 150
        }>(&UserProfile::display_name),
        
        // Bio: text field, no length limit
        "bio", &UserProfile::bio,
        
        // Avatar URL: optional, reasonable length for URLs
        "avatar_url", make_tags<SqlTags{
            .length = 500
        }>(&UserProfile::avatar_url),
        
        // Phone: optional, indexed for lookups
        "phone", make_tags<SqlTags{
            .index = true,
            .length = 20
        }>(&UserProfile::phone),
        
        // Birth date: optional, for age calculations
        "birth_date", &UserProfile::birth_date,
        
        // Preferences
        "timezone", make_tags<SqlTags{
            .length = 50
        }>(&UserProfile::timezone),
        
        "language", make_tags<SqlTags{
            .length = 10
        }>(&UserProfile::language),
        
        // Profile update tracking
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&UserProfile::updated_at)
    );
};
NEKO_END_NAMESPACE

// Role-based access control
struct Role {
    int64_t id;
    std::string name;
    std::string description;
    bool is_active;
    SqlDate created_at;
    SqlDate updated_at;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<Role, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&Role::id),
        
        // Role name: unique, indexed
        "name", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,
            .length = 50
        }>(&Role::name),
        
        "description", make_tags<SqlTags{
            .length = 255
        }>(&Role::description),
        
        "is_active", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&Role::is_active),
        
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true
        }>(&Role::created_at),
        
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&Role::updated_at)
    );
};
NEKO_END_NAMESPACE

// User-Role assignment (many-to-many)
struct UserRole {
    int64_t id;
    int64_t user_id;
    int64_t role_id;
    SqlDate assigned_at;
    int64_t assigned_by;  // User ID who assigned the role
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<UserRole, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&UserRole::id),
        
        // Foreign keys: indexed for joins
        "user_id", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserRole::user_id),
        
        "role_id", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserRole::role_id),
        
        "assigned_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true,
            .index = true
        }>(&UserRole::assigned_at),
        
        "assigned_by", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserRole::assigned_by)
    );
};
NEKO_END_NAMESPACE

// Session management
struct UserSession {
    std::string session_id;
    int64_t user_id;
    std::string ip_address;
    std::string user_agent;
    SqlDate created_at;
    SqlDate last_activity;
    SqlDate expires_at;
    bool is_active;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<UserSession, void> {
    constexpr static auto value = Object(
        // Session ID as primary key
        "session_id", make_tags<SqlTags{
            .primary_key = true,
            .length = 128
        }>(&UserSession::session_id),
        
        // User reference: indexed for user session queries
        "user_id", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserSession::user_id),
        
        // Session metadata
        "ip_address", make_tags<SqlTags{
            .not_null = true,
            .index = true,  // For security analysis
            .length = 45    // IPv6 length
        }>(&UserSession::ip_address),
        
        "user_agent", make_tags<SqlTags{
            .length = 500
        }>(&UserSession::user_agent),
        
        // Timestamps
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true,
            .index = true
        }>(&UserSession::created_at),
        
        "last_activity", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true,
            .index = true  // For cleanup queries
        }>(&UserSession::last_activity),
        
        "expires_at", make_tags<SqlTags{
            .not_null = true,
            .index = true  // For cleanup queries
        }>(&UserSession::expires_at),
        
        "is_active", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserSession::is_active)
    );
};
NEKO_END_NAMESPACE

// Password reset tokens
struct PasswordResetToken {
    std::string token;
    int64_t user_id;
    SqlDate created_at;
    SqlDate expires_at;
    bool is_used;
    SqlDate used_at;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<PasswordResetToken, void> {
    constexpr static auto value = Object(
        "token", make_tags<SqlTags{
            .primary_key = true,
            .length = 64
        }>(&PasswordResetToken::token),
        
        "user_id", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&PasswordResetToken::user_id),
        
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true,
            .index = true
        }>(&PasswordResetToken::created_at),
        
        "expires_at", make_tags<SqlTags{
            .not_null = true,
            .index = true  // For cleanup
        }>(&PasswordResetToken::expires_at),
        
        "is_used", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&PasswordResetToken::is_used),
        
        "used_at", &PasswordResetToken::used_at
    );
};
NEKO_END_NAMESPACE

// Audit log for user actions
struct UserAuditLog {
    int64_t id;
    int64_t user_id;
    std::string action;
    std::string resource_type;
    std::string resource_id;
    std::string old_values;
    std::string new_values;
    std::string ip_address;
    std::string user_agent;
    SqlDate timestamp;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<UserAuditLog, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&UserAuditLog::id),
        
        // User reference: indexed for user activity queries
        "user_id", make_tags<SqlTags{
            .not_null = true,
            .index = true
        }>(&UserAuditLog::user_id),
        
        // Action details: indexed for filtering
        "action", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 50
        }>(&UserAuditLog::action),
        
        "resource_type", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 50
        }>(&UserAuditLog::resource_type),
        
        "resource_id", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 100
        }>(&UserAuditLog::resource_id),
        
        // Change tracking: no length limit for JSON data
        "old_values", &UserAuditLog::old_values,
        "new_values", &UserAuditLog::new_values,
        
        // Request metadata
        "ip_address", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 45
        }>(&UserAuditLog::ip_address),
        
        "user_agent", make_tags<SqlTags{
            .length = 500
        }>(&UserAuditLog::user_agent),
        
        // Timestamp: indexed for time-based queries
        "timestamp", make_tags<SqlTags{
            .not_null = true,
            .created_at = true,
            .index = true
        }>(&UserAuditLog::timestamp)
    );
};
NEKO_END_NAMESPACE

// Demonstration functions
IoTask<void> demonstrate_user_management_schemas() {
    std::cout << "=== User Management System Schemas ===" << std::endl;
    
    // Show core user table
    std::cout << "\n1. Core User table (MySQL):" << std::endl;
    auto userSchema = SchemaGenerator<MysqlTag>::generateCreateTable<User>("users");
    std::cout << userSchema << std::endl;
    
    // Show user profile table
    std::cout << "\n2. User Profile table (MySQL):" << std::endl;
    auto profileSchema = SchemaGenerator<MysqlTag>::generateCreateTable<UserProfile>("user_profiles");
    std::cout << profileSchema << std::endl;
    
    // Show role system
    std::cout << "\n3. Role table (MySQL):" << std::endl;
    auto roleSchema = SchemaGenerator<MysqlTag>::generateCreateTable<Role>("roles");
    std::cout << roleSchema << std::endl;
    
    std::cout << "\n4. User-Role assignment table (MySQL):" << std::endl;
    auto userRoleSchema = SchemaGenerator<MysqlTag>::generateCreateTable<UserRole>("user_roles");
    std::cout << userRoleSchema << std::endl;
    
    // Show session management
    std::cout << "\n5. Session table (MySQL):" << std::endl;
    auto sessionSchema = SchemaGenerator<MysqlTag>::generateCreateTable<UserSession>("user_sessions");
    std::cout << sessionSchema << std::endl;
}

IoTask<void> demonstrate_user_lifecycle() {
    std::cout << "\n=== User Lifecycle Demo ===" << std::endl;
    
    // 1. User registration
    std::cout << "\n1. User Registration:" << std::endl;
    
    User newUser;
    newUser.id = 0;  // Auto-generated
    newUser.username = "alice_smith";
    newUser.email = "alice@example.com";
    newUser.password_hash = "$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMQJqhN8/LewdBPj/RK.PJ/..G";  // bcrypt hash
    newUser.is_active = true;
    newUser.email_verified = false;  // Will be verified later
    // created_at and updated_at automatically set
    
    std::cout << "New user created:" << std::endl;
    std::cout << "  Username: " << newUser.username << std::endl;
    std::cout << "  Email: " << newUser.email << std::endl;
    std::cout << "  Active: " << (newUser.is_active ? "Yes" : "No") << std::endl;
    std::cout << "  Email Verified: " << (newUser.email_verified ? "Yes" : "No") << std::endl;
    
    // 2. Profile creation
    std::cout << "\n2. Profile Creation:" << std::endl;
    
    UserProfile profile;
    profile.user_id = 1;  // Assuming user ID 1 after insert
    profile.first_name = "Alice";
    profile.last_name = "Smith";
    profile.display_name = "Alice S.";
    profile.bio = "Software developer passionate about clean code and user experience.";
    profile.timezone = "America/New_York";
    profile.language = "en";
    // updated_at automatically set
    
    std::cout << "Profile created:" << std::endl;
    std::cout << "  Name: " << profile.first_name << " " << profile.last_name << std::endl;
    std::cout << "  Display: " << profile.display_name << std::endl;
    std::cout << "  Timezone: " << profile.timezone << std::endl;
    
    // 3. Role assignment
    std::cout << "\n3. Role Assignment:" << std::endl;
    
    Role userRole;
    userRole.id = 0;  // Auto-generated
    userRole.name = "user";
    userRole.description = "Standard user with basic permissions";
    userRole.is_active = true;
    // created_at and updated_at automatically set
    
    UserRole assignment;
    assignment.id = 0;  // Auto-generated
    assignment.user_id = 1;
    assignment.role_id = 1;  // Assuming role ID 1
    assignment.assigned_by = 1;  // Self-assigned or by admin
    // assigned_at automatically set
    
    std::cout << "Role assigned:" << std::endl;
    std::cout << "  Role: " << userRole.name << std::endl;
    std::cout << "  Description: " << userRole.description << std::endl;
    
    // 4. Session creation
    std::cout << "\n4. Session Creation:" << std::endl;
    
    UserSession session;
    session.session_id = "sess_abc123def456ghi789jkl012mno345pqr678stu901vwx234yzab567cdef890";
    session.user_id = 1;
    session.ip_address = "192.168.1.100";
    session.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    session.is_active = true;
    // created_at and last_activity automatically set
    // expires_at calculated based on session timeout
    
    std::cout << "Session created:" << std::endl;
    std::cout << "  Session ID: " << session.session_id.substr(0, 20) << "..." << std::endl;
    std::cout << "  IP Address: " << session.ip_address << std::endl;
    std::cout << "  Active: " << (session.is_active ? "Yes" : "No") << std::endl;
}

IoTask<void> demonstrate_security_features() {
    std::cout << "\n=== Security Features Demo ===" << std::endl;
    
    // 1. Password reset token
    std::cout << "\n1. Password Reset Token:" << std::endl;
    
    PasswordResetToken resetToken;
    resetToken.token = "reset_abc123def456ghi789jkl012mno345pqr678stu901vwx234yzab567";
    resetToken.user_id = 1;
    resetToken.is_used = false;
    // created_at automatically set
    // expires_at calculated (e.g., 1 hour from creation)
    
    std::cout << "Reset token generated:" << std::endl;
    std::cout << "  Token: " << resetToken.token.substr(0, 20) << "..." << std::endl;
    std::cout << "  User ID: " << resetToken.user_id << std::endl;
    std::cout << "  Used: " << (resetToken.is_used ? "Yes" : "No") << std::endl;
    
    // 2. Audit logging
    std::cout << "\n2. Audit Logging:" << std::endl;
    
    UserAuditLog auditEntry;
    auditEntry.id = 0;  // Auto-generated
    auditEntry.user_id = 1;
    auditEntry.action = "password_change";
    auditEntry.resource_type = "user";
    auditEntry.resource_id = "1";
    auditEntry.old_values = "{\"password_hash\": \"old_hash\"}";
    auditEntry.new_values = "{\"password_hash\": \"new_hash\"}";
    auditEntry.ip_address = "192.168.1.100";
    auditEntry.user_agent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36";
    // timestamp automatically set
    
    std::cout << "Audit entry created:" << std::endl;
    std::cout << "  Action: " << auditEntry.action << std::endl;
    std::cout << "  Resource: " << auditEntry.resource_type << " #" << auditEntry.resource_id << std::endl;
    std::cout << "  IP: " << auditEntry.ip_address << std::endl;
    
    // 3. Session security
    std::cout << "\n3. Session Security Features:" << std::endl;
    std::cout << "  • Session IDs are cryptographically secure random strings" << std::endl;
    std::cout << "  • IP address tracking for session hijacking detection" << std::endl;
    std::cout << "  • User agent tracking for device identification" << std::endl;
    std::cout << "  • Automatic expiration with configurable timeout" << std::endl;
    std::cout << "  • Activity tracking for idle session cleanup" << std::endl;
}

IoTask<void> demonstrate_query_patterns() {
    std::cout << "\n=== Common Query Patterns ===" << std::endl;
    
    std::cout << "\n1. User Authentication Queries:" << std::endl;
    std::cout << "   -- Login by username or email (uses unique indexes)" << std::endl;
    std::cout << "   SELECT * FROM users WHERE username = ? AND is_active = true;" << std::endl;
    std::cout << "   SELECT * FROM users WHERE email = ? AND is_active = true;" << std::endl;
    
    std::cout << "\n2. User Profile Queries:" << std::endl;
    std::cout << "   -- Get complete user info with profile (uses primary key join)" << std::endl;
    std::cout << "   SELECT u.*, p.* FROM users u" << std::endl;
    std::cout << "   LEFT JOIN user_profiles p ON u.id = p.user_id" << std::endl;
    std::cout << "   WHERE u.id = ?;" << std::endl;
    
    std::cout << "\n3. Role-Based Access Queries:" << std::endl;
    std::cout << "   -- Get user roles (uses indexed foreign keys)" << std::endl;
    std::cout << "   SELECT r.name FROM roles r" << std::endl;
    std::cout << "   JOIN user_roles ur ON r.id = ur.role_id" << std::endl;
    std::cout << "   WHERE ur.user_id = ? AND r.is_active = true;" << std::endl;
    
    std::cout << "\n4. Session Management Queries:" << std::endl;
    std::cout << "   -- Validate session (uses primary key)" << std::endl;
    std::cout << "   SELECT * FROM user_sessions" << std::endl;
    std::cout << "   WHERE session_id = ? AND is_active = true AND expires_at > NOW();" << std::endl;
    
    std::cout << "   -- Cleanup expired sessions (uses expires_at index)" << std::endl;
    std::cout << "   DELETE FROM user_sessions WHERE expires_at < NOW();" << std::endl;
    
    std::cout << "\n5. Audit and Analytics Queries:" << std::endl;
    std::cout << "   -- User activity report (uses timestamp index)" << std::endl;
    std::cout << "   SELECT action, COUNT(*) FROM user_audit_log" << std::endl;
    std::cout << "   WHERE user_id = ? AND timestamp >= ?" << std::endl;
    std::cout << "   GROUP BY action;" << std::endl;
    
    std::cout << "   -- Security analysis (uses ip_address index)" << std::endl;
    std::cout << "   SELECT ip_address, COUNT(DISTINCT user_id) as user_count" << std::endl;
    std::cout << "   FROM user_audit_log" << std::endl;
    std::cout << "   WHERE timestamp >= ?" << std::endl;
    std::cout << "   GROUP BY ip_address" << std::endl;
    std::cout << "   HAVING user_count > 5;" << std::endl;
}

IoTask<void> demonstrate_performance_considerations() {
    std::cout << "\n=== Performance Considerations ===" << std::endl;
    
    std::cout << "\n1. Index Strategy:" << std::endl;
    std::cout << "   • Primary keys: Automatic clustering and uniqueness" << std::endl;
    std::cout << "   • Unique constraints: Automatic index creation" << std::endl;
    std::cout << "   • Foreign keys: Indexed for fast joins" << std::endl;
    std::cout << "   • Search fields: Indexed for WHERE clauses" << std::endl;
    std::cout << "   • Timestamp fields: Indexed for range queries and cleanup" << std::endl;
    
    std::cout << "\n2. String Length Optimization:" << std::endl;
    std::cout << "   • Short identifiers: VARCHAR with specific length" << std::endl;
    std::cout << "   • Long content: TEXT without length restriction" << std::endl;
    std::cout << "   • URLs and paths: VARCHAR with generous but bounded length" << std::endl;
    
    std::cout << "\n3. Timestamp Automation Benefits:" << std::endl;
    std::cout << "   • Reduces application code complexity" << std::endl;
    std::cout << "   • Ensures consistent audit trails" << std::endl;
    std::cout << "   • Database-level defaults for reliability" << std::endl;
    
    std::cout << "\n4. Query Optimization:" << std::endl;
    std::cout << "   • Compound indexes for multi-column queries" << std::endl;
    std::cout << "   • Partial indexes for filtered queries" << std::endl;
    std::cout << "   • Regular cleanup of expired data" << std::endl;
}

// Main function to run all user management examples
IoTask<void> run_user_management_examples() {
    try {
        co_await demonstrate_user_management_schemas();
        co_await demonstrate_user_lifecycle();
        co_await demonstrate_security_features();
        co_await demonstrate_query_patterns();
        co_await demonstrate_performance_considerations();
        
        std::cout << "\n=== Summary ===" << std::endl;
        std::cout << "This user management system demonstrates:" << std::endl;
        std::cout << "• Comprehensive user account management with authentication" << std::endl;
        std::cout << "• Extended user profiles with flexible information storage" << std::endl;
        std::cout << "• Role-based access control with many-to-many relationships" << std::endl;
        std::cout << "• Secure session management with automatic cleanup" << std::endl;
        std::cout << "• Password reset functionality with token expiration" << std::endl;
        std::cout << "• Complete audit logging for security and compliance" << std::endl;
        std::cout << "• Optimized database schema with appropriate indexes" << std::endl;
        std::cout << "• Automatic timestamp management for audit trails" << std::endl;
        std::cout << "\nAll tables use SqlTags to ensure consistent, optimized" << std::endl;
        std::cout << "database schemas across different database systems." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// Uncomment to run as standalone example
// int main() {
//     return ilias::run(run_user_management_examples());
// }