# SqlTags Examples

This directory contains practical examples demonstrating various SqlTags usage patterns and real-world scenarios.

## Example Categories

### Basic Usage
- [`basic-constraints.cpp`](basic-constraints.cpp) - Primary keys, not null, unique constraints
- [`data-types.cpp`](data-types.cpp) - Type modifiers and string lengths
- [`timestamps.cpp`](timestamps.cpp) - Automatic timestamp management

### Real-World Models
- [`user-management.cpp`](user-management.cpp) - Complete user management system
- [`e-commerce.cpp`](e-commerce.cpp) - Product catalog and order management
- [`blog-system.cpp`](blog-system.cpp) - Blog posts, comments, and categories
- [`audit-logging.cpp`](audit-logging.cpp) - Audit trail and logging system

### Advanced Patterns
- [`multi-database.cpp`](multi-database.cpp) - Cross-database compatibility
- [`schema-generation.cpp`](schema-generation.cpp) - Dynamic schema generation
- [`validation.cpp`](validation.cpp) - Configuration validation and error handling
- [`migration.cpp`](migration.cpp) - Schema migration patterns

### Integration Examples
- [`orm-integration.cpp`](orm-integration.cpp) - Integration with ORM builders
- [`transaction-patterns.cpp`](transaction-patterns.cpp) - Transaction management with SqlTags
- [`performance-optimization.cpp`](performance-optimization.cpp) - Performance-focused configurations

## Running Examples

Each example is a complete, compilable C++ file that demonstrates specific SqlTags features. To use them:

1. Include the necessary headers
2. Copy the struct definitions and metadata
3. Adapt the database connection code for your setup
4. Run the example code

## Example Structure

Each example follows this structure:

```cpp
// 1. Headers and includes
#include <ilias/sql_orm/detail/orm_types.hpp>
// ... other includes

// 2. Struct definitions
struct ExampleModel {
    // Field definitions
};

// 3. Reflection metadata with SqlTags
NEKO_BEGIN_NAMESPACE
template<> struct Meta<ExampleModel, void> {
    constexpr static auto value = Object(
        // SqlTags configurations
    );
};
NEKO_END_NAMESPACE

// 4. Usage demonstration
IoTask<void> example_usage() {
    // Database operations
}
```

## Key Learning Points

- **Constraint Combinations**: How different SqlTags work together
- **Database Differences**: How the same SqlTags generate different SQL for different databases
- **Runtime Behavior**: How SqlTags affect INSERT/UPDATE operations
- **Validation**: How to catch configuration errors early
- **Performance**: How to optimize database schemas with SqlTags

Start with the basic examples and progress to more complex scenarios as you become familiar with the SqlTags system.