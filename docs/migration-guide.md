# SqlTags Migration Guide

This guide helps you migrate existing IliasSql code to use the enhanced SqlTags system.

## Overview

The enhanced SqlTags system is backward compatible with existing code, but provides many new features that can improve your database schema definitions and runtime behavior.

## Migration Steps

### Step 1: Assess Current Usage

First, identify how you're currently using SqlTags in your codebase:

#### Basic Usage (No Migration Needed)

If you're using simple SqlTags like this, no changes are required:

```cpp
// This continues to work unchanged
template<> struct Meta<User, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true}>(&User::id),
        "name", make_tags<SqlTags{.not_null = true}>(&User::name),
        "email", &User::email  // No tags
    );
};
```

#### Manual Schema Creation

If you're manually creating database schemas, you can now generate them automatically:

```cpp
// OLD: Manual schema creation
co_await db.execute(R"(
    CREATE TABLE users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        email TEXT UNIQUE
    )
)");

// NEW: Automatic schema generation
auto createTableSQL = SchemaGenerator<SqliteTag>::generateCreateTable<User>("users");
co_await db.execute(createTableSQL);
```

### Step 2: Add Enhanced Constraints

Enhance your existing SqlTags with new constraint options:

#### Before: Basic Constraints

```cpp
struct User {
    int64_t id;
    std::string name;
    std::string email;
};

template<> struct Meta<User, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true}>(&User::id),
        "name", make_tags<SqlTags{.not_null = true}>(&User::name),
        "email", make_tags<SqlTags{.unique = true}>(&User::email)
    );
};
```

#### After: Enhanced Constraints

```cpp
struct User {
    int64_t id;
    std::string name;
    std::string email;
    SqlDate created_at;
    SqlDate updated_at;
};

template<> struct Meta<User, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true  // NEW: Auto increment
        }>(&User::id),
        
        "name", make_tags<SqlTags{
            .not_null = true,
            .length = 100,          // NEW: Explicit length
            .index = true           // NEW: Index for faster queries
        }>(&User::name),
        
        "email", make_tags<SqlTags{
            .not_null = true,       // NEW: Make required
            .unique = true,
            .length = 255           // NEW: Explicit length
        }>(&User::email),
        
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true      // NEW: Automatic timestamp
        }>(&User::created_at),
        
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true      // NEW: Automatic timestamp
        }>(&User::updated_at)
    );
};
```

### Step 3: Migrate Timestamp Handling

#### Before: Manual Timestamp Management

```cpp
// OLD: Manual timestamp handling
User user;
user.id = 0;
user.name = "John Doe";
user.email = "john@example.com";
user.created_at = SqlDate(std::chrono::system_clock::now());  // Manual

auto stmt = co_await db.prepare<User>("INSERT INTO users VALUES (:id, :name, :email, :created_at)");
stmt.bind(user);
co_await stmt.execute();

// For updates, manually set updated_at
user.name = "John Smith";
user.updated_at = SqlDate(std::chrono::system_clock::now());  // Manual
auto updateStmt = co_await db.prepare<User>("UPDATE users SET name = :name, updated_at = :updated_at WHERE id = :id");
```

#### After: Automatic Timestamp Management

```cpp
// NEW: Automatic timestamp handling
User user;
user.id = 0;  // Will be auto-generated
user.name = "John Doe";
user.email = "john@example.com";
// created_at and updated_at are automatically set

auto stmt = co_await db.prepare<User>("INSERT INTO users VALUES (:id, :name, :email, :created_at, :updated_at)");
stmt.bind(user);
co_await stmt.execute();  // Timestamps automatically populated

// For updates, updated_at is automatically set
user.name = "John Smith";
auto updateStmt = co_await db.prepare<User>("UPDATE users SET name = :name, updated_at = :updated_at WHERE id = :id");
updateStmt.bind(user);
co_await updateStmt.execute();  // updated_at automatically updated
```

### Step 4: Add Data Type Modifiers

#### Before: Generic Types

```cpp
struct Product {
    int64_t id;
    std::string name;
    double price;
    int quantity;
};

template<> struct Meta<Product, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true}>(&Product::id),
        "name", make_tags<SqlTags{.not_null = true}>(&Product::name),
        "price", make_tags<SqlTags{.not_null = true}>(&Product::price),
        "quantity", make_tags<SqlTags{.not_null = true}>(&Product::quantity)
    );
};
```

