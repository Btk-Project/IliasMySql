#include <gtest/gtest.h>
#include <sqlite3.h>

#include <ilias/platform.hpp>
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/interfaces.hpp"

#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;

struct Person {
    int                    id;
    std::string            name;
    int                    age;
    std::string            email;
    SqlDate                born;
    std::vector<std::byte> promise;
    char                   val1;
    int                    val2;
};

ILIAS_NAMESPACE::Task<void> test() {
    // --- A. 连接数据库 (Connect) ---
    // SQLite 不需要 host/port，直接打开文件，":memory:" 表示内存数据库，类似原测试中的临时环境
    ConnectOptions options;
    options.filename = ":memory:";
    auto &driver     = DriverManager::instance();
    auto  ret        = driver.createConnection("sqlite", options);
    EXPECT_TRUE(ret.has_value()) << "Can't open database: " << ret.error().message();
    if (!ret.has_value()) {
        co_return;
    }
    auto mysql_connection = std::move(ret.value());
    auto connect_ret      = co_await mysql_connection->connect();
    EXPECT_TRUE(connect_ret.has_value()) << "Can't open database: " << connect_ret.error().message();
    std::cout << "[INFO] Opened database successfully" << std::endl;
    // --- B. 建表 (Create Table) ---
    // SQLite 类型映射: INT -> INTEGER, VARCHAR -> TEXT, DATETIME -> TEXT, BLOB -> BLOB
    const char *create_sql = "CREATE TABLE IF NOT EXISTS test_table ("
                             "id INTEGER PRIMARY KEY, "
                             "name TEXT NOT NULL, "
                             "age INTEGER, "
                             "born TEXT, " // 存 ISO8601 字符串
                             "email TEXT UNIQUE, "
                             "promise BLOB, "
                             "val1 INTEGER, " // SQLite 没有 TINYINT，用 INTEGER
                             "val2 INTEGER"
                             ");";
    auto ret1 = co_await mysql_connection->execute(create_sql);
    EXPECT_TRUE(ret1);
    if (!ret1) {
        std::cout << "[INFO] error: " << ret1.error().message();
        co_return;
    }
    std::cout << "[INFO] create table test_table success" << std::endl;

    // --- C. 准备数据 ---
    std::vector<Person> persons = {
        {1,
         "a test user",
         18,
         "test@test.com",
         SqlDate(2025, 6, 20, 1, 1, 1),
         {std::byte {1}, std::byte {2}, std::byte {0}},
         1,
         234},
        {2,
         "王小明",
         19,
         "xiaoming@test.com",
         SqlDate(2025, 4, 21, 15, 23, 13),
         {std::byte {1}, std::byte {0}, std::byte {3}},
         0,
         145},
        {3,
         "Alice",
         18,
         "Alice@test.com",
         SqlDate(2025, 2, 22, 12, 23, 23),
         {std::byte {1}, std::byte {2}, std::byte {5}},
         3,
         345},
        {4,
         "Bob",
         18,
         "Bob@test.com",
         SqlDate(2025, 1, 20, 23, 12, 32),
         {std::byte {3}, std::byte {2}, std::byte {3}},
         123,
         434},
    };

    // --- D. 插入数据 (Prepare & Bind) ---
    const char *insert_sql = "INSERT INTO test_table (id, name, age, born, email, promise, val1, val2) "
                             "VALUES (:id, :name, :age, :born, :email, :promise, :val1, :val2)";

    auto ret2 = co_await mysql_connection->prepare(insert_sql);

    EXPECT_TRUE(ret2);
    auto stmt = std::move(ret2.value());
    for (const auto &person : persons) {
        std::cout << "id " << person.id << std::endl;
        stmt->bind("id", person.id);
        std::cout << "name " << person.name << std::endl;
        stmt->bind("name", person.name);
        stmt->bind("age", person.age);
        stmt->bind("email", person.email);
        stmt->bind("born", person.born);
        stmt->bind("promise", person.promise);
        stmt->bind("val1", person.val1);
        stmt->bind("val2", person.val2);
        auto ret1 = co_await stmt->execute();
        EXPECT_TRUE(ret1.has_value());
        if (!ret1.has_value()) {
            co_return;
        }
        ILIAS_INFO("sql-test", "insert data success, effect rows: {}", ret1.value());

        std::cout << "[INFO] insert data success, id: " << person.id << std::endl;
        stmt->reset();
    }

    // --- E. 条件查询 (Select WHERE) ---
    const char *select_where_sql = "SELECT * FROM test_table WHERE id > :id";
    ret2                         = co_await mysql_connection->prepare(select_where_sql);
    EXPECT_TRUE(ret2);
    if (!ret2) {
        co_return;
    }
    (*ret2)->bind("id", 1);
    auto ret4 = co_await (*ret2)->query();
    EXPECT_TRUE(ret4.has_value());
    if (!ret4.has_value()) {
        std::cout << "[ERROR] select error: " << ret4.error().value() << std::endl;
        co_return;
    }
    auto result = std::move(ret4.value());

    std::cout << "[INFO] Executing conditional query..." << std::endl;

    ILIAS_INFO("sql-test", "column size {}", result->columnCount());
    while (1) {
        auto ret = co_await result->next();
        if (!ret) {
            ILIAS_WARN("sql-test", "error: {}", ret.error().message());
            break;
        }
        if (!*ret) {
            ILIAS_INFO("sql-test", "end of result");
            break;
        }
        auto idV = result->getValue("id").value();
        auto id    = std::get<int64_t>(idV);
        auto nameV = result->getValue("name").value();
        auto name = get<std::string>(nameV);
        auto ageV = result->getValue("age").value();
        auto age   = std::get<int64_t>(ageV);
        auto bornV = result->getValue("born").value();
        auto born   = std::get<std::string>(bornV);
        auto emailV = result->getValue("email").value();
        auto email    = std::get<std::string>(emailV);
        auto promiseV = result->getValue("promise").value();
        auto promise = std::get<std::vector<std::byte>>(promiseV);
        auto val1V   = result->getValue("val1").value();
        auto val1  = std::get<int64_t>(val1V);
        auto val2V = result->getValue("val2").value();
        auto        val2 = std::get<int64_t>(val2V);
        std::string str;
        for (auto &b : promise) {
            str += std::to_string(static_cast<int>(b)) + ".";
        }
        ILIAS_INFO("sql-test", "id:{} name:{} age:{} email:{} born:{} val1:{} val2:{} promise:{}", id, name, age, email,
                   born, (int)val1, val2, str);
    }
}

TEST(SQL, test) {
    test().wait();
}

int main(int argc, char **argv) {
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    cpptrace::init();
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    // return RUN_ALL_TESTS();
    test().wait();
    return 0;
}