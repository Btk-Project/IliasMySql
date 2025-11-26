#include <gtest/gtest.h>

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
    ConnectOptions options;
    options.host     = "127.0.0.1";
    options.port     = 3306;
    options.user     = "root";
    options.password = "123456";
    options.extra.insert(std::make_pair("InitCommand", "SET NAMES 'utf8mb4'"));
    options.extra.insert(std::make_pair("ConnectTimeout", "30"));
    auto &driver = DriverManager::instance();
    auto  ret    = driver.createConnection("mysql", options);
    EXPECT_TRUE(ret.has_value());
    if (!ret.has_value()) {
        co_return;
    }
    auto mysql_connection = std::move(ret.value());
    auto connect_ret      = co_await mysql_connection->connect();
    EXPECT_TRUE(connect_ret.has_value());
    // create datebase test.
    auto ret1 = co_await mysql_connection->execute("CREATE DATABASE IF NOT EXISTS test");
    EXPECT_TRUE(ret1.has_value());
    if (!ret1.has_value()) {
        co_return;
    }
    ILIAS_INFO("sql-test", "create database test success, effect rows: {}", ret1.value());
    // use database test.
    auto ret2 = co_await mysql_connection->selectDatabase("test");
    EXPECT_TRUE(ret2.has_value());
    if (!ret2.has_value()) {
        co_return;
    }
    // delete test_table if exists
    ret1 = co_await mysql_connection->execute("DROP TABLE IF EXISTS test_table");
    EXPECT_TRUE(ret1.has_value());
    if (!ret1.has_value()) {
        co_return;
    }
    ILIAS_INFO("sql-test", "drop table test_table success, effect rows: {}", ret1.value());
    // create table test. / primary key id | NOT NULL name varchar(255) | age int | born date | UNIQUE email
    // varchar(255)
    ret1 = co_await mysql_connection->execute(
        "CREATE TABLE IF NOT EXISTS test_table (id INT NOT NULL PRIMARY KEY, name VARCHAR(255) NOT "
        "NULL, age INT, born DATETIME, email VARCHAR(255) UNIQUE, promise BLOB(1000), val1 TINYINT, val2 MEDIUMINT)");
    EXPECT_TRUE(ret1.has_value());
    if (!ret1.has_value()) {
        co_return;
    }
    ILIAS_INFO("sql-test", "create table test_table success, effect rows: {}", ret1.value());
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
        auto ret = co_await mysql_connection->prepare(
            "INSERT INTO test_table (id, name, age, born, email, promise, val1, val2) VALUES (:id, :name, "
            ":age, :born, :email, :promise, :val1, :val2)");
        EXPECT_TRUE(ret.has_value());
        if (!ret.has_value()) {
            co_return;
        }
        (*ret)->bind("id", person.id);
        (*ret)->bind("name", person.name);
        (*ret)->bind("age", person.age);
        (*ret)->bind("email", person.email);
        (*ret)->bind("born", person.born);
        (*ret)->bind("promise", person.promise);
        (*ret)->bind("val1", person.val1);
        (*ret)->bind("val2", person.val2);
        auto ret1 = co_await (*ret)->execute();
        EXPECT_TRUE(ret1.has_value());
        if (!ret1.has_value()) {
            co_return;
        }
        ILIAS_INFO("sql-test", "insert data success, effect rows: {}", ret1.value());
    }

    // select * from test_table
    auto ret3 = co_await mysql_connection->prepare("SELECT * FROM test_table WHERE id>:id");
    EXPECT_TRUE(ret3.has_value());
    if (!ret3.has_value()) {
        co_return;
    }
    (*ret3)->bind("id", 1);
    auto ret4 = co_await (*ret3)->query();
    EXPECT_TRUE(ret4.has_value());
    if (!ret4.has_value()) {
        co_return;
    }
    auto result = std::move(ret4.value());
    ILIAS_INFO("sql-test", "column size {}", result->columnCount());
    ILIAS_INFO("sql-test", "row size {}", result->rowCount());
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
        auto        id      = std::get<int>(result->getValue("id").value_or(-1));
        auto        name    = get<std::string>(result->getValue("name").value_or("null"));
        auto        age     = std::get<int>(result->getValue("age").value_or(-1));
        auto        born    = std::get<SqlDate>(result->getValue("born").value_or(SqlDate()));
        auto        email   = std::get<std::string>(result->getValue("email").value_or("null"));
        auto        promise = std::get<std::vector<std::byte>>(result->getValue("promise").value());
        auto        val1    = std::get<char>(result->getValue("val1").value());
        auto        val2    = std::get<int>(result->getValue("val2").value());
        std::string str;
        for (auto &b : promise) {
            str += std::to_string(static_cast<int>(b)) + ".";
        }
        ILIAS_INFO("sql", "id:{} name:{} age:{} email:{} born:{} val1:{} val2:{} promise:{}", id, name, age, email,
                   born.toString(), (int)val1, val2, str);
    }

    ret4 = co_await mysql_connection->query("SELECT * FROM test_table");
    EXPECT_TRUE(ret4.has_value());
    if (!ret4.has_value()) {
        co_return;
    }
    auto result1 = std::move(ret4.value());
    ILIAS_INFO("sql-test", "select size {}", result1->rowCount());
    while (1) {
        auto ret = co_await result1->next();
        if (!ret) {
            ILIAS_WARN("sql-test", "error: {}", ret.error().message());
            break;
        }
        if (!*ret) {
            ILIAS_INFO("sql-test", "end of result");
            break;
        }
        auto        id      = std::get<int>(result1->getValue("id").value_or(-1));
        auto        name    = get<std::string>(result1->getValue("name").value_or("null"));
        auto        age     = std::get<int>(result1->getValue("age").value_or(-1));
        auto        born    = std::get<SqlDate>(result1->getValue("born").value_or(SqlDate()));
        auto        email   = std::get<std::string>(result1->getValue("email").value_or("null"));
        auto        promise = std::get<std::vector<std::byte>>(result1->getValue("promise").value());
        auto        val1    = std::get<char>(result1->getValue("val1").value());
        auto        val2    = std::get<int>(result1->getValue("val2").value());
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
    cpptrace::init();
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
    return 0;
}