#### After: Optimized Types

```cpp
struct Product {
    int64_t id;
    std::string name;
    double price;
    uint32_t quantity;  // Changed to unsigned
};

template<> struct Meta<Product, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&Product::id),
        
        "name", make_tags<SqlTags{
            .not_null = true,
            .length = 200,          // NEW: Appropriate length
            .index = true           // NEW: Index for searches
        }>(&Product::name),
        
        "price", make_tags<SqlTags{
            .not_null = true,
            .unsigned_type = true   // NEW: Unsigned for positive values
        }>(&Product::price),
        
        "quantity", make_tags<SqlTags{
            .not_null = true,
            .unsigned_type = true   // NEW: Unsigned for quantities
        }>(&Product::quantity)
    );
};
```

### Step 5: Implement Validation

Add validation to catch configuration errors early:

```cpp
// Add validation in your initialization code
template<typename T>
void validateTableConfiguration() {
    // This would be implemented as part of your ORM form
    auto errors = Form<T, SqliteTag>::validateTableConfiguration();
    if (!errors.empty()) {
        for (const auto& error : errors) {
            ILIAS_ERROR("Schema", "Validation error for {}: {}", 
                       typeid(T).name(), error);
        }
        throw std::runtime_error("Schema validation failed");
    }
}

// Call during application startup
validateTableConfiguration<User>();
validateTableConfiguration<Product>();
```

## Common Migration Patterns

### Pattern 1: ID Fields

#### Before
```cpp
"id", make_tags<SqlTags{.primary_key = true}>(&Model::id)
```

#### After
```cpp
"id", make_tags<SqlTags{.primary_key = true, .auto_increment = true}>(&Model::id)
```

### Pattern 2: String Fields

#### Before
```cpp
"name", make_tags<SqlTags{.not_null = true}>(&Model::name)
```

#### After
```cpp
// For short, indexed strings
"name", make_tags<SqlTags{.not_null = true, .length = 100, .index = true}>(&Model::name)

// For long content
"description", make_tags<SqlTags{.not_null = true}>(&Model::description)  // Uses TEXT
```

### Pattern 3: Unique Fields

#### Before
```cpp
"email", make_tags<SqlTags{.unique = true}>(&Model::email)
```

#### After
```cpp
"email", make_tags<SqlTags{.not_null = true, .unique = true, .length = 255}>(&Model::email)
```

### Pattern 4: Numeric Fields

#### Before
```cpp
"price", make_tags<SqlTags{.not_null = true}>(&Model::price)
```

#### After
```cpp
"price", make_tags<SqlTags{.not_null = true, .unsigned_type = true}>(&Model::price)
```

### Pattern 5: Timestamp Fields

#### Before
```cpp
"created_at", make_tags<SqlTags{.not_null = true}>(&Model::created_at)
```

#### After
```cpp
"created_at", make_tags<SqlTags{.not_null = true, .created_at = true}>(&Model::created_at)
"updated_at", make_tags<SqlTags{.not_null = true, .updated_at = true}>(&Model::updated_at)
```

## Database-Specific Considerations

### SQLite Migration

SQLite has some limitations to be aware of:

```cpp
// These features work differently in SQLite:
SqlTags{.unsigned_type = true}  // Ignored (SQLite doesn't have unsigned types)
SqlTags{.updated_at = true}     // Handled at application level (no ON UPDATE)
```

### MySQL Migration

MySQL supports all features:

```cpp
// All features work as expected in MySQL
SqlTags{
    .not_null = true,
    .unsigned_type = true,      // Generates UNSIGNED modifier
    .auto_increment = true,     // Generates AUTO_INCREMENT
    .updated_at = true          // Generates ON UPDATE CURRENT_TIMESTAMP
}
```

### PostgreSQL Migration

PostgreSQL has some differences:

```cpp
// PostgreSQL-specific behavior:
SqlTags{.auto_increment = true}  // Uses SERIAL/BIGSERIAL types
SqlTags{.unsigned_type = true}   // Ignored (PostgreSQL doesn't have unsigned types)
SqlTags{.updated_at = true}      // Handled at application level
```

