#include <cstdlib>
#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <string>
#include <vector>
#include <chrono>
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;

// ==========================================
// 1. 测试辅助宏和工具
// ==========================================
#define CO_EXPECT_RESULT(result)                                                                                       \
    do {                                                                                                               \
        EXPECT_TRUE(result.has_value());                                                                               \
        if (!result.has_value()) {                                                                                     \
            ILIAS_ERROR("orm-test", "failed: {}", result.error().message());                                           \
        }                                                                                                              \
    } while (0)

#define CO_EXPECT_NOT_RESULT(result)                                                                                   \
    do {                                                                                                               \
        EXPECT_FALSE(result.has_value());                                                                              \
        if (!result.has_value()) {                                                                                     \
            ILIAS_INFO("orm-test", "expected failure: {}", result.error().message());                                  \
        }                                                                                                              \
    } while (0)

#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        CO_EXPECT_RESULT(ret);                                                                                         \
        if (!ret.has_value()) {                                                                                        \
            co_return Unexpected(ret.error());                                                                         \
        }                                                                                                              \
    } while (0)

// 性能测试辅助类
class PerformanceTimer {
public:
    PerformanceTimer(const std::string &name) : name_(name), start_(std::chrono::high_resolution_clock::now()) {}

    ~PerformanceTimer() {
        auto end      = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_);
        ILIAS_INFO("orm-test", "Performance [{}]: {} ms", name_, duration.count());
    }

private:
    std::string                                    name_;
    std::chrono::high_resolution_clock::time_point start_;
};

#define PERF_TIMER(name) PerformanceTimer _timer(name)

// ==========================================
// 2. 测试数据模型定义
// ==========================================

// 简化的用户实体
struct SimpleUser {
    int                        id;
    std::string                name;
    std::optional<int>         age;
    std::optional<std::string> email;
    SqlDate                    created_at;
    bool                       is_active = true;
    double                     balance   = 0.0;
};

// 简化的订单实体
struct SimpleOrder {
    int         id      = 0;
    int         user_id = 0;
    double      amount  = 0.0;
    std::string status  = "pending";
    SqlDate     order_date;
};

// 测试数据模型
struct ExtendedUser {
    int                id;
    std::string        name;
    std::optional<int> age;
    std::string        email;
    double             salary;
    std::string        department;
    SqlDate            created_at;
};

// 反射元数据定义
NEKO_BEGIN_NAMESPACE
template <>
struct Meta<ExtendedUser, void> {
    constexpr static auto value = Object("id", make_tags<SqlTags {.primary_key = true}>(&ExtendedUser::id), "name",
                                         make_tags<SqlTags {.not_null = true}>(&ExtendedUser::name), "age",
                                         make_tags<SqlTags {}>(&ExtendedUser::age), "email",
                                         make_tags<SqlTags {.unique = true}>(&ExtendedUser::email), "salary",
                                         make_tags<SqlTags {.not_null = true}>(&ExtendedUser::salary), "department",
                                         make_tags<SqlTags {.not_null = true}>(&ExtendedUser::department), "created_at",
                                         make_tags<SqlTags {.not_null = true}>(&ExtendedUser::created_at));
};
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = Object(
        "id", make_tags<SqlTags {.unique = true, .not_null = true, .primary_key = true}>(&SimpleUser::id), "name",
        make_tags<SqlTags {.not_null = true}>(&SimpleUser::name), "age", make_tags<SqlTags {}>(&SimpleUser::age),
        "email", make_tags<SqlTags {.unique = true, .not_null = false}>(&SimpleUser::email), "created_at",
        make_tags<SqlTags {.not_null = true}>(&SimpleUser::created_at), "is_active",
        make_tags<SqlTags {.not_null = true}>(&SimpleUser::is_active), "balance",
        make_tags<SqlTags {.not_null = true}>(&SimpleUser::balance));
};

template <>
struct Meta<SimpleOrder, void> {
    constexpr static auto value =
        Object("id", make_tags<SqlTags {.primary_key = true, .auto_increment = true}>(&SimpleOrder::id), "user_id",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::user_id), "amount",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::amount), "status",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::status), "order_date",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::order_date));
};
NEKO_END_NAMESPACE

// ==========================================
// 3. ORM 接口测试套件
// ==========================================
class ORMInterfaceTestSuite {
public:
    // 数据库连接配置
    static auto get_options() -> ConnectOptions {
        auto get_env = [](const char *name, const char *default_val) -> std::string {
            const char *val = std::getenv(name);
            return val ? std::string(val) : std::string(default_val);
        };
        auto get_env_int = [](const char *name, int default_val) -> int {
            const char *val = std::getenv(name);
            return val ? std::atoi(val) : default_val;
        };

        ConnectOptions options;
        options.host     = get_env("DB_HOST", "127.0.0.1");
        options.port     = get_env_int("DB_PORT", 3306);
        options.user     = get_env("DB_USER", "root");
        options.password = get_env("DB_PASS", "123456");
        options.database = get_env("DB_NAME", "test");
        ILIAS_INFO("mysql-test", "Connecting to MySQL: host={}, port={}, user={}, password={}, database={}",
                   options.host, options.port, options.user, options.password, options.database);

        // MySQL 优化配置
        options.extra.insert(std::make_pair("InitCommand", "SET NAMES 'utf8mb4'"));
        options.extra.insert(std::make_pair("ConnectTimeout", "30"));
        options.extra.insert(std::make_pair("ReadTimeout", "30"));
        options.extra.insert(std::make_pair("WriteTimeout", "30"));
        options.extra.insert(std::make_pair("Protocol", "MYSQL_PROTOCOL_TCP"));
        options.extra.insert(std::make_pair("SslEnforce", "false"));
        options.extra.insert(std::make_pair("SslVerifyServerCert", "false"));
        options.extra.insert(std::make_pair("DefaultAuth", "mysql_native_password"));

        return options;
    }

