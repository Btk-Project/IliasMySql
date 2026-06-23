/**
 * @file test_orm_interface.cpp
 * @brief ORM interface tests for PostgreSQL backend
 *
 * Tests ORM layer integration with PostgreSQL backend including:
 * - Basic CRUD operations
 * - Advanced query features
 * - Transaction management
 * - Error handling
 * - Type conversion
 *
 * Validates: Requirements 1.x, 2.x, 3.x, 4.x, 5.x, 6.x
 */

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
            co_return Err(ret.error());                                                                         \
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
// clang-format off
template <>
struct Meta<ExtendedUser, void> {
    constexpr static auto value = Object(
        "id",           make_tags<SqlTags::createPrimaryKeyTags()>(&ExtendedUser::id), 
        "name",         make_tags<SqlTags {.not_null = true}>(&ExtendedUser::name), 
        "age",          make_tags<SqlTags {}>(&ExtendedUser::age), 
        "email",        make_tags<SqlTags {.unique = true, .index = true}>(&ExtendedUser::email), 
        "salary",       make_tags<SqlTags {.not_null = true}>(&ExtendedUser::salary), 
        "department",   make_tags<SqlTags {.not_null = true}>(&ExtendedUser::department), 
        "created_at",   make_tags<SqlTags {.not_null = true, .created_at = true}>(&ExtendedUser::created_at));
};
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = Object(
        "id",           make_tags<SqlTags::createPrimaryKeyTags()>(&SimpleUser::id), 
        "name",         make_tags<SqlTags {.not_null = true}>(&SimpleUser::name),
        "age",          make_tags<SqlTags {}>(&SimpleUser::age),
        "email",        make_tags<SqlTags {.not_null = false, .unique = true}>(&SimpleUser::email), 
        "created_at",   make_tags<SqlTags {.not_null = true}>(&SimpleUser::created_at),
        "is_active",    make_tags<SqlTags {.not_null = true}>(&SimpleUser::is_active),
        "balance",      make_tags<SqlTags {.not_null = true}>(&SimpleUser::balance));
};