## Testing Your Migration

### 1. Schema Validation

Test that your new schemas generate correctly:

```cpp
// Test schema generation for each database
auto sqliteSchema = SchemaGenerator<SqliteTag>::generateCreateTable<User>("users");
auto mysqlSchema = SchemaGenerator<MysqlTag>::generateCreateTable<User>("users");
auto postgresSchema = SchemaGenerator<PostgresTag>::generateCreateTable<User>("users");

std::cout << "SQLite: " << sqliteSchema << std::endl;
std::cout << "MySQL: " << mysqlSchema << std::endl;
std::cout << "PostgreSQL: " << postgresSchema << std::endl;
```

### 2. Runtime Behavior

Test that timestamp automation works:

```cpp
// Test automatic timestamp population
User user;
user.name = "Test User";
user.email = "test@example.com";

auto stmt = co_await db.prepare<User>("INSERT INTO users VALUES (:id, :name, :email, :created_at, :updated_at)");
stmt.bind(user);
co_await stmt.execute();

// Verify that created_at was automatically set
assert(!user.created_at.toString().empty());
```

### 3. Validation Testing

Test that validation catches errors:

```cpp
// Test validation
SqlTags invalidTags{.primary_key = true, .length = -1};
assert(!invalidTags.isValid());

auto errors = invalidTags.getValidationErrors();
assert(!errors.empty());
```

## Rollback Strategy

If you need to rollback your migration:

### 1. Keep Old Schema Creation

Keep your old manual schema creation code commented out:

```cpp
// OLD schema creation (keep as backup)
/*
co_await db.execute(R"(
    CREATE TABLE users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL,
        email TEXT UNIQUE
    )
)");
*/

// NEW schema creation
auto createTableSQL = SchemaGenerator<SqliteTag>::generateCreateTable<User>("users");
co_await db.execute(createTableSQL);
```

### 2. Gradual Migration

Migrate one table at a time:

```cpp
// Migrate tables gradually
if (use_enhanced_schema) {
    auto schema = SchemaGenerator<SqliteTag>::generateCreateTable<User>("users");
    co_await db.execute(schema);
} else {
    // Use old manual schema
    co_await db.execute(old_user_schema);
}
```

## Performance Considerations

The enhanced SqlTags system has minimal performance impact:

### Compile Time

- SqlTags validation happens at compile time where possible
- No runtime overhead for constraint checking

### Runtime

- Timestamp automation adds minimal overhead
- Schema generation is typically done once at startup
- Index creation improves query performance

### Memory

- SqlTags are compile-time structures with no runtime memory overhead
- Generated SQL strings are created on-demand

## Troubleshooting

### Common Issues

#### 1. Validation Errors

```cpp
// Problem: Conflicting constraints
SqlTags{.primary_key = true, .unique = false}  // Error: primary key implies unique

// Solution: Remove redundant constraints
SqlTags{.primary_key = true}  // unique is implied
```

#### 2. Type Mismatches

```cpp
// Problem: Auto increment on string field
std::string name;
SqlTags{.auto_increment = true}  // Error: auto increment requires numeric type

// Solution: Use auto increment only on numeric fields
int64_t id;
SqlTags{.auto_increment = true}  // Correct
```

#### 3. Length Issues

```cpp
// Problem: Negative length
SqlTags{.length = -1}  // Error: invalid length

// Solution: Use positive length or zero for unlimited
SqlTags{.length = 255}  // Correct
SqlTags{.length = 0}    // Correct (uses TEXT)
```

### Getting Help

If you encounter issues during migration:

1. Check the validation errors using `getValidationErrors()`
2. Verify your SqlTags configuration against the examples in this guide
3. Test schema generation for your target database
4. Refer to the [API Reference](api-reference.md) for detailed documentation

## Next Steps

After completing your migration:

1. Review the [SqlTags Guide](sql-tags-guide.md) for advanced usage patterns
2. Explore the [Examples](examples/) directory for more complex scenarios
3. Consider adding validation to your CI/CD pipeline to catch schema issues early
4. Monitor your application to ensure the new features work as expected

The enhanced SqlTags system provides powerful tools for database schema management while maintaining backward compatibility with your existing code.