    // 数据库初始化
    static auto setup_db() -> IoTask<SqlDatabase> {
        auto options  = get_options();
        auto open_ret = co_await SqlDatabase::open("mysql", options);
        if (!open_ret) {
            ILIAS_ERROR("orm-test", "Failed to open MySQL: {}", open_ret.error().message());
            throw std::runtime_error("Failed to connect to MySQL");
        }
        auto db = std::move(open_ret.value());

        // 清理旧表
        co_await db.execute("DROP TABLE IF EXISTS simple_users");
        co_await db.execute("DROP TABLE IF EXISTS extended_users");

        co_return db;
    }

    // ==========================================
    // 测试场景 1: 基础 CRUD 操作
    // ==========================================
    static auto test_basic_crud_operations() -> IoTask<void> {
        PERF_TIMER("test_basic_crud_operations");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_basic_crud_operations");

        // 创建用户表
        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 1. Create - 插入测试数据
        {
            PERF_TIMER("insert_operations");

            // 单条插入
            auto insert_ret =
                co_await users.insert(1, "Alice", 25, "alice@test.com", SqlDate(2024, 1, 1, 10, 0, 0), true, 1000.50);
            CO_ASSERT_VAL(insert_ret);
            EXPECT_EQ(insert_ret.value(), 1);

            // 批量插入
            std::vector<SimpleUser> batch_users = {
                {2, "Bob", 30, std::nullopt, SqlDate(2024, 1, 2), true, 2000.0},
                {3, "Charlie", std::nullopt, "charlie@test.com", SqlDate(2024, 1, 3), false, 500.0},
                {4, "Diana", 28, "diana@test.com", SqlDate(2024, 1, 4), true, 1500.75}};
            auto batch_ret = co_await users.insert(batch_users);
            CO_ASSERT_VAL(batch_ret);
            EXPECT_EQ(batch_ret.value(), 3);
            co_await users.print();
        }
        // 2. Read - 查询测试
        {
            PERF_TIMER("read_operations");

            // 简单查询
            auto query_ret = co_await users.select().query();
            CO_ASSERT_VAL(query_ret);
            auto result = std::move(query_ret.value());

            int count = 0;
            ilias_for_await(auto &user, result.range()) {
                EXPECT_GT(user.id, 0);
                EXPECT_FALSE(user.name.empty());
                count++;
            }
            EXPECT_EQ(count, 4);

            // 条件查询
            auto cond_ret = co_await users.select().where(users.sql(&SimpleUser::is_active) == true).query();
            CO_ASSERT_VAL(cond_ret);
            auto cond_result = std::move(cond_ret.value());

            count = 0;
            ilias_for_await(auto &user, cond_result.range()) {
                EXPECT_TRUE(user.is_active);
                count++;
            }
            EXPECT_GE(count, 1); // 至少有一些活跃用户
        }

        // 3. Update - 更新测试
        {
            PERF_TIMER("update_operations");

            auto update_ret = co_await users.update()
                                  .set(users.sql(&SimpleUser::balance) = 3000.0, users.sql(&SimpleUser::age) = 31)
                                  .where(users.sql(&SimpleUser::name) == "Bob")
                                  .execute();
            CO_ASSERT_VAL(update_ret);
            EXPECT_EQ(update_ret.value(), 1);

            // 验证更新
            auto verify_ret = co_await users.select().where(users.sql(&SimpleUser::name) == "Bob").query();
            CO_ASSERT_VAL(verify_ret);
            auto verify_result = std::move(verify_ret.value());

            ilias_for_await(auto &user, verify_result.range()) {
                EXPECT_EQ(user.balance, 3000.0);
                EXPECT_EQ(user.age.value(), 31);
            }
        }

        // 4. Delete - 删除测试
        {
            PERF_TIMER("delete_operations");

            auto delete_ret = co_await users.remove().where(users.sql(&SimpleUser::is_active) == false).execute();
            CO_ASSERT_VAL(delete_ret);
            EXPECT_EQ(delete_ret.value(), 1); // Charlie

            // 验证删除
            auto count_ret = co_await users.count().query();
            CO_ASSERT_VAL(count_ret);
            auto count_result = std::move(count_ret.value());

            int remaining_count = 0;
            ilias_for_await([[maybe_unused]] auto &row, count_result.range()) {
                count_result.load(0, remaining_count);
            }
            EXPECT_EQ(remaining_count, 3);
        }

        ILIAS_INFO("orm-test", ">>> test_basic_crud_operations PASSED");
        co_return {};
    }

