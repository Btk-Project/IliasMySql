# SqlTags API Reference

Complete reference documentation for all SqlTags features and APIs.

## Table of Contents

1. [SqlTags Structure](#sqltags-structure)
2. [Constraint Fields](#constraint-fields)
3. [Type Modifier Fields](#type-modifier-fields)
4. [Timestamp Fields](#timestamp-fields)
5. [Validation Methods](#validation-methods)
6. [Helper Methods](#helper-methods)
7. [Schema Generation API](#schema-generation-api)
8. [Timestamp Management API](#timestamp-management-api)
9. [Dialect System API](#dialect-system-api)
10. [Error Handling](#error-handling)

## SqlTags Structure

### Definition

```cpp
struct SqlTags {
    // Core constraints
    bool primary_key    = false;
    bool not_null       = false;
    bool unique         = false;
    bool auto_increment = false;
    bool index          = false;
    
    // Data type modifiers
    bool unsigned_type = false;
    int  length        = 0;
    
    // Timestamp automation
    bool created_at = false;
    bool updated_at = false;
    
    // Methods
    bool isValid() const;
    std::vector<std::string> getValidationErrors() const;
    bool hasTimestampBehavior() const;
    bool requiresIndex() const;
};
```

### Usage

```cpp
// Aggregate initialization
SqlTags tags{
    .primary_key = true,
    .auto_increment = true,
    .not_null = true
};

// In reflection metadata
"field_name", make_tags<SqlTags{.not_null = true, .length = 100}>(&Struct::field)
```

## Constraint Fields

### primary_key

**Type:** `bool`  
**Default:** `false`

Marks the field as the primary key of the table.

**Behavior:**
- Automatically implies `unique = true` and `not_null = true`
- Creates a PRIMARY KEY constraint in the database
- Only one field per table should have this set to `true`

**Generated SQL:**
- SQLite: `PRIMARY KEY`
- MySQL: `PRIMARY KEY`
- PostgreSQL: `PRIMARY KEY`

**Example:**
```cpp
SqlTags{.primary_key = true}
// Generates: "id INTEGER PRIMARY KEY"
```

**Validation Rules:**
- Cannot be combined with `unique = false` (primary key implies unique)
- Should be used on exactly one field per table

### not_null

**Type:** `bool`  
**Default:** `false`

Prevents the field from accepting NULL values.

**Behavior:**
- Creates a NOT NULL constraint in the database
- Automatically set to `true` when `primary_key = true`

**Generated SQL:**
- All databases: `NOT NULL`

**Example:**
```cpp
SqlTags{.not_null = true}
// Generates: "name TEXT NOT NULL"
```

### unique

**Type:** `bool`  
**Default:** `false`

Ensures all values in the field are unique across the table.

**Behavior:**
- Creates a UNIQUE constraint in the database
- Automatically creates an index for the field
- Automatically set to `true` when `primary_key = true`

**Generated SQL:**
- SQLite: `UNIQUE`
- MySQL: `UNIQUE KEY`
- PostgreSQL: `UNIQUE`

**Example:**
```cpp
SqlTags{.unique = true}
// Generates: "email TEXT UNIQUE"
```

### auto_increment

**Type:** `bool`  
**Default:** `false`

Automatically generates sequential values for the field.

**Behavior:**
- Must be used with numeric types only
- Typically combined with `primary_key = true`
- Database automatically assigns values during INSERT

**Generated SQL:**
- SQLite: `AUTOINCREMENT` (when combined with PRIMARY KEY)
- MySQL: `AUTO_INCREMENT`
- PostgreSQL: Uses `SERIAL` or `BIGSERIAL` type

**Example:**
```cpp
SqlTags{.primary_key = true, .auto_increment = true}
// SQLite: "id INTEGER PRIMARY KEY AUTOINCREMENT"
// MySQL: "id BIGINT AUTO_INCREMENT PRIMARY KEY"
// PostgreSQL: "id BIGSERIAL PRIMARY KEY"
```

**Validation Rules:**
- Can only be used with integral types
- Generates validation error if used with string or other non-numeric types

### index

**Type:** `bool`  
**Default:** `false`

Creates an index on the field for faster queries.

**Behavior:**
- Creates a separate CREATE INDEX statement
- Not needed for `primary_key` or `unique` fields (they automatically get indexes)
- Improves SELECT performance but may slow INSERT/UPDATE operations

**Generated SQL:**
- All databases: `CREATE INDEX idx_table_field ON table (field)`

**Example:**
```cpp
SqlTags{.index = true}
// Generates additional statement: "CREATE INDEX idx_users_name ON users (name)"
```

## Type Modifier Fields

### unsigned_type

**Type:** `bool`  
**Default:** `false`

For numeric types, generates unsigned variants.

**Behavior:**
- Only affects integral types
- Ignored for non-numeric types without error
- Database support varies

**Generated SQL:**
- SQLite: No effect (SQLite doesn't distinguish signed/unsigned)
- MySQL: Adds `UNSIGNED` modifier
- PostgreSQL: No effect (PostgreSQL doesn't have unsigned types)

**Example:**
```cpp
// For int32_t field
SqlTags{.unsigned_type = true}
// MySQL: "quantity INT UNSIGNED"
// SQLite: "quantity INTEGER" (no change)
// PostgreSQL: "quantity INTEGER" (no change)
```

### length

**Type:** `int`  
**Default:** `0`

Specifies the maximum length for string fields.

**Behavior:**
- `length > 0`: Uses `VARCHAR(length)`
- `length = 0` with indexing needs: Uses `VARCHAR(255)` (default)
- `length = 0` without indexing: Uses `TEXT` or equivalent

**Generated SQL:**
- `length > 0`: `VARCHAR(length)`
- `length = 0` + indexing: `VARCHAR(255)`
- `length = 0` + no indexing: `TEXT`

**Example:**
```cpp
SqlTags{.length = 100}
// Generates: "name VARCHAR(100)"

SqlTags{.length = 0, .index = true}
// Generates: "description VARCHAR(255)" (default for indexable)

SqlTags{.length = 0}
// Generates: "content TEXT" (unlimited length)
```

**Validation Rules:**
- Must be non-negative
- Negative values generate validation errors

## Timestamp Fields

### created_at

**Type:** `bool`  
**Default:** `false`

Automatically sets the field to current timestamp when records are inserted.

**Behavior:**
- Sets database default to `CURRENT_TIMESTAMP`
- Application-level timestamp population during INSERT operations
- Manual values override automatic behavior

**Generated SQL:**
- All databases: `DEFAULT CURRENT_TIMESTAMP`

**Runtime Behavior:**
- Automatically populated before INSERT operations
- Manual values in the struct override automatic population
- Only affects INSERT, not UPDATE operations

**Example:**
```cpp
SqlTags{.created_at = true}
// Generates: "created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
// Runtime: Automatically set during INSERT
```

### updated_at

**Type:** `bool`  
**Default:** `false`

Automatically updates the field to current timestamp when records are modified.

**Behavior:**
- Database-specific implementation
- Application-level timestamp population during UPDATE operations
- Manual values override automatic behavior

**Generated SQL:**
- MySQL: `ON UPDATE CURRENT_TIMESTAMP`
- SQLite/PostgreSQL: Handled at application level

**Runtime Behavior:**
- Automatically updated before UPDATE operations
- Manual values in the struct override automatic population
- Also updated during INSERT if `created_at = false`

**Example:**
```cpp
SqlTags{.updated_at = true}
// MySQL: "updated_at DATETIME ON UPDATE CURRENT_TIMESTAMP"
// SQLite: "updated_at TEXT" (application handles updates)
// Runtime: Automatically updated during UPDATE
```

### Combined Timestamp Behavior

When both `created_at = true` and `updated_at = true`:

**Behavior:**
- Field behaves as `updated_at` (updated on both INSERT and UPDATE)
- Useful for a single "last_modified" field

**Example:**
```cpp
SqlTags{.created_at = true, .updated_at = true}
// Behavior: Updated on both INSERT and UPDATE operations
```

## Validation Methods

### isValid()

```cpp
bool isValid() const;
```

**Purpose:** Checks if the SqlTags configuration is valid.

**Returns:** `true` if configuration is valid, `false` otherwise.

**Example:**
```cpp
SqlTags tags{.primary_key = true, .length = -1};
if (!tags.isValid()) {
    // Handle invalid configuration
}
```

### getValidationErrors()

```cpp
std::vector<std::string> getValidationErrors() const;
```

**Purpose:** Returns detailed validation error messages.

**Returns:** Vector of error message strings. Empty if configuration is valid.

**Example:**
```cpp
SqlTags tags{.auto_increment = true};  // On string field
auto errors = tags.getValidationErrors();
for (const auto& error : errors) {
    std::cout << "Error: " << error << std::endl;
}
// Output: "Error: auto_increment can only be used with numeric types"
```

**Common Error Messages:**
- `"primary_key implies unique, cannot set unique = false"`
- `"length cannot be negative"`
- `"auto_increment can only be used with numeric types"`
- `"timestamp fields require datetime-compatible types"`

## Helper Methods

### hasTimestampBehavior()

```cpp
bool hasTimestampBehavior() const;
```

**Purpose:** Checks if the field has any automatic timestamp behavior.

**Returns:** `true` if `created_at = true` or `updated_at = true`.

**Example:**
```cpp
SqlTags tags{.created_at = true};
if (tags.hasTimestampBehavior()) {
    // Field will be automatically managed
}
```

### requiresIndex()

```cpp
bool requiresIndex() const;
```

**Purpose:** Checks if the field requires an index to be created.

**Returns:** `true` if `primary_key = true`, `unique = true`, or `index = true`.

**Example:**
```cpp
SqlTags tags{.unique = true};
if (tags.requiresIndex()) {
    // Index will be created for this field
}
```

## Schema Generation API

### SchemaGenerator Class Template

```cpp
template<typename BackendTag>
class SchemaGenerator {
public:
    template<typename T>
    static std::string generateColumnDefinition(std::string_view columnName, const SqlTags& tags);
    
    template<typename FormType>
    static std::string generateCreateTable(std::string_view tableName);
    
    static std::vector<std::string> generateIndexStatements(
        std::string_view tableName,
        const std::vector<std::pair<std::string, SqlTags>>& columns
    );
};
```

### generateColumnDefinition()

```cpp
template<typename T>
static std::string generateColumnDefinition(std::string_view columnName, const SqlTags& tags);
```

**Purpose:** Generates SQL column definition for a single field.

**Parameters:**
- `T`: C++ type of the field
- `columnName`: Name of the database column
- `tags`: SqlTags configuration

**Returns:** SQL column definition string.

**Example:**
```cpp
auto columnDef = SchemaGenerator<MysqlTag>::generateColumnDefinition<std::string>(
    "name", SqlTags{.not_null = true, .length = 100}
);
// Result: "`name` VARCHAR(100) NOT NULL"
```

### generateCreateTable()

```cpp
template<typename FormType>
static std::string generateCreateTable(std::string_view tableName);
```

**Purpose:** Generates complete CREATE TABLE statement for a struct.

**Parameters:**
- `FormType`: C++ struct with reflection metadata
- `tableName`: Name of the database table

**Returns:** Complete CREATE TABLE SQL statement.

**Example:**
```cpp
auto createSQL = SchemaGenerator<SqliteTag>::generateCreateTable<User>("users");
// Result: "CREATE TABLE users (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL, ...)"
```

### generateIndexStatements()

```cpp
static std::vector<std::string> generateIndexStatements(
    std::string_view tableName,
    const std::vector<std::pair<std::string, SqlTags>>& columns
);
```

**Purpose:** Generates CREATE INDEX statements for fields marked with `index = true`.

**Parameters:**
- `tableName`: Name of the database table
- `columns`: Vector of column name and SqlTags pairs

**Returns:** Vector of CREATE INDEX SQL statements.

**Example:**
```cpp
std::vector<std::pair<std::string, SqlTags>> columns = {
    {"name", SqlTags{.index = true}},
    {"email", SqlTags{.unique = true}}  // Won't generate index (unique creates its own)
};
auto indexes = SchemaGenerator<PostgresTag>::generateIndexStatements("users", columns);
// Result: ["CREATE INDEX \"idx_users_name\" ON \"users\" (\"name\")"]
```

## Timestamp Management API

### TimestampManager Class

```cpp
class TimestampManager {
public:
    template<typename T>
    static void applyCreatedAt(T& object, const std::vector<std::string>& createdAtFields);
    
    template<typename T>
    static void applyUpdatedAt(T& object, const std::vector<std::string>& updatedAtFields);
    
    template<typename T>
    static std::vector<std::string> getTimestampFields(bool createdAt, bool updatedAt);
};
```

### applyCreatedAt()

```cpp
template<typename T>
static void applyCreatedAt(T& object, const std::vector<std::string>& createdAtFields);
```

**Purpose:** Applies current timestamp to fields marked with `created_at = true`.

**Parameters:**
- `object`: Struct instance to modify
- `createdAtFields`: List of field names with created_at behavior

**Behavior:**
- Sets fields to current timestamp if they are not already set
- Respects manual values (doesn't override non-default values)

### applyUpdatedAt()

```cpp
template<typename T>
static void applyUpdatedAt(T& object, const std::vector<std::string>& updatedAtFields);
```

**Purpose:** Applies current timestamp to fields marked with `updated_at = true`.

**Parameters:**
- `object`: Struct instance to modify
- `updatedAtFields`: List of field names with updated_at behavior

**Behavior:**
- Always sets fields to current timestamp
- Overrides any existing values

### getTimestampFields()

```cpp
template<typename T>
static std::vector<std::string> getTimestampFields(bool createdAt, bool updatedAt);
```

**Purpose:** Extracts field names with timestamp behavior from struct metadata.

**Parameters:**
- `createdAt`: Include fields with `created_at = true`
- `updatedAt`: Include fields with `updated_at = true`

**Returns:** Vector of field names with the specified timestamp behavior.

## Dialect System API

### Dialect Class Template

```cpp
template<typename BackendTag>
struct Dialect {
    static bool check(std::string_view name);
    
    template<typename T>
    static constexpr std::string type_name(const SqlTags& tags);
    
    template<typename T>
    static std::string generate_column_definition(std::string_view name, const SqlTags& tags);
    
    static std::vector<std::string> generate_index_statements(
        std::string_view table_name,
        const std::vector<std::pair<std::string, SqlTags>>& columns
    );
};
```

### Backend Tags

#### SqliteTag

```cpp
struct SqliteTag {};
```

**Characteristics:**
- Simple type system (INTEGER, REAL, TEXT, BLOB)
- No unsigned types
- AUTOINCREMENT only with PRIMARY KEY
- No ON UPDATE CURRENT_TIMESTAMP support

#### MysqlTag

```cpp
struct MysqlTag {};
```

**Characteristics:**
- Rich type system with size specifiers
- Supports UNSIGNED modifier
- Full AUTO_INCREMENT support
- ON UPDATE CURRENT_TIMESTAMP support
- Uses backticks for identifiers

#### PostgresTag

```cpp
struct PostgresTag {};
```

**Characteristics:**
- Precise type system (SMALLINT, INTEGER, BIGINT, etc.)
- Uses SERIAL/BIGSERIAL for auto increment
- No unsigned types
- Uses double quotes for identifiers
- No ON UPDATE CURRENT_TIMESTAMP support

### check()

```cpp
static bool check(std::string_view name);
```

**Purpose:** Checks if a database driver name matches this dialect.

**Parameters:**
- `name`: Database driver name (case-insensitive)

**Returns:** `true` if the name matches this dialect.

**Example:**
```cpp
bool isMysql = Dialect<MysqlTag>::check("mysql");     // true
bool isMariaDB = Dialect<MysqlTag>::check("mariadb"); // true
bool isSqlite = Dialect<SqliteTag>::check("sqlite");  // true
```

### type_name()

```cpp
template<typename T>
static constexpr std::string type_name(const SqlTags& tags);
```

**Purpose:** Maps C++ types to database-specific SQL types.

**Parameters:**
- `T`: C++ type
- `tags`: SqlTags configuration (affects type selection)

**Returns:** Database-specific type name.

**Example:**
```cpp
// For std::string with length = 100
auto sqliteType = Dialect<SqliteTag>::type_name<std::string>(SqlTags{.length = 100});
// Result: "TEXT"

auto mysqlType = Dialect<MysqlTag>::type_name<std::string>(SqlTags{.length = 100});
// Result: "VARCHAR(100)"
```

## Error Handling

### Validation Errors

SqlTags validation catches common configuration errors:

#### Constraint Conflicts

```cpp
SqlTags{.primary_key = true, .unique = false}
// Error: "primary_key implies unique, cannot set unique = false"
```

#### Type Mismatches

```cpp
// For std::string field:
SqlTags{.auto_increment = true}
// Error: "auto_increment can only be used with numeric types"
```

#### Invalid Values

```cpp
SqlTags{.length = -5}
// Error: "length cannot be negative"
```

### Runtime Errors

Runtime errors are reported through the standard IliasSql error system:

```cpp
auto result = co_await db.execute(invalidSQL);
if (!result) {
    std::error_code ec = result.error();
    if (ec.category() == sql_error_category()) {
        // SQL-related error, possibly from SqlTags constraint violation
    }
}
```

### Error Categories

#### Configuration Errors

Caught at compile-time or during validation:
- Invalid constraint combinations
- Type mismatches
- Invalid parameter values

#### Schema Generation Errors

Caught during schema generation:
- Unsupported database features
- Invalid SQL syntax
- Missing dependencies

#### Runtime Errors

Caught during database operations:
- Constraint violations
- Type conversion errors
- Database-specific limitations

## Best Practices

### 1. Always Validate

```cpp
SqlTags tags{/* configuration */};
assert(tags.isValid());
```

### 2. Use Appropriate Types

```cpp
// Good: Specific constraints
SqlTags{.primary_key = true, .auto_increment = true}

// Avoid: Redundant constraints
SqlTags{.primary_key = true, .unique = true, .not_null = true}  // unique and not_null are implied
```

### 3. Specify String Lengths

```cpp
// Good: Explicit length for indexed strings
SqlTags{.unique = true, .length = 255}

// Acceptable: Let system choose default
SqlTags{.unique = true}  // Uses VARCHAR(255)
```

### 4. Use Timestamp Fields Appropriately

```cpp
// Good: Separate fields for different purposes
"created_at", make_tags<SqlTags{.created_at = true}>(&Model::created_at),
"updated_at", make_tags<SqlTags{.updated_at = true}>(&Model::updated_at),
```

This API reference covers all aspects of the SqlTags system. For practical examples and usage patterns, see the [SqlTags Guide](sql-tags-guide.md) and [Examples](examples/).