template <>
struct Meta<SimpleOrder, void> {
    constexpr static auto value = Object(
        "id",           make_tags<SqlTags::createPrimaryKeyTags(true)>(&SimpleOrder::id), 
        "user_id",      make_tags<SqlTags {.not_null = true}>(&SimpleOrder::user_id),
        "amount",       make_tags<SqlTags {.not_null = true}>(&SimpleOrder::amount),
        "status",       make_tags<SqlTags {.not_null = true}>(&SimpleOrder::status),
        "order_date",   make_tags<SqlTags {.not_null = true}>(&SimpleOrder::order_date));
};
// clang-format on
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
        options.host     = get_env("PG_HOST", "localhost");
        options.port     = get_env_int("PG_PORT", 5432);
        options.user     = get_env("PG_USER", "test");
        options.password = get_env("PG_PASS", "test");
        options.database = get_env("PG_NAME", "testdb");
        ILIAS_INFO("pgsql-test", "Connecting to PostgreSQL: host={}, port={}, user={}, database={}", options.host,
                   options.port, options.user, options.database);

        return options;
    }

    // 数据库初始化
    static auto setup_db() -> IoTask<SqlDatabase> {
        auto options  = get_options();
        auto open_ret = co_await SqlDatabase::open("postgres", options);
        if (!open_ret) {
            ILIAS_ERROR("orm-test", "Failed to open PostgreSQL: {}", open_ret.error().message());
            throw std::runtime_error("Failed to connect to PostgreSQL");
        }
        auto db = std::move(open_ret.value());

        // 清理旧表
        co_await db.execute("DROP TABLE IF EXISTS simple_users");
        co_await db.execute("DROP TABLE IF EXISTS extended_users");

        co_return db;
    }

    // ==========================================
    // 测试场景 1: 基础 CRUD 操作 + 数据正确性校验
    // ==========================================
    static auto test_basic_crud_operations() -> IoTask<void> {
        PERF_TIMER("test_basic_crud_operations");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_basic_crud_operations");

        // 创建用户表
        auto users_ret = co_await Form<SimpleUser, PostgresTag>::create_if_not_exists(db, "simple_users");
        auto ret1      = co_await db.query("SELECT * FROM simple_users WHERE 1=0");
        CO_ASSERT_VAL(ret1);
        auto result1 = std::move(ret1.value());
        int  count1  = 0;
        ilias_for_await(auto &row, result1.range()) {
            count1++;
        }
        EXPECT_EQ(count1, 0) << "Query on empty result set should return zero rows";

        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 1. Create - 插入测试数据 + 数据完整性验证
        {
            PERF_TIMER("insert_operations");

            // 单条插入 - 验证所有字段的精确性
            auto insert_ret =
                co_await users.emplace(1, "Alice", 25, "alice@test.com", SqlDate(2024, 1, 1, 10, 0, 0), true, 1000.50);
            CO_ASSERT_VAL(insert_ret);
            EXPECT_EQ(insert_ret.value(), 1);

            // 立即验证插入的数据完整性
            auto verify_alice = co_await users.select().where(users.sql(&SimpleUser::id) == 1).query();
            CO_ASSERT_VAL(verify_alice);
            auto alice_result = std::move(verify_alice.value());

            bool alice_found = false;
            ilias_for_await(auto &user, alice_result.range()) {
                alice_found = true;
                // 验证每个字段的精确值
                EXPECT_EQ(user.id, 1);
                EXPECT_EQ(user.name, "Alice");
                EXPECT_TRUE(user.age.has_value());
                EXPECT_EQ(user.age.value(), 25);
                EXPECT_TRUE(user.email.has_value());
                EXPECT_EQ(user.email.value(), "alice@test.com");
                EXPECT_TRUE(user.is_active);
                EXPECT_DOUBLE_EQ(user.balance, 1000.50);

                // 验证日期时间精度
                EXPECT_EQ(user.created_at.year, 2024);
                EXPECT_EQ(user.created_at.month, 1);
                EXPECT_EQ(user.created_at.day, 1);
                EXPECT_EQ(user.created_at.hour, 10);
                EXPECT_EQ(user.created_at.minute, 0);
                EXPECT_EQ(user.created_at.second, 0);
            }
            EXPECT_TRUE(alice_found) << "Alice record not found after insertion";

            // 批量插入 - 包含边界值和NULL值测试
            std::vector<SimpleUser> batch_users = {
                {2, "Bob", 30, std::nullopt, SqlDate(2024, 1, 2), true, 2000.0},
                {3, "Charlie", std::nullopt, "charlie@test.com", SqlDate(2024, 1, 3), false, 500.0},
                {4, "Diana", 28, "diana@test.com", SqlDate(2024, 1, 4), true, 1500.75}};
            auto batch_ret = co_await users.insert(batch_users);
            CO_ASSERT_VAL(batch_ret);
            EXPECT_EQ(batch_ret.value(), 3);

            // 验证批量插入的数据完整性
            auto verify_batch =
                co_await users.select().where(users.sql(&SimpleUser::id) > 1).orderBy(users.sql(&SimpleUser::id), false).query();
            CO_ASSERT_VAL(verify_batch);
            auto batch_result = std::move(verify_batch.value());

            std::vector<SimpleUser> retrieved_users;
            ilias_for_await(auto &user, batch_result.range()) {
                retrieved_users.push_back(user);
            }

            EXPECT_EQ(retrieved_users.size(), 3);

            // 验证Bob的数据
            auto &bob = retrieved_users[0];
            EXPECT_EQ(bob.id, 2);
            EXPECT_EQ(bob.name, "Bob");
            EXPECT_TRUE(bob.age.has_value());
            EXPECT_EQ(bob.age.value(), 30);
            EXPECT_FALSE(bob.email.has_value()); // NULL值验证
            EXPECT_TRUE(bob.is_active);
            EXPECT_DOUBLE_EQ(bob.balance, 2000.0);

            // 验证Charlie的数据
            auto &charlie = retrieved_users[1];
            EXPECT_EQ(charlie.id, 3);
            EXPECT_EQ(charlie.name, "Charlie");
            EXPECT_FALSE(charlie.age.has_value()); // NULL值验证
            EXPECT_TRUE(charlie.email.has_value());
            EXPECT_EQ(charlie.email.value(), "charlie@test.com");
            EXPECT_FALSE(charlie.is_active);
            EXPECT_DOUBLE_EQ(charlie.balance, 500.0);

            // 验证Diana的数据
            auto &diana = retrieved_users[2];
            EXPECT_EQ(diana.id, 4);
            EXPECT_EQ(diana.name, "Diana");
            EXPECT_TRUE(diana.age.has_value());
            EXPECT_EQ(diana.age.value(), 28);
            EXPECT_TRUE(diana.email.has_value());
            EXPECT_EQ(diana.email.value(), "diana@test.com");
            EXPECT_TRUE(diana.is_active);
            EXPECT_DOUBLE_EQ(diana.balance, 1500.75);

            co_await users.print();
        }

        // 2. Read - 查询测试 + 数据一致性验证
        {
            PERF_TIMER("read_operations");

            // 全表查询 - 验证数据完整性
            auto query_ret = co_await users.select().orderBy(users.sql(&SimpleUser::id), false).query();
            CO_ASSERT_VAL(query_ret);
            auto result = std::move(query_ret.value());

            std::vector<SimpleUser> all_users;
            ilias_for_await(auto &user, result.range()) {
                all_users.push_back(user);
                EXPECT_GT(user.id, 0);
                EXPECT_FALSE(user.name.empty());
                EXPECT_GE(user.balance, 0.0);
            }
            EXPECT_EQ(all_users.size(), 4);

            // 验证数据的顺序和完整性
            EXPECT_EQ(all_users[0].name, "Alice");
            EXPECT_EQ(all_users[1].name, "Bob");
            EXPECT_EQ(all_users[2].name, "Charlie");
            EXPECT_EQ(all_users[3].name, "Diana");

            // 条件查询 - 验证过滤逻辑的正确性
            auto cond_ret = co_await users.select().where(users.sql(&SimpleUser::is_active) == true).query();
            CO_ASSERT_VAL(cond_ret);
            auto cond_result = std::move(cond_ret.value());

            std::vector<std::string> active_users;
            ilias_for_await(auto &user, cond_result.range()) {
                EXPECT_TRUE(user.is_active);
                active_users.push_back(user.name);
            }
            EXPECT_EQ(active_users.size(), 3); // Alice, Bob, Diana

            // 验证具体的活跃用户
            std::sort(active_users.begin(), active_users.end());
            EXPECT_EQ(active_users[0], "Alice");
            EXPECT_EQ(active_users[1], "Bob");
            EXPECT_EQ(active_users[2], "Diana");
        }

        // 3. Update - 更新测试 + 数据变更验证
        {
            PERF_TIMER("update_operations");

            // 记录更新前的状态
            auto before_update = co_await users.select().where(users.sql(&SimpleUser::name) == "Bob").query();
            CO_ASSERT_VAL(before_update);
            auto before_result = std::move(before_update.value());

            SimpleUser bob_before;
            bool       found_before = false;
            ilias_for_await(auto &user, before_result.range()) {
                bob_before   = user;
                found_before = true;
            }
            EXPECT_TRUE(found_before);
            EXPECT_DOUBLE_EQ(bob_before.balance, 2000.0);
            EXPECT_EQ(bob_before.age.value(), 30);

            // 执行更新操作
            auto update_ret = co_await users.update()
                                  .set(users.sql(&SimpleUser::balance) = 3000.0, users.sql(&SimpleUser::age) = 31)
                                  .where(users.sql(&SimpleUser::name) == "Bob")
                                  .execute();
            CO_ASSERT_VAL(update_ret);
            EXPECT_EQ(update_ret.value(), 1);

            // 验证更新后的数据完整性
            auto verify_ret = co_await users.select().where(users.sql(&SimpleUser::name) == "Bob").query();
            CO_ASSERT_VAL(verify_ret);
            auto verify_result = std::move(verify_ret.value());

            bool found_after = false;
            ilias_for_await(auto &user, verify_result.range()) {
                found_after = true;
                EXPECT_DOUBLE_EQ(user.balance, 3000.0);
                EXPECT_EQ(user.age.value(), 31);

                // 验证未更新的字段保持不变
                EXPECT_EQ(user.id, bob_before.id);
                EXPECT_EQ(user.name, bob_before.name);
                EXPECT_EQ(user.email, bob_before.email);
                EXPECT_EQ(user.is_active, bob_before.is_active);
                EXPECT_EQ(user.created_at.year, bob_before.created_at.year);
                EXPECT_EQ(user.created_at.month, bob_before.created_at.month);
                EXPECT_EQ(user.created_at.day, bob_before.created_at.day);
            }
            EXPECT_TRUE(found_after);
        }

        // 4. Delete - 删除测试 + 数据一致性验证
        {
            PERF_TIMER("delete_operations");

            // 记录删除前的状态
            auto before_delete = co_await users.select().query();
            CO_ASSERT_VAL(before_delete);
            auto before_result = std::move(before_delete.value());

            std::vector<std::string> names_before;
            ilias_for_await(auto &user, before_result.range()) {
                names_before.push_back(user.name);
            }
            EXPECT_EQ(names_before.size(), 4);

            // 执行删除操作
            auto delete_ret = co_await users.remove().where(users.sql(&SimpleUser::is_active) == false).execute();
            CO_ASSERT_VAL(delete_ret);
            EXPECT_EQ(delete_ret.value(), 1); // Charlie

            // 验证删除后的数据完整性
            auto after_delete = co_await users.select().orderBy(users.sql(&SimpleUser::id), false).query();
            CO_ASSERT_VAL(after_delete);
            auto after_result = std::move(after_delete.value());

            std::vector<std::string> names_after;
            ilias_for_await(auto &user, after_result.range()) {
                names_after.push_back(user.name);
                EXPECT_TRUE(user.is_active);
            }

            EXPECT_EQ(names_after.size(), 3);
            EXPECT_EQ(names_after[0], "Alice");
            EXPECT_EQ(names_after[1], "Bob");
            EXPECT_EQ(names_after[2], "Diana");

            // 确认Charlie确实被删除
            auto charlie_check = co_await users.select().where(users.sql(&SimpleUser::name) == "Charlie").query();
            CO_ASSERT_VAL(charlie_check);
            auto charlie_result = std::move(charlie_check.value());

            int charlie_count = 0;
            ilias_for_await([[maybe_unused]] auto &user, charlie_result.range()) {
                charlie_count++;
            }
            EXPECT_EQ(charlie_count, 0);

            // 验证最终计数
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

        auto users_ret = co_await Form<SimpleUser, PostgresTag>::create_if_not_exists(db, "simple_users");
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
                                .orderBy(users.sql(&SimpleUser::id), false) // ASC
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
                                .orderBy(users.sql(&SimpleUser::balance), true) // DESC
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

        auto users_ret = co_await Form<SimpleUser, PostgresTag>::create_if_not_exists(db, "simple_users");
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
        auto users_ret = co_await Form<SimpleUser, PostgresTag>::create_if_not_exists(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 1. 主键冲突测试
        {
            PERF_TIMER("primary_key_conflict");

            // 插入第一条记录
            auto insert1_ret =
                co_await users.emplace(1, "User1", 25, "user1@test.com", SqlDate(2024, 1, 1), true, 100.0);
            CO_ASSERT_VAL(insert1_ret);

            // 尝试插入相同主键的记录
            auto insert2_ret =
                co_await users.emplace(1, "User2", 30, "user2@test.com", SqlDate(2024, 1, 2), true, 200.0);
            CO_EXPECT_NOT_RESULT(insert2_ret); // 应该失败
        }

        // 2. 唯一键冲突测试
        {
            PERF_TIMER("unique_key_conflict");

            // 尝试插入相同email的记录
            auto insert_ret = co_await users.emplace(2, "User3", 35, "user1@test.com", SqlDate(2024, 1, 3), true, 300.0);
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
        co_await db.execute("DROP TABLE IF EXISTS type_test");
        ILIAS_INFO("orm-test", ">>> Running test_type_converter_coverage");

        // 创建测试表，包含各种数据类型
        auto create_sql = R"(
            CREATE TABLE type_test (
                id INT PRIMARY KEY,
                bool_val BOOLEAN,
                float_val REAL,
                blob_val BYTEA,
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

        // 3. 测试 blob 类型转换 (PostgreSQL uses BYTEA)
        {
            // PostgreSQL uses hex format for bytea by default
            auto insert_ret =
                co_await db.execute("INSERT INTO type_test (id, blob_val) VALUES (3, E'\\\\x62696e617279')");
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
    // 测试场景 6: 数据正确性和类型转换完整性测试
    // ==========================================
    static auto test_data_integrity_and_type_conversion() -> IoTask<void> {
        PERF_TIMER("test_data_integrity_and_type_conversion");
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("orm-test", ">>> Running test_data_integrity_and_type_conversion");

        auto users_ret = co_await Form<SimpleUser, PostgresTag>::create_if_not_exists(db, "simple_users");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        // 1. 边界值测试
        {
            PERF_TIMER("boundary_values");

            // 测试极大值
            auto insert_large = co_await users.emplace(1, "LargeBalance", 100, "large@test.com",
                                                      SqlDate(2024, 12, 31, 23, 59, 59), true, 999999999.99);
            CO_ASSERT_VAL(insert_large);

            auto verify_large = co_await users.select().where(users.sql(&SimpleUser::id) == 1).query();
            CO_ASSERT_VAL(verify_large);
            auto large_result = std::move(verify_large.value());

            ilias_for_await(auto &user, large_result.range()) {
                EXPECT_DOUBLE_EQ(user.balance, 999999999.99);
                EXPECT_EQ(user.created_at.year, 2024);
                EXPECT_EQ(user.created_at.month, 12);
                EXPECT_EQ(user.created_at.day, 31);
                EXPECT_EQ(user.created_at.hour, 23);
                EXPECT_EQ(user.created_at.minute, 59);
                EXPECT_EQ(user.created_at.second, 59);
            }

            // 测试零值
            auto insert_zero =
                co_await users.emplace(2, "ZeroBalance", 0, "zero@test.com", SqlDate(2024, 1, 1, 0, 0, 0), false, 0.0);
            CO_ASSERT_VAL(insert_zero);

            auto verify_zero = co_await users.select().where(users.sql(&SimpleUser::id) == 2).query();
            CO_ASSERT_VAL(verify_zero);
            auto zero_result = std::move(verify_zero.value());

            ilias_for_await(auto &user, zero_result.range()) {
                EXPECT_DOUBLE_EQ(user.balance, 0.0);
                EXPECT_EQ(user.age.value(), 0);
                EXPECT_FALSE(user.is_active);
            }
        }

        // 2. 特殊字符测试
        {
            PERF_TIMER("special_characters");

            // 测试包含特殊字符的字符串
            auto insert_special = co_await users.emplace(3, "User'With\"Special<>Chars", 25, "special@test.com",
                                                        SqlDate(2024, 6, 15), true, 100.0);
            CO_ASSERT_VAL(insert_special);

            auto verify_special = co_await users.select().where(users.sql(&SimpleUser::id) == 3).query();
            CO_ASSERT_VAL(verify_special);
            auto special_result = std::move(verify_special.value());

            ilias_for_await(auto &user, special_result.range()) {
                EXPECT_EQ(user.name, "User'With\"Special<>Chars");
            }
        }

        // 3. Unicode 字符测试
        {
            PERF_TIMER("unicode_characters");

            auto insert_unicode =
                co_await users.emplace(4, "用户名测试", 30, "unicode@test.com", SqlDate(2024, 6, 15), true, 200.0);
            CO_ASSERT_VAL(insert_unicode);

            auto verify_unicode = co_await users.select().where(users.sql(&SimpleUser::id) == 4).query();
            CO_ASSERT_VAL(verify_unicode);
            auto unicode_result = std::move(verify_unicode.value());

            ilias_for_await(auto &user, unicode_result.range()) {
                EXPECT_EQ(user.name, "用户名测试");
            }
        }

        ILIAS_INFO("orm-test", ">>> test_data_integrity_and_type_conversion PASSED");
        co_return {};
    }
};

// ==========================================
// 4. Google Test 集成
// ==========================================

// 基础CRUD测试
TEST(PostgresORMInterface, BasicCRUDOperations) {
    ORMInterfaceTestSuite::test_basic_crud_operations().wait();
}

// 高级查询测试
TEST(PostgresORMInterface, AdvancedQueryFeatures) {
    ORMInterfaceTestSuite::test_advanced_query_features().wait();
}

// 事务管理测试
TEST(PostgresORMInterface, TransactionManagement) {
    ORMInterfaceTestSuite::test_transaction_management().wait();
}

// 错误处理测试
TEST(PostgresORMInterface, ErrorHandling) {
    ORMInterfaceTestSuite::test_error_handling().wait();
}

// 类型转换器覆盖率测试
TEST(PostgresORMInterface, TypeConverterCoverage) {
    ORMInterfaceTestSuite::test_type_converter_coverage().wait();
}

// 数据正确性和类型转换测试
TEST(PostgresORMInterface, DataIntegrityAndTypeConversion) {
    ORMInterfaceTestSuite::test_data_integrity_and_type_conversion().wait();
}

// ==========================================
// 5. 主函数和初始化
// ==========================================
int main(int argc, char **argv) {
    // 初始化错误追踪
    cpptrace::init();

    // 设置日志级别
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ILIAS_LOG_ADD_WHITELIST("ilias-pgsql");
    ILIAS_LOG_ADD_WHITELIST("pgsql-test");
    ILIAS_LOG_ADD_WHITELIST("orm-test");

    // 初始化平台上下文
    ilias::PlatformContext ioContext;
    ioContext.install();

    // 初始化Google Test
    ::testing::InitGoogleTest(&argc, argv);

    // 输出测试环境信息
    ILIAS_INFO("orm-test", "=== PostgreSQL ORM Interface Test Environment ===");
    ILIAS_INFO("orm-test", "Platform: Linux");
    ILIAS_INFO("orm-test", "C++ Standard: {}", __cplusplus);

    // 检查环境变量
    const char *db_host = std::getenv("DB_HOST");
    const char *db_port = std::getenv("DB_PORT");
    const char *db_user = std::getenv("DB_USER");
    const char *db_name = std::getenv("DB_NAME");

    ILIAS_INFO("orm-test", "Database Configuration:");
    ILIAS_INFO("orm-test", "  Host: {}", db_host ? db_host : "localhost (default)");
    ILIAS_INFO("orm-test", "  Port: {}", db_port ? db_port : "5432 (default)");
    ILIAS_INFO("orm-test", "  User: {}", db_user ? db_user : "test (default)");
    ILIAS_INFO("orm-test", "  Database: {}", db_name ? db_name : "testdb (default)");

    ILIAS_INFO("orm-test", "================================================");

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