    // ==========================================
    // 测试场景 2: 高级查询功能
    // ==========================================
    static auto test_advanced_query_features() -> IoTask<void> {
        PERF_TIMER("test_advanced_query_features");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_advanced_query_features");

        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 准备测试数据
        std::vector<SimpleUser> test_users;
        for (int i = 1; i <= 20; ++i) {
            test_users.push_back({i, "User" + std::to_string(i), 20 + (i % 30),
                                  "user" + std::to_string(i) + "@test.com", SqlDate(2024, 1, i % 28 + 1),
                                  i % 2 == 1, // 奇数ID为活跃用户
                                  100.0 * i});
        }
        auto insert_ret = co_await users.insert(test_users);
        CO_ASSERT_VAL(insert_ret);

        // 1. 分页查询测试
        {
            PERF_TIMER("pagination_query");

            auto page_ret = co_await users.select()
                                .orderBy("id", false) // ASC
                                .limit(5)
                                .offset(5)
                                .query();
            CO_ASSERT_VAL(page_ret);
            auto page_result = std::move(page_ret.value());

            std::vector<int> ids;
            ilias_for_await(auto &user, page_result.range()) {
                ids.push_back(user.id);
            }

            EXPECT_EQ(ids.size(), 5);
            EXPECT_EQ(ids[0], 6);  // 第6个用户
            EXPECT_EQ(ids[4], 10); // 第10个用户
        }

        // 2. 排序查询测试
        {
            PERF_TIMER("sorting_query");

            auto sort_ret = co_await users.select()
                                .where(users.sql(&SimpleUser::is_active) == true)
                                .orderBy("balance", true) // DESC
                                .limit(3)
                                .query();
            CO_ASSERT_VAL(sort_ret);
            auto sort_result = std::move(sort_ret.value());

            std::vector<double> balances;
            ilias_for_await(auto &user, sort_result.range()) {
                balances.push_back(user.balance);
            }

            EXPECT_EQ(balances.size(), 3);
            EXPECT_GE(balances[0], balances[1]);
            EXPECT_GE(balances[1], balances[2]);
        }

        // 3. 聚合查询测试
        {
            PERF_TIMER("aggregation_query");

            // COUNT 查询
            auto count_ret = co_await users.count().where(users.sql(&SimpleUser::is_active) == true).query();
            CO_ASSERT_VAL(count_ret);
            auto count_result = std::move(count_ret.value());

            int active_count = 0;
            ilias_for_await([[maybe_unused]] auto &row, count_result.range()) {
                count_result.load(0, active_count);
            }
            EXPECT_EQ(active_count, 10); // 奇数ID用户
        }

        ILIAS_INFO("orm-test", ">>> test_advanced_query_features PASSED");
        co_return {};
    }

    // ==========================================
    // 测试场景 3: 事务管理测试
    // ==========================================
    static auto test_transaction_management() -> IoTask<void> {
        PERF_TIMER("test_transaction_management");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_transaction_management");

        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 1. 正常事务提交测试
        {
            PERF_TIMER("transaction_commit");

            auto tx = (co_await db.transaction()).value();

            // 在事务中插入数据
            auto insert_ret =
                co_await tx.execute("INSERT INTO simple_users (id, name, email, created_at, is_active, balance) "
                                    "VALUES (1, 'TxUser1', 'tx1@test.com', NOW(), true, 100.0)");
            CO_ASSERT_VAL(insert_ret);

            auto insert_ret2 =
                co_await tx.execute("INSERT INTO simple_users (id, name, email, created_at, is_active, balance) "
                                    "VALUES (2, 'TxUser2', 'tx2@test.com', NOW(), true, 200.0)");
            CO_ASSERT_VAL(insert_ret2);

            // 提交事务
            auto commit_ret = co_await tx.commit();
            CO_EXPECT_RESULT(commit_ret);

            // 验证数据存在
            auto count_ret = co_await users.count().query();
            CO_ASSERT_VAL(count_ret);
            auto count_result = std::move(count_ret.value());

            int count = 0;
            ilias_for_await([[maybe_unused]] auto &row, count_result.range()) {
                count_result.load(0, count);
            }
            EXPECT_EQ(count, 2);
        }

        // 2. 事务回滚测试
        {
            PERF_TIMER("transaction_rollback");

            auto tx = (co_await db.transaction()).value();

            // 在事务中插入数据
            auto insert_ret =
                co_await tx.execute("INSERT INTO simple_users (id, name, email, created_at, is_active, balance) "
                                    "VALUES (3, 'TxUser3', 'tx3@test.com', NOW(), true, 300.0)");
            CO_ASSERT_VAL(insert_ret);

            // 回滚事务
            auto rollback_ret = co_await tx.rollback();
            EXPECT_TRUE(rollback_ret);

            // 验证数据不存在
            auto count_ret = co_await users.count().query();
            CO_ASSERT_VAL(count_ret);
            auto count_result = std::move(count_ret.value());

            int count = 0;
            ilias_for_await([[maybe_unused]] auto &row, count_result.range()) {
                count_result.load(0, count);
            }
            EXPECT_EQ(count, 2); // 仍然是之前的2条记录
        }

        ILIAS_INFO("orm-test", ">>> test_transaction_management PASSED");
        co_return {};
    }

