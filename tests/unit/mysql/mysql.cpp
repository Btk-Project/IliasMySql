#include <gtest/gtest.h>

#include <ilias/platform.hpp>
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "ilias/sql/sqldatabase.hpp"

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
    ConnectOptions options;
    options.host     = "127.0.0.1";
    options.port     = 3306;
    options.user     = "root";
    options.password = "123456";
    options.database = "test";
    options.extra.insert(std::make_pair("InitCommand", "SET NAMES 'utf8mb4'"));
    options.extra.insert(std::make_pair("ConnectTimeout", "30"));
    // --- A. 连接数据库 (Connect) ---
    auto ret = co_await SqlDatabase::open("mysql", options); //
    EXPECT_TRUE(ret.has_value()) << "Can't open database: " << ret.error().message();
    if (!ret.has_value()) {
        co_return;
    }
    auto db = std::move(ret.value());
    ILIAS_INFO("sql-test", "create sql {} with {}", db->sqlname(), db->sqlinfo());
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
    auto ret1 = co_await db.execute(create_sql);
    EXPECT_TRUE(ret1);
    if (!ret1) {
        ILIAS_ERROR("sql-test", "error: {}", ret1.error().message());
        co_return;
    }
    // --- 清空表防止上一次测试干扰 ---
    auto ret7 = co_await db.execute("DELETE FROM test_table");
    EXPECT_TRUE(ret7);
    if (!ret7) {
        co_return;
    }
    ILIAS_INFO("sql-test", "create table test_table success");

    // --- C. 准备数据 ---
    std::vector<Person> persons = {
        {1,
         "a test user",
         18,
         "test@test.com",
         SqlDate(2025, 6, 20, 1, 1, 1),
         {std::byte {1}, std::byte {2}, std::byte {0}},
         'a',
         234},
        {2,
         "王小明",
         19,
         "xiaoming@test.com",
         SqlDate(2025, 4, 21, 15, 23, 13),
         {std::byte {1}, std::byte {0}, std::byte {3}},
         'b',
         145},
        {3,
         "Alice",
         18,
         "Alice@test.com",
         SqlDate(2025, 2, 22, 12, 23, 23),
         {std::byte {1}, std::byte {2}, std::byte {5}},
         '3',
         345},
        {4,
         "Bob",
         18,
         "Bob@test.com",
         SqlDate(2025, 1, 20, 23, 12, 32),
         {std::byte {3}, std::byte {2}, std::byte {3}},
         '2',
         434},
    };

    // --- D. 插入数据 (Prepare & Bind) ---
    const char *insert_sql = "INSERT INTO test_table (id, name, age, born, email, promise, val1, val2) "
                             "VALUES (:id, :name, :age, :born, :email, :promise, :val1, :val2)";

    auto ret2 = co_await db.prepare<Person>(insert_sql);
    EXPECT_TRUE(ret2);
    auto stmt = std::move(ret2.value());
    for (auto &person : persons) {
        stmt.bind(person);
        auto ret1 = co_await stmt->execute();
        EXPECT_TRUE(ret1.has_value());
        if (!ret1.has_value()) {
            co_return;
        }
        ILIAS_INFO("sql-test", "insert data success, effect rows: {}", ret1.value());
        stmt->reset();
    }

    // --- E. 条件查询 (Select WHERE) ---
    const char *select_where_sql = "SELECT * FROM test_table WHERE id > :id";
    auto        ret3             = co_await db.prepare<std::tuple<int>>(select_where_sql);
    EXPECT_TRUE(ret3);
    if (!ret3) {
        co_return;
    }
    auto sqlstmt1 = std::move(ret3.value());
    sqlstmt1.bind(1);
    auto ret4 = co_await sqlstmt1.query();
    EXPECT_TRUE(ret4.has_value());
    if (!ret4.has_value()) {
        ILIAS_ERROR("sql-test", "select error {}", ret4.error().value());
        co_return;
    }
    SqlResult<Person> result = std::move(ret4.value());

    ILIAS_INFO("sql-test", "Executing {} query...", select_where_sql);

    ILIAS_INFO("sql-test", "column size {}", result->columnCount());
    ilias_for_await(auto person_result, result.range()) {
        std::string str;
        for (auto &b : person_result.promise) {
            str += std::to_string(static_cast<int>(b)) + ".";
        }
        ILIAS_INFO("sql-test", "id: {} name: {} age: {} email: {} born: {} val1: {} val2: {} promise: {}",
                   person_result.id, person_result.name, person_result.age, person_result.email,
                   person_result.born.toString(), (int)person_result.val1, person_result.val2, str);
    }

    const char *select_all_sql = "SELECT * FROM test_table";
    using PersonTuple = std::tuple<int, std::string, int, SqlDate, std::string, std::vector<std::byte>, char, int>;
    auto ret5         = co_await db.prepare<PersonTuple>(select_all_sql);
    EXPECT_TRUE(ret5);
    if (!ret5) {
        co_return;
    }
    auto ret6 = co_await (*ret5).query();
    EXPECT_TRUE(ret6.has_value());
    if (!ret6.has_value()) {
        ILIAS_ERROR("sql-test", "select error {}", ret6.error().value());
        co_return;
    }

    auto result1 = std::move(ret6.value());

    ILIAS_INFO("sql-test", "Executing {} query...", select_all_sql);

    ILIAS_INFO("sql-test", "column size {}", result1->columnCount());
    ilias_for_await(auto person_result, result1.range()) {
        std::string str;
        auto [id, name, age, born, email, promise, val1, val2] = person_result;
        for (auto &b : promise) {
            str += std::to_string(static_cast<int>(b)) + ".";
        }
        ILIAS_INFO("sql-test", "id: {} name: {} age: {} email: {} born: {} val1: {} val2: {} promise: {}", id, name,
                   age, email, born.toString(), (int)val1, val2, str);
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
    return RUN_ALL_TESTS();
    return 0;
}