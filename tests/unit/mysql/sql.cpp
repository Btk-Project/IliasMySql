#include <gtest/gtest.h>

#include <ilias/platform.hpp>
#include "ilias/mysql/sqlquery.hpp"
#include "ilias/mysql/sqlresult.hpp"

ILIAS_MYSQL_USE_NAMESPACE;

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
    SqlDatabase db;
    db.setHost("127.0.0.1");
    db.setUserName("root");
    db.setPassword("123456");
    db.setPort(3306);
    auto initCommand = sqlopt::InitCommand("SET NAMES 'utf8mb4'");
    db.setOption(initCommand);
    auto connectTimeout = sqlopt::ConnectTimeout(30);
    db.setOption(connectTimeout);

    auto ret1 = co_await db.open();
    EXPECT_TRUE(ret1.has_value());
    if (!ret1.has_value()) {
        co_return;
    }
    SqlQuery query(db);

    // create datebase test.
    auto ret = co_await query.execute("CREATE DATABASE IF NOT EXISTS test");
    EXPECT_TRUE(ret.has_value());
    if (!ret.has_value()) {
        co_return;
    }
    // use database test.
    ret1 = co_await db.selectDb("test");
    EXPECT_TRUE(ret.has_value());
    if (!ret.has_value()) {
        co_return;
    }
    // delete test_table if exists
    ret = co_await query.execute("DROP TABLE IF EXISTS test_table");
    EXPECT_TRUE(ret.has_value());
    if (!ret.has_value()) {
        co_return;
    }
    // create table test. / primary key id | NOT NULL name varchar(255) | age int | born date | UNIQUE email
    // varchar(255)
    ret = co_await query.execute(
        "CREATE TABLE IF NOT EXISTS test_table (id INT NOT NULL PRIMARY KEY, name VARCHAR(255) NOT "
        "NULL, age INT, born DATETIME, email VARCHAR(255) UNIQUE, promise BLOB(1000), val1 TINYINT, val2 MEDIUMINT)");
    EXPECT_TRUE(ret.has_value());
    if (!ret.has_value()) {
        co_return;
    }
    // insert data
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
    for (auto &person : persons) {
        auto ret = co_await query.prepare(
            "INSERT INTO test_table (id, name, age, born, email, promise, val1, val2) VALUES (:id, :name, "
            ":age, :born, :email, :promise, :val1, :val2)");
        EXPECT_TRUE(ret.has_value());
        if (!ret.has_value()) {
            co_return;
        }
        query.set("id", person.id);
        query.set("name", person.name);
        query.set("age", person.age);
        query.set("email", person.email);
        query.set("born", person.born);
        query.setView("promise", person.promise);
        query.set("val1", person.val1);
        query.set("val2", person.val2);
        auto ret1 = co_await query.execute();
        EXPECT_TRUE(ret1.has_value());
        if (!ret1.has_value()) {
            co_return;
        }
    }

    // select * from test_table
    ret1 = co_await query.prepare("SELECT * FROM test_table WHERE id>:id");
    EXPECT_TRUE(ret1.has_value());
    if (!ret1.has_value()) {
        co_return;
    }
    query.set("id", 1);
    ret = co_await query.execute();
    EXPECT_TRUE(ret.has_value());
    if (!ret.has_value()) {
        co_return;
    }
    auto result = std::move(ret.value());
    ILIAS_INFO("sql-test", "select size {}", result.countRows());
    while (1) {
        auto ret = co_await result.next();
        if (!ret) {
            ILIAS_WARN("sql-test", "error: {}", ret.error().message());
            break;
        }
        if (!*ret) {
            ILIAS_INFO("sql-test", "end of result");
            break;
        }
        auto        id      = result.get<int>("id").value_or(-1);
        auto        name    = result.get<std::string>("name").value_or("null");
        auto        age     = result.get<int>("age").value_or(-1);
        auto        born    = result.get<SqlDate>("born").value_or(SqlDate());
        auto        email   = result.get<std::string>("email").value_or("null");
        auto        promise = result.get<std::vector<std::byte>>("promise").value();
        auto        val1    = result.get<char>("val1").value();
        auto        val2    = result.get<int>("val2").value();
        std::string str;
        for (auto &b : promise) {
            str += std::to_string(static_cast<int>(b)) + ".";
        }
        ILIAS_INFO("sql", "id:{} name:{} age:{} email:{} born:{} val1:{} val2:{} promise:{}", id, name, age, email,
                   born.toString(), (int)val1, val2, str);
    }

    auto ret2 = co_await query.execute("SELECT * FROM test_table");
    EXPECT_TRUE(ret2.has_value());
    if (!ret2.has_value()) {
        co_return;
    }
    auto result1 = std::move(ret2.value());
    ILIAS_INFO("sql-test", "select size {}", result1.countRows());
    while (1) {
        auto ret = co_await result1.next();
        if (!ret) {
            ILIAS_WARN("sql-test", "error: {}", ret.error().message());
            break;
        }
        if (!*ret) {
            ILIAS_INFO("sql-test", "end of result");
            break;
        }
        auto        id      = result1.get<int>("id").value_or(-1);
        auto        name    = result1.get<std::string>("name").value_or("null");
        auto        age     = result1.get<int>("age").value_or(-1);
        auto        born    = result1.get<SqlDate>("born").value_or(SqlDate());
        auto        email   = result1.get<std::string>("email").value_or("null");
        auto        promise = result1.get<std::vector<std::byte>>("promise").value();
        auto        val1    = result1.get<char>("val1").value();
        auto        val2    = result1.get<int>("val2").value();
        std::string str;
        for (auto &b : promise) {
            str += std::to_string(static_cast<int>(b)) + ".";
        }
        ILIAS_INFO("sql", "id:{} name:{} age:{} email:{} born:{} val1:{} val2:{} promise:{}", id, name, age, email,
                   born.toString(), (int)val1, val2, str);
    }

    // co_await mysql.autoCommit(false);
    co_return;
}

TEST(SQL, test) {
    test().wait();
}

int main(int argc, char **argv) {
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
    return 0;
}