    // ==========================================
    // 测试场景 4: 错误处理测试
    // ==========================================
    static auto test_error_handling() -> IoTask<void> {
        PERF_TIMER("test_error_handling");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_error_handling");
        co_await db.execute("DROP TABLE IF EXISTS simple_users");
        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 1. 主键冲突测试
        {
            PERF_TIMER("primary_key_conflict");

            // 插入第一条记录
            auto insert1_ret =
                co_await users.insert(1, "User1", 25, "user1@test.com", SqlDate(2024, 1, 1), true, 100.0);
            CO_ASSERT_VAL(insert1_ret);

            // 尝试插入相同主键的记录
            auto insert2_ret =
                co_await users.insert(1, "User2", 30, "user2@test.com", SqlDate(2024, 1, 2), true, 200.0);
            CO_EXPECT_NOT_RESULT(insert2_ret); // 应该失败
        }

        // 2. 唯一键冲突测试
        {
            PERF_TIMER("unique_key_conflict");

            // 尝试插入相同email的记录
            auto insert_ret = co_await users.insert(2, "User3", 35, "user1@test.com", SqlDate(2024, 1, 3), true, 300.0);
            CO_EXPECT_NOT_RESULT(insert_ret); // 应该失败，email重复
        }

        // 3. 查询不存在的表
        {
            PERF_TIMER("table_not_exists");

            auto query_ret = co_await db.query<SimpleUser>("SELECT * FROM non_existent_table");
            CO_EXPECT_NOT_RESULT(query_ret); // 应该失败
        }

        // 4. SQL 语法错误
        {
            PERF_TIMER("sql_syntax_error");

            auto query_ret = co_await db.execute("INVALID SQL SYNTAX");
            CO_EXPECT_NOT_RESULT(query_ret); // 应该失败
        }

        ILIAS_INFO("orm-test", ">>> test_error_handling PASSED");
        co_return {};
    }

    // ==========================================
    // 测试场景 5: 类型转换器覆盖率测试
    // ==========================================
    static auto test_type_converter_coverage() -> IoTask<void> {
        PERF_TIMER("test_type_converter_coverage");
        auto db = (co_await setup_db()).value();
        co_await db->execute("DROP TABLE IF EXISTS type_test");
        ILIAS_INFO("orm-test", ">>> Running test_type_converter_coverage");

        // 创建测试表，包含各种数据类型
        auto create_sql = R"(
            CREATE TABLE type_test (
                id INT PRIMARY KEY,
                bool_val BOOLEAN,
                float_val FLOAT,
                blob_val BLOB,
                null_int INT NULL,
                null_text VARCHAR(255) NULL
            )
        )";
        auto create_ret = co_await db.execute(create_sql);
        CO_ASSERT_VAL(create_ret);

        // 1. 测试 bool 类型转换
        {
            auto insert_ret = co_await db.execute("INSERT INTO type_test (id, bool_val) VALUES (1, TRUE)");
            CO_ASSERT_VAL(insert_ret);

            auto query_ret = co_await db.query<bool>("SELECT bool_val FROM type_test WHERE id = 1");
            CO_ASSERT_VAL(query_ret);
            auto result = std::move(query_ret.value());

            ilias_for_await(auto &val, result.range()) {
                EXPECT_TRUE(std::get<0>(val));
            }
        }

        // 2. 测试 float 类型转换
        {
            auto insert_ret = co_await db.execute("INSERT INTO type_test (id, float_val) VALUES (2, 3.14)");
            CO_ASSERT_VAL(insert_ret);

            auto query_ret = co_await db.query<float>("SELECT float_val FROM type_test WHERE id = 2");
            CO_ASSERT_VAL(query_ret);
            auto result = std::move(query_ret.value());

            ilias_for_await(auto &val, result.range()) {
                EXPECT_NEAR(std::get<0>(val), 3.14f, 0.01f);
            }
        }

        // 3. 测试 blob 类型转换
        {
            auto insert_ret = co_await db.execute("INSERT INTO type_test (id, blob_val) VALUES (3, 'binary_data')");
            CO_ASSERT_VAL(insert_ret);

            auto query_ret = co_await db.query<std::vector<uint8_t>>("SELECT blob_val FROM type_test WHERE id = 3");
            CO_ASSERT_VAL(query_ret);
            auto result = std::move(query_ret.value());

            ilias_for_await(auto &val, result.range()) {
                EXPECT_FALSE(std::get<0>(val).empty());
            }
        }

