# IliasSql

<!-- CI Status Badges -->
[![Linux CI](https://github.com/Btk-Project/IliasMySql/actions/workflows/xmake-test-on-linux.yml/badge.svg)](https://github.com/Btk-Project/IliasMySql/actions/workflows/xmake-test-on-linux.yml)
[![Windows CI](https://github.com/Btk-Project/IliasMySql/actions/workflows/xmake-test-on-cl.yml/badge.svg)](https://github.com/Btk-Project/IliasMySql/actions/workflows/xmake-test-on-cl.yml)

<!-- Project Info Badges -->
[![License](https://img.shields.io/github/license/Btk-Project/IliasMySql)](LICENSE)
[![C++ Standard](https://img.shields.io/badge/C%2B%2B-20-blue.svg?logo=c%2B%2B)](https://en.cppreference.com/w/cpp/20)
[![Build System](https://img.shields.io/badge/build-xmake-green)](https://xmake.io)
[![Platform](https://img.shields.io/badge/platform-windows%20%7C%20linux-lightgrey)](https://github.com/Btk-Project/IliasMySql)

**IliasSql** 是一个基于 C++20 协程 (Coroutines) 的高性能异步 SQL 客户端库，作为 `Ilias` 框架的一部分。它提供了统一的接口来访问 SQLite 和 MySQL/MariaDB，支持非阻塞 I/O、类型安全的参数绑定、结构体反射映射以及流式结果集处理。

## 核心特性

*   **全异步接口**: 基于 `ilias::task` 和 C++20 `co_await`，在执行 SQL 查询时不会阻塞线程。
*   **多后端支持**:
    *   SQLite3 (包含内存数据库模式)
    *   MySQL / MariaDB (使用原生异步 C API)
*   **类型安全与反射**:
    *   支持将 C++ 结构体直接映射到 SQL 语句参数 (`prepare_with`)。
    *   支持将 SQL 结果集直接映射回 C++ 结构体 (`query<T>`)。
    *   编译期检查 SQL 占位符数量与结构体成员数量的一致性。
*   **流式结果集**: 使用 `ilias_for_await` 逐行异步获取数据，内存占用低。
*   **RAII 事务管理**: 自动回滚，手动提交。

## 依赖环境

*   C++20 编译器 (GCC, Clang, MSVC)
*   [Ilias](https://github.com/BusyStudent/Ilias) 框架 (提供 Task 系统和 IO 上下文)
*   [NekoProtoTools](https://github.com/liuli-neko/NekoProtoTools) (用于结构体反射)
*   SQLite3 / MariaDB Client Library
*   构建系统: Xmake (推荐)

## 快速开始

### 1. 引入头文件

```cpp
#include <ilias/platform.hpp>
#include <ilias/sql/sqldatabase.hpp>
#include <ilias/sql/types.hpp>

// 使用命名空间
using namespace ilias;
using namespace ilias::sql;
```

### 2. 定义数据模型

利用反射库，可以直接将结构体用于数据库操作。

```cpp
struct User {
    int64_t     id;
    std::string name;
    int         age;
    SqlDate     created_at; // 库提供的日期类型
};
```

### 3. 连接数据库

```cpp
IoTask<void> run_database_demo() {
    // 配置连接选项
    ConnectOptions options;
    options.host = "127.0.0.1";
    options.port = 3306;
    options.user = "root";
    options.password = "secret";
    options.database = "test_db";
    
    // 打开数据库 (工厂模式)
    // 对于 SQLite，使用 SqlDatabase::open("sqlite", options) 或 open_in_memory()
    auto db_result = co_await SqlDatabase::open("mysql", options);
    
    if (!db_result) {
        ILIAS_ERROR("DB", "连接失败: {}", db_result.error().message());
        co_return;
    }
    
    SqlDatabase db = std::move(db_result.value());
    
    // ... 后续操作
}
```

### 4. 插入数据 (结构体绑定)

支持直接传入结构体进行参数绑定。

```cpp
User new_user {0, "Alice", 25, SqlDate(std::chrono::system_clock::now())};

// prepare_with 会在编译期/运行期检查占位符数量
// 注意：SQL 中的 :id, :name 等名称主要用于标记，绑定顺序依赖结构体定义顺序

auto stmt_ret = co_await db.prepare_with("INSERT INTO users (id, name, age, created_at) VALUES (:id, :name, :age, :time)", new_user);
if (stmt_ret) {
    auto stmt = std::move(stmt_ret.value());
    // 执行
    auto rows = co_await stmt.execute(); 
    std::cout << "Inserted rows: " << rows.value() << std::endl;
}
```

### 5. 查询与流式遍历

使用 `query<T>` 将结果集自动映射为对象，并使用 `ilias_for_await` 进行异步迭代。

```cpp
// 查询年龄大于 18 的用户
auto query_ret = co_await db.query<User>("SELECT * FROM users WHERE age > 18");

if (query_ret) {
    auto result = std::move(query_ret.value());
    
    // 异步迭代器，仅在需要时获取下一行
    ilias_for_await(auto& user, result.range()) {
        std::cout << "User: " << user.name 
                  << ", Age: " << user.age 
                  << ", Date: " << user.created_at.toString() << std::endl;
    }
}
```

### 6. 事务处理

```cpp
auto tx_ret = co_await db.transaction();
if (tx_ret) {
    auto tx = std::move(tx_ret.value());
    
    // 在事务中执行操作
    co_await tx.execute("UPDATE users SET age = age + 1 WHERE id = 1");
    co_await tx.execute("DELETE FROM logs WHERE date < '2023-01-01'");
    
    // 提交事务
    bool success = co_await tx.commit();
    if (!success) {
        // commit 失败，逻辑上可能需要处理
    }
} 
// 如果 tx 析构时未 commit，自动执行 rollback
```

## API 概览

### `SqlDatabase`
数据库操作的主入口。
- `open(driver, options)`: 建立连接。
- `prepare(sql)` / `prepare_with(sql, args...)`: 创建预编译语句。
- `query(sql)` / `query<T>(sql)`: 直接查询。
- `execute(sql)`: 直接执行（不返回结果集）。
- `transaction()`: 开始一个事务。

### `SqlStatement<T>`
预编译语句对象，`T` 为绑定的参数类型（可选）。
- `bind(args...)`: 绑定参数（支持基本类型、结构体、Tuple）。
- `execute()`: 执行写操作，返回影响行数。
- `query()`: 执行读操作，返回 `SqlResult`。
- `reset()`: 重置状态以复用语句。

### `SqlResult<T>`
查询结果集，`T` 为映射的目标类型。
- `range()`: 返回一个 Generator，用于 `ilias_for_await` 循环。
- `next()`: 手动移动到下一行。
- `load(index/name, val)`: 手动读取列数据。

## 错误处理

IliasSql 使用 `Result<T, std::error_code>` 模式（类似 `std::expected`）。所有异步操作均需检查返回值是否包含错误。

```cpp
auto ret = co_await db.execute("INVALID SQL");
if (!ret) {
    std::cerr << "Error code: " << ret.error().value() << "\n"
              << "Message: " << ret.error().message() << std::endl;
}
```

## 构建测试

项目使用 Xmake 管理构建。

```bash
# 运行 SQLite 测试
xmake build test_sqlite
xmake run test_sqlite

# 运行 MySQL 测试 (需要本地有 MySQL 服务)
xmake build test_mysql
xmake run test_mysql
```

## 运行 MySQL 测试（本地与 CI 说明）

- 环境变量说明：测试使用以下环境变量来连接 MySQL（CI 通过 env 传入）：
    - `DB_HOST` (默认 `127.0.0.1`)
    - `DB_PORT` (默认 `3306`)
    - `DB_USER` (默认 `root`)
    - `DB_PASS` (默认 `root`)
    - `DB_NAME` (默认 `test_db`)

- 本地运行推荐使用 Docker 启动一个 MySQL 实例：

```bash
docker run --name ilias-test-mysql -e MYSQL_ROOT_PASSWORD=root -e MYSQL_DATABASE=test_db -p 3306:3306 -d mysql:8.0
# 等待几秒后运行测试
 xmake build test_mysql
 xmake run test_mysql
```

- Windows CI 常见问题：
    - Windows 上的 MySQL 服务器有时默认使用 Named Pipe 或者 localhost 解析为 IPv6，导致 TCP 客户端无法连接。仓库的 Windows workflow 已调整为使用 `127.0.0.1` 并在创建数据库前循环检测 `mysqladmin ping`，同时会创建并授权 `root@127.0.0.1`，以提高 CI 稳定性。
    - 如果在本地 Windows 上构建失败，确保已安装 MariaDB/MySQL 的 C client（或相应的开发包），以便链接运行时库。

## TODO
*   [ ] 支持更多数据库驱动
    - 目前仅支持 SQLite 和 MySQL（mariaDB）
    - 允许绑定到c++对象并使用loop执行
*   [ ] 完善loop逻辑，允许组合loop和事务。
    - loop中允许使用事务时需要保持变量到事务提交，如果绑定到变量并持续更改会有问题，需要考虑设计。
*   [ ] 支持更多 SQL 语句
