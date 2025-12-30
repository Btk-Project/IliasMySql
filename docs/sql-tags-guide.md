# SqlTags Complete Guide

## Overview

SqlTags is a powerful system for defining database schema attributes directly in your C++ code. It provides a declarative way to specify constraints, data types, and behaviors that are automatically translated into appropriate SQL for different database systems.

## Table of Contents

1. [Basic Concepts](#basic-concepts)
2. [Core Constraints](#core-constraints)
3. [Data Type Modifiers](#data-type-modifiers)
4. [Automatic Timestamp Management](#automatic-timestamp-management)
5. [Cross-Database Compatibility](#cross-database-compatibility)
6. [Validation and Error Handling](#validation-and-error-handling)
7. [Advanced Usage](#advanced-usage)

## Basic Concepts

SqlTags are defined as part of your struct's reflection metadata using the `make_tags` function:

```cpp
#include <ilias/sql_orm/detail/orm_types.hpp>

struct User {
    int64_t id;
    std::string name;
    std::optional<int> age;
    SqlDate created_at;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<User, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true, .auto_increment = true}>(&User::id),
        "name", make_tags<SqlTags{.not_null = true, .length = 100}>(&User::name),
        "age", &User::age,  // No tags = default behavior
        "created_at", make_tags<SqlTags{.created_at = true}>(&User::created_at)
    );
};
NEKO_END_NAMESPACE
```

## Core Constraints

### Primary Key

```cpp
SqlTags{.primary_key = true}
```

Marks a field as the primary key. Automatically implies `unique = true` and `not_null = true`.

**Generated SQL:**
- SQLite: `PRIMARY KEY`
- MySQL: `PRIMARY KEY`
- PostgreSQL: `PRIMARY KEY`

### Not Null

```cpp
SqlTags{.not_null = true}
```

Ensures the field cannot contain NULL values.

**Generated SQL:**
- All databases: `NOT NULL`

### Unique Constraint

```cpp
SqlTags{.unique = true}
```

Ensures all values in the field are unique across the table.

**Generated SQL:**
- SQLite: `UNIQUE`
- MySQL: `UNIQUE KEY`
- PostgreSQL: `UNIQUE`

### Auto Increment

```cpp
SqlTags{.auto_increment = true}
```

Automatically generates sequential values for the field. Must be used with numeric types.

**Generated SQL:**
- SQLite: `AUTOINCREMENT` (when combined with PRIMARY KEY)
- MySQL: `AUTO_INCREMENT`
- PostgreSQL: Uses `SERIAL` or `BIGSERIAL` type

### Index

```cpp
SqlTags{.index = true}
```

Creates an index on the field for faster queries.

**Generated SQL:**
- All databases: `CREATE INDEX idx_table_column ON table (column)`

## Data Type Modifiers

### Unsigned Types

```cpp
SqlTags{.unsigned_type = true}
```

For numeric types, generates unsigned variants. Ignored for non-numeric types.

**Generated SQL:**
- SQLite: No effect (SQLite doesn't have unsigned types)
- MySQL: `UNSIGNED` modifier
- PostgreSQL: No effect (PostgreSQL doesn't have unsigned types)

### String Length

```cpp
SqlTags{.length = 255}
```

Specifies the maximum length for string fields.

**Generated SQL:**
- `length > 0`: `VARCHAR(length)`
- `length = 0` with indexing: `VARCHAR(255)` (default for indexable strings)
- `length = 0` without indexing: `TEXT`

## Automatic Timestamp Management

### Created At

```cpp
SqlTags{.created_at = true}
```

Automatically sets the field to the current timestamp when a record is inserted.

**Behavior:**
- Sets default value to `CURRENT_TIMESTAMP`
- Automatically populated during INSERT operations
- Manual values override automatic behavior

### Updated At

```cpp
SqlTags{.updated_at = true}
```

Automatically updates the field to the current timestamp when a record is modified.

**Behavior:**
- MySQL: Uses `ON UPDATE CURRENT_TIMESTAMP`
- SQLite/PostgreSQL: Handled at application level
- Automatically updated during UPDATE operations

### Combined Behavior

```cpp
SqlTags{.created_at = true, .updated_at = true}
```

When both flags are set, the field behaves as `updated_at` (updated on both INSERT and UPDATE).

## Cross-Database Compatibility

SqlTags automatically generates appropriate SQL for different database systems:

### Type Mapping Examples

| C++ Type | SQLite | MySQL | PostgreSQL |
|----------|--------|-------|------------|
| `bool` | `INTEGER` | `TINYINT(1)` | `BOOLEAN` |
| `int32_t` | `INTEGER` | `INT` | `INTEGER` |
| `int64_t` | `INTEGER` | `BIGINT` | `BIGINT` |
| `float` | `REAL` | `FLOAT` | `REAL` |
| `double` | `REAL` | `DOUBLE` | `DOUBLE PRECISION` |
| `std::string` | `TEXT` | `VARCHAR(n)/TEXT` | `VARCHAR(n)/TEXT` |
| `SqlDate` | `TEXT` | `DATETIME` | `TIMESTAMP` |
| `SqlBlob` | `BLOB` | `BLOB` | `BYTEA` |

### Database-Specific Features

Some features are handled differently across databases:

- **Auto Increment**: PostgreSQL uses `SERIAL` types instead of `AUTO_INCREMENT`
- **Unsigned Types**: Only MySQL supports unsigned modifiers
- **Timestamp Updates**: MySQL supports `ON UPDATE CURRENT_TIMESTAMP`, others use application-level handling

## Validation and Error Handling

SqlTags includes comprehensive validation to catch configuration errors early:

### Constraint Conflicts

```cpp
// This will generate a validation error:
SqlTags{.primary_key = true, .unique = false}  // Primary key implies unique
```

### Type Mismatches

```cpp
// This will generate a validation error:
SqlTags{.auto_increment = true}  // On a string field
```

### Invalid Values

```cpp
// This will generate a validation error:
SqlTags{.length = -5}  // Negative length
```

### Checking Validation

```cpp
SqlTags tags{.primary_key = true, .length = -1};

if (!tags.isValid()) {
    auto errors = tags.getValidationErrors();
    for (const auto& error : errors) {
        std::cout << "Validation error: " << error << std::endl;
    }
}
```

## Advanced Usage

### Complex Constraint Combinations

```cpp
struct Product {
    int64_t id;
    std::string sku;
    std::string name;
    double price;
    int stock_quantity;
    SqlDate created_at;
    SqlDate updated_at;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<Product, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{
            .primary_key = true,
            .auto_increment = true
        }>(&Product::id),
        
        "sku", make_tags<SqlTags{
            .not_null = true,
            .unique = true,
            .index = true,
            .length = 50
        }>(&Product::sku),
        
        "name", make_tags<SqlTags{
            .not_null = true,
            .index = true,
            .length = 200
        }>(&Product::name),
        
        "price", make_tags<SqlTags{
            .not_null = true,
            .unsigned_type = true
        }>(&Product::price),
        
        "stock_quantity", make_tags<SqlTags{
            .not_null = true,
            .unsigned_type = true
        }>(&Product::stock_quantity),
        
        "created_at", make_tags<SqlTags{
            .not_null = true,
            .created_at = true
        }>(&Product::created_at),
        
        "updated_at", make_tags<SqlTags{
            .not_null = true,
            .updated_at = true
        }>(&Product::updated_at)
    );
};
NEKO_END_NAMESPACE
```

### Helper Methods

SqlTags provides several helper methods for common operations:

```cpp
SqlTags tags{.created_at = true, .updated_at = true};

// Check if field has timestamp behavior
if (tags.hasTimestampBehavior()) {
    std::cout << "Field has automatic timestamp management" << std::endl;
}

// Check if field requires an index
if (tags.requiresIndex()) {
    std::cout << "Field will have an index created" << std::endl;
}
```

### Schema Generation Integration

SqlTags integrates with the schema generation system:

```cpp
#include <ilias/sql_orm/detail/schema_generator.hpp>

// Generate column definition
auto columnDef = SchemaGenerator<SqliteTag>::generateColumnDefinition<std::string>(
    "name", SqlTags{.not_null = true, .length = 100}
);
// Result: "name TEXT NOT NULL"

// Generate complete table schema
auto createTableSQL = SchemaGenerator<MysqlTag>::generateCreateTable<Product>("products");
// Result: Complete CREATE TABLE statement with all constraints

// Generate index statements
std::vector<std::pair<std::string, SqlTags>> columns = {
    {"name", SqlTags{.index = true}},
    {"sku", SqlTags{.unique = true}}
};
auto indexStatements = SchemaGenerator<PostgresTag>::generateIndexStatements("products", columns);
```

## Best Practices

### 1. Use Appropriate Constraints

```cpp
// Good: Logical constraint combination
SqlTags{.primary_key = true, .auto_increment = true}

// Avoid: Redundant constraints (primary_key implies unique and not_null)
SqlTags{.primary_key = true, .unique = true, .not_null = true}
```

### 2. Specify String Lengths

```cpp
// Good: Explicit length for indexed strings
SqlTags{.not_null = true, .unique = true, .length = 255}

// Acceptable: Let system choose default for indexed strings
SqlTags{.not_null = true, .unique = true}  // Will use VARCHAR(255)

// Good: Use TEXT for large content
SqlTags{.not_null = true}  // Will use TEXT for non-indexed strings
```

### 3. Use Timestamp Fields Appropriately

```cpp
// Good: Separate created_at and updated_at fields
"created_at", make_tags<SqlTags{.created_at = true}>(&Model::created_at),
"updated_at", make_tags<SqlTags{.updated_at = true}>(&Model::updated_at),

// Acceptable: Single field for both behaviors
"modified_at", make_tags<SqlTags{.created_at = true, .updated_at = true}>(&Model::modified_at),
```

### 4. Validate Configurations

```cpp
// Always validate complex configurations
SqlTags complexTags{
    .primary_key = true,
    .auto_increment = true,
    .length = 255
};

assert(complexTags.isValid());
```

## Common Patterns

### User Management Table

```cpp
struct User {
    int64_t id;
    std::string username;
    std::string email;
    std::string password_hash;
    bool is_active;
    SqlDate created_at;
    SqlDate last_login;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<User, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true, .auto_increment = true}>(&User::id),
        "username", make_tags<SqlTags{.not_null = true, .unique = true, .length = 50}>(&User::username),
        "email", make_tags<SqlTags{.not_null = true, .unique = true, .length = 255}>(&User::email),
        "password_hash", make_tags<SqlTags{.not_null = true, .length = 255}>(&User::password_hash),
        "is_active", make_tags<SqlTags{.not_null = true}>(&User::is_active),
        "created_at", make_tags<SqlTags{.not_null = true, .created_at = true}>(&User::created_at),
        "last_login", &User::last_login  // Nullable, no automatic timestamp
    );
};
NEKO_END_NAMESPACE
```

### Audit Log Table

```cpp
struct AuditLog {
    int64_t id;
    int64_t user_id;
    std::string action;
    std::string table_name;
    std::string old_values;
    std::string new_values;
    SqlDate timestamp;
};

NEKO_BEGIN_NAMESPACE
template<> struct Meta<AuditLog, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags{.primary_key = true, .auto_increment = true}>(&AuditLog::id),
        "user_id", make_tags<SqlTags{.not_null = true, .index = true}>(&AuditLog::user_id),
        "action", make_tags<SqlTags{.not_null = true, .index = true, .length = 50}>(&AuditLog::action),
        "table_name", make_tags<SqlTags{.not_null = true, .index = true, .length = 100}>(&AuditLog::table_name),
        "old_values", &AuditLog::old_values,  // TEXT field, nullable
        "new_values", &AuditLog::new_values,  // TEXT field, nullable
        "timestamp", make_tags<SqlTags{.not_null = true, .created_at = true}>(&AuditLog::timestamp)
    );
};
NEKO_END_NAMESPACE
```

This guide covers the essential aspects of using SqlTags. For complete API documentation, see the [API Reference](api-reference.md).