        // 4. 测试 NULL 值处理 - 非 optional 类型遇到 NULL
        {
            auto insert_ret = co_await db.execute("INSERT INTO type_test (id, null_int) VALUES (4, NULL)");
            CO_ASSERT_VAL(insert_ret);

            // 尝试将 NULL 转换为非 optional 类型应该失败
            auto query_ret = co_await db.query<void>("SELECT null_int FROM type_test WHERE id = 4");
            CO_ASSERT_VAL(query_ret);
            int a;
            ilias_for_await(auto &ret, query_ret->range(a)) {
                CO_EXPECT_NOT_RESULT(ret);
            }
        }

        // 5. 测试 optional 类型处理 NULL
        {
            auto query_ret = co_await db.query<std::optional<int>>("SELECT null_int FROM type_test WHERE id = 4");
            CO_ASSERT_VAL(query_ret);
            auto result = std::move(query_ret.value());

            ilias_for_await(auto &val, result.range()) {
                EXPECT_FALSE(std::get<0>(val).has_value());
            }
        }

        ILIAS_INFO("orm-test", ">>> test_type_converter_coverage PASSED");
        co_return {};
    }

    // ==========================================
    // 测试场景 6: 条件构建器覆盖率测试
    // ==========================================
    static auto test_condition_builder_coverage() -> IoTask<void> {
        PERF_TIMER("test_condition_builder_coverage");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_condition_builder_coverage");

        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 准备测试数据
        std::vector<SimpleUser> test_users = {{1, "Alice", 25, "alice@test.com", SqlDate(2024, 1, 1), true, 1000.0},
                                              {2, "Bob", 30, "bob@test.com", SqlDate(2024, 1, 2), false, 2000.0},
                                              {3, "Charlie", 35, std::nullopt, SqlDate(2024, 1, 3), true, 1500.0}};
        auto                    insert_ret = co_await users.insert(test_users);
        CO_ASSERT_VAL(insert_ret);

        // 1. 测试各种比较操作符
        {
            // 测试 < 操作符
            auto lt_ret = co_await users.select().where(users.sql(&SimpleUser::age) < 30).query();
            CO_ASSERT_VAL(lt_ret);
            auto lt_result = std::move(lt_ret.value());

            int count = 0;
            ilias_for_await(auto &user, lt_result.range()) {
                EXPECT_LT(user.age.value(), 30);
                count++;
            }
            EXPECT_EQ(count, 1); // Alice

            // 测试 <= 操作符
            auto le_ret = co_await users.select().where(users.sql(&SimpleUser::age) <= 30).query();
            CO_ASSERT_VAL(le_ret);
            auto le_result = std::move(le_ret.value());

            count = 0;
            ilias_for_await(auto &user, le_result.range()) {
                EXPECT_LE(user.age.value(), 30);
                count++;
            }
            EXPECT_EQ(count, 2); // Alice, Bob

            // 测试 > 操作符
            auto gt_ret = co_await users.select().where(users.sql(&SimpleUser::age) > 30).query();
            CO_ASSERT_VAL(gt_ret);
            auto gt_result = std::move(gt_ret.value());

            count = 0;
            ilias_for_await(auto &user, gt_result.range()) {
                EXPECT_GT(user.age.value(), 30);
                count++;
            }
            EXPECT_EQ(count, 1); // Charlie

            // 测试 >= 操作符
            auto ge_ret = co_await users.select().where(users.sql(&SimpleUser::age) >= 30).query();
            CO_ASSERT_VAL(ge_ret);
            auto ge_result = std::move(ge_ret.value());

            count = 0;
            ilias_for_await(auto &user, ge_result.range()) {
                EXPECT_GE(user.age.value(), 30);
                count++;
            }
            EXPECT_EQ(count, 2); // Bob, Charlie

            // 测试 != 操作符
            auto ne_ret = co_await users.select().where(users.sql(&SimpleUser::name) != "Alice").query();
            CO_ASSERT_VAL(ne_ret);
            auto ne_result = std::move(ne_ret.value());

            count = 0;
            ilias_for_await(auto &user, ne_result.range()) {
                EXPECT_NE(user.name, "Alice");
                count++;
            }
            EXPECT_EQ(count, 2); // Bob, Charlie
        }

        // 2. 测试 LIKE 操作符
        {
            auto like_ret = co_await users.select().where(users.sql(&SimpleUser::name).like("A%")).query();
            CO_ASSERT_VAL(like_ret);
            auto like_result = std::move(like_ret.value());

            int count = 0;
            ilias_for_await(auto &user, like_result.range()) {
                EXPECT_EQ(user.name[0], 'A'); // 检查名字以A开头
                count++;
            }
            EXPECT_EQ(count, 1); // Alice
        }

        // 3. 测试 NULL 值比较
        {
            // 测试 IS NULL
            auto null_ret = co_await users.select().where(users.sql(&SimpleUser::email) == nullptr).query();
            CO_ASSERT_VAL(null_ret);
            auto null_result = std::move(null_ret.value());

            int count = 0;
            ilias_for_await(auto &user, null_result.range()) {
                EXPECT_FALSE(user.email.has_value());
                count++;
            }
            EXPECT_EQ(count, 1); // Charlie

            // 测试 IS NOT NULL
            auto not_null_ret = co_await users.select().where(users.sql(&SimpleUser::email) != nullptr).query();
            CO_ASSERT_VAL(not_null_ret);
            auto not_null_result = std::move(not_null_ret.value());

            count = 0;
            ilias_for_await(auto &user, not_null_result.range()) {
                EXPECT_TRUE(user.email.has_value());
                count++;
            }
            EXPECT_EQ(count, 2); // Alice, Bob
        }

        // 4. 测试逻辑操作符组合
        {
            // 测试 AND 操作符
            auto and_ret =
                co_await users.select()
                    .where((users.sql(&SimpleUser::is_active) == true) && (users.sql(&SimpleUser::balance) > 1200.0))
                    .query();
            CO_ASSERT_VAL(and_ret);
            auto and_result = std::move(and_ret.value());

            int count = 0;
            ilias_for_await(auto &user, and_result.range()) {
                EXPECT_TRUE(user.is_active);
                EXPECT_GT(user.balance, 1200.0);
                count++;
            }
            EXPECT_EQ(count, 1); // Charlie

            // 测试 OR 操作符
            auto or_ret =
                co_await users.select()
                    .where((users.sql(&SimpleUser::name) == "Alice") || (users.sql(&SimpleUser::name) == "Bob"))
                    .query();
            CO_ASSERT_VAL(or_ret);
            auto or_result = std::move(or_ret.value());

            count = 0;
            ilias_for_await(auto &user, or_result.range()) {
                EXPECT_TRUE(user.name == "Alice" || user.name == "Bob");
                count++;
            }
            EXPECT_EQ(count, 2); // Alice, Bob

            // 测试 NOT 操作符
            auto not_ret = co_await users.select().where(!(users.sql(&SimpleUser::is_active) == true)).query();
            CO_ASSERT_VAL(not_ret);
            auto not_result = std::move(not_ret.value());

            count = 0;
            ilias_for_await(auto &user, not_result.range()) {
                EXPECT_FALSE(user.is_active);
                count++;
            }
            EXPECT_EQ(count, 1); // Bob
        }

        // 5. 测试命名参数绑定
        {
            auto update_ret = co_await users.update()
                                  .set(users.sql(&SimpleUser::balance) = 3000.0)
                                  .where(users.sql(&SimpleUser::name) == "Alice")
                                  .execute();
            CO_ASSERT_VAL(update_ret);
            EXPECT_EQ(update_ret.value(), 1);

            // 验证更新
            auto verify_ret = co_await users.select().where(users.sql(&SimpleUser::name) == "Alice").query();
            CO_ASSERT_VAL(verify_ret);
            auto verify_result = std::move(verify_ret.value());

            ilias_for_await(auto &user, verify_result.range()) {
                EXPECT_EQ(user.balance, 3000.0);
            }
        }

        ILIAS_INFO("orm-test", ">>> test_condition_builder_coverage PASSED");
        co_return {};
    }

    static auto test_new_condition_interfaces() -> IoTask<void> {
        auto db        = (co_await setup_db()).value();
        auto users_ret = co_await Form<ExtendedUser, MysqlTag>::create(db, "extended_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 准备测试数据
        std::vector<ExtendedUser> test_users = {
            {1, "Alice", 25, "alice@company.com", 50000.0, "Engineering", SqlDate(2024, 1, 1)},
            {2, "Bob", std::nullopt, "bob@company.com", 60000.0, "Marketing", SqlDate(2024, 1, 2)},
            {3, "Charlie", 30, "charlie@company.com", 55000.0, "Engineering", SqlDate(2024, 1, 3)},
            {4, "Diana", 28, "diana@company.com", 65000.0, "Sales", SqlDate(2024, 1, 4)},
            {5, "Eve", 35, "eve@company.com", 70000.0, "Engineering", SqlDate(2024, 1, 5)}};

        auto insert_ret = co_await users.insert(test_users);
        CO_ASSERT_VAL(insert_ret);
        co_await users.print();
        // 1. 测试 has_value() - 检查可选字段
        {
            auto query_ret = co_await users.select().where(users.sql(&ExtendedUser::age).has_value()).query();
            CO_ASSERT_VAL(query_ret);

            int count = 0;
            ilias_for_await(auto &user, query_ret.value().range()) {
                EXPECT_TRUE(user.age.has_value());
                count++;
            }
            EXPECT_EQ(count, 4); // Bob 没有年龄
        }

        // 2. 测试 is_null() 和 is_not_null()
        {
            co_await users.print();
            auto null_query = co_await users.select().where(users.sql(&ExtendedUser::age).is_null()).query();
            CO_ASSERT_VAL(null_query);

            int null_count = 0;
            ilias_for_await(auto &user, null_query.value().range()) {
                printf("%s\n", fmtlib::format("{} {} {}", user.id, user.name, user.age.value_or(-1)).c_str());
                EXPECT_FALSE(user.age.has_value());
                null_count++;
            }
            EXPECT_EQ(null_count, 1); // 只有 Bob

            auto not_null_query = co_await users.select().where(users.sql(&ExtendedUser::age).is_not_null()).query();
            CO_ASSERT_VAL(not_null_query);

            int not_null_count = 0;
            ilias_for_await(auto &user, not_null_query.value().range()) {
                EXPECT_TRUE(user.age.has_value());
                not_null_count++;
            }
            EXPECT_EQ(not_null_count, 4);
        }

        // 3. 测试 in() 操作符
        {
            auto in_query = co_await users.select()
                                .where(users.sql(&ExtendedUser::department).in({"Engineering", "Sales"}))
                                .query();
            CO_ASSERT_VAL(in_query);

            int count = 0;
            ilias_for_await(auto &user, in_query.value().range()) {
                EXPECT_TRUE(user.department == "Engineering" || user.department == "Sales");
                count++;
            }
            EXPECT_EQ(count, 4); // Alice, Charlie, Diana, Eve
        }

        // 4. 测试 not_in() 操作符
        {
            auto not_in_query =
                co_await users.select().where(users.sql(&ExtendedUser::department).not_in({"Marketing"})).query();
            CO_ASSERT_VAL(not_in_query);

            int count = 0;
            ilias_for_await(auto &user, not_in_query.value().range()) {
                EXPECT_NE(user.department, "Marketing");
                count++;
            }
            EXPECT_EQ(count, 4); // 除了 Bob
        }

        // 5. 测试 between() 操作符
        {
            auto between_query =
                co_await users.select().where(users.sql(&ExtendedUser::salary).between(55000.0, 65000.0)).query();
            CO_ASSERT_VAL(between_query);

            int count = 0;
            ilias_for_await(auto &user, between_query.value().range()) {
                EXPECT_GE(user.salary, 55000.0);
                EXPECT_LE(user.salary, 65000.0);
                count++;
            }
            EXPECT_EQ(count, 3); // Charlie, Bob, Diana
        }

        // 6. 测试字符串匹配函数
        {
            // starts_with
            auto starts_query =
                co_await users.select().where(users.sql(&ExtendedUser::email).starts_with("alice")).query();
            CO_ASSERT_VAL(starts_query);

            int starts_count = 0;
            ilias_for_await(auto &user, starts_query.value().range()) {
                EXPECT_TRUE(user.email.starts_with("alice"));
                starts_count++;
            }
            EXPECT_EQ(starts_count, 1);

            // contains
            auto contains_query =
                co_await users.select().where(users.sql(&ExtendedUser::email).contains("company")).query();
            CO_ASSERT_VAL(contains_query);

            int contains_count = 0;
            ilias_for_await(auto &user, contains_query.value().range()) {
                EXPECT_TRUE(user.email.find("company") != std::string::npos);
                contains_count++;
            }
            EXPECT_EQ(contains_count, 5); // 所有用户
        }

        co_return {};
    }

    static auto test_aggregate_functions() -> IoTask<void> {
        auto db        = (co_await setup_db()).value();
        auto users_ret = co_await Form<ExtendedUser, MysqlTag>::create(db, "extended_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 准备测试数据
        std::vector<ExtendedUser> test_users = {
            {1, "Alice", 25, "alice@company.com", 50000.0, "Engineering", SqlDate(2024, 1, 1)},
            {2, "Bob", 30, "bob@company.com", 60000.0, "Engineering", SqlDate(2024, 1, 2)},
            {3, "Charlie", 35, "charlie@company.com", 70000.0, "Sales", SqlDate(2024, 1, 3)}};

        auto insert_ret = co_await users.insert(test_users);
        CO_ASSERT_VAL(insert_ret);

        // 测试 COUNT
        {
            auto count_query = co_await users.select(count(users.sql(&ExtendedUser::id))).query();
            CO_ASSERT_VAL(count_query);

            ilias_for_await(auto &row, count_query.value().range()) {
                auto [total_count] = row;
                EXPECT_EQ(total_count, 3);
            }
        }

        // 测试 SUM
        {
            auto sum_query = co_await users.select(sum(users.sql(&ExtendedUser::salary))).query();
            CO_ASSERT_VAL(sum_query);

            ilias_for_await(auto &row, sum_query.value().range()) {
                auto [total_salary] = row;
                EXPECT_DOUBLE_EQ(total_salary, 180000.0);
            }
        }

        // 测试 AVG
        {
            auto avg_query = co_await users.select(avg(users.sql(&ExtendedUser::salary))).query();
            CO_ASSERT_VAL(avg_query);

            ilias_for_await(auto &row, avg_query.value().range()) {
                auto [avg_salary] = row;
                EXPECT_DOUBLE_EQ(avg_salary, 60000.0);
            }
        }

        // 测试 MIN/MAX
        {
            auto minmax_query =
                co_await users.select(min(users.sql(&ExtendedUser::salary)), max(users.sql(&ExtendedUser::salary)))
                    .query();
            CO_ASSERT_VAL(minmax_query);

            ilias_for_await(auto &row, minmax_query.value().range()) {
                auto [min_salary, max_salary] = row;
                EXPECT_DOUBLE_EQ(min_salary, 50000.0);
                EXPECT_DOUBLE_EQ(max_salary, 70000.0);
            }
        }

        co_return {};
    }

    static auto test_math_functions() -> IoTask<void> {
        auto db        = (co_await setup_db()).value();
        auto users_ret = co_await Form<ExtendedUser, MysqlTag>::create(db, "extended_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 准备测试数据
        std::vector<ExtendedUser> test_users = {
            {1, "Alice", 25, "alice@company.com", -1500.75, "Engineering", SqlDate(2024, 1, 1)},
            {2, "Bob", 30, "bob@company.com", 2300.25, "Engineering", SqlDate(2024, 1, 2)},
            {3, "Charlie", 35, "charlie@company.com", 1750.99, "Sales", SqlDate(2024, 1, 3)}};

        auto insert_ret = co_await users.insert(test_users);
        CO_ASSERT_VAL(insert_ret);

        // 测试 ABS
        {
            auto abs_query =
                co_await users.select(users.sql(&ExtendedUser::name), abs(users.sql(&ExtendedUser::salary)))
                    .where(users.sql(&ExtendedUser::id) == 1)
                    .query();
            CO_ASSERT_VAL(abs_query);

            ilias_for_await(auto &row, abs_query.value().range()) {
                auto [name, abs_salary] = row;
                EXPECT_EQ(name, "Alice");
                EXPECT_DOUBLE_EQ(abs_salary, 1500.75);
            }
        }

        // 测试 ROUND
        {
            auto round_query =
                co_await users.select(users.sql(&ExtendedUser::name), round(users.sql(&ExtendedUser::salary), 1))
                    .where(users.sql(&ExtendedUser::id) == 3)
                    .query();
            CO_ASSERT_VAL(round_query);

            ilias_for_await(auto &row, round_query.value().range()) {
                auto [name, rounded_salary] = row;
                EXPECT_EQ(name, "Charlie");
                EXPECT_DOUBLE_EQ(rounded_salary, 1751.0);
            }
        }

        co_return {};
    }
};

// ==========================================
// 5. Google Test 集成
// ==========================================
TEST(ORMExtensionsTest, TestNewConditionInterfaces) {
    ORMInterfaceTestSuite::test_new_condition_interfaces().wait();
}

// 测试聚合函数
TEST(ORMExtensionsTest, TestAggregateFunctions) {
    ORMInterfaceTestSuite::test_aggregate_functions().wait();
}

// 测试数学函数
TEST(ORMExtensionsTest, TestMathFunctions) {
    ORMInterfaceTestSuite::test_math_functions().wait();
}

// 基础CRUD测试
TEST(ORMInterface, BasicCRUDOperations) {
    ORMInterfaceTestSuite::test_basic_crud_operations().wait();
}

// 高级查询测试
TEST(ORMInterface, AdvancedQueryFeatures) {
    ORMInterfaceTestSuite::test_advanced_query_features().wait();
}

// 事务管理测试
TEST(ORMInterface, TransactionManagement) {
    ORMInterfaceTestSuite::test_transaction_management().wait();
}

// 错误处理测试
TEST(ORMInterface, ErrorHandling) {
    ORMInterfaceTestSuite::test_error_handling().wait();
}

// 类型转换器覆盖率测试
TEST(ORMInterface, TypeConverterCoverage) {
    ORMInterfaceTestSuite::test_type_converter_coverage().wait();
}

// 条件构建器覆盖率测试
TEST(ORMInterface, ConditionBuilderCoverage) {
    ORMInterfaceTestSuite::test_condition_builder_coverage().wait();
}

// ==========================================
// 6. 主函数和初始化
// ==========================================
int main(int argc, char **argv) {
    // 初始化错误追踪
    cpptrace::init();

    // 设置日志级别
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);

    // 初始化平台上下文
    ilias::PlatformContext ioContext;
    ioContext.install();

    // 初始化Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // 输出测试环境信息
    ILIAS_INFO("orm-test", "=== ORM Interface Test Environment ===");
    ILIAS_INFO("orm-test", "Platform: Linux");
    ILIAS_INFO("orm-test", "C++ Standard: {}", __cplusplus);

    // 检查环境变量
    const char *db_host = std::getenv("DB_HOST");
    const char *db_port = std::getenv("DB_PORT");
    const char *db_user = std::getenv("DB_USER");
    const char *db_name = std::getenv("DB_NAME");

    ILIAS_INFO("orm-test", "Database Configuration:");
    ILIAS_INFO("orm-test", "  Host: {}", db_host ? db_host : "127.0.0.1 (default)");
    ILIAS_INFO("orm-test", "  Port: {}", db_port ? db_port : "3306 (default)");
    ILIAS_INFO("orm-test", "  User: {}", db_user ? db_user : "root (default)");
    ILIAS_INFO("orm-test", "  Database: {}", db_name ? db_name : "test (default)");

    ILIAS_INFO("orm-test", "========================================");

    // 运行所有测试
    int result = RUN_ALL_TESTS();

    // 输出测试结果摘要
    if (result == 0) {
        ILIAS_INFO("orm-test", "=== All Tests PASSED ===");
    }
    else {
        ILIAS_ERROR("orm-test", "=== Some Tests FAILED ===");
    }

    return result;
}