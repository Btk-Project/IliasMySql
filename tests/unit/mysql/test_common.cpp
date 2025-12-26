#include <cstdlib>
#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <string>
#include <vector>
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;

// ==========================================
// 1. 辅助宏 (与 SQLite 测试保持一致)
// ==========================================
#define CO_EXPECT_RESULT(result)                                                                                       \
    do {                                                                                                               \
        EXPECT_TRUE(result.has_value());                                                                               \
        if (!result.has_value()) {                                                                                     \
            ILIAS_ERROR("mysql-test", "failed: {}", result.error().message());                                         \
        }                                                                                                              \
    } while (0)

#define CO_EXPECT_NOT_RESULT(result)                                                                                   \
    do {                                                                                                               \
        EXPECT_FALSE(result.has_value());                                                                              \
        if (!result.has_value()) {                                                                                     \
            ILIAS_INFO("mysql-test", "expected failure: {}", result.error().message());                                \
        }                                                                                                              \
    } while (0)

#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value()) {                                                                                        \
            ILIAS_ERROR("mysql-test", "assert failed: {}", ret.error().message());                                     \
            co_return {};                                                                                              \
        }                                                                                                              \
        CO_EXPECT_RESULT(ret);                                                                                         \
    } while (0)

// ==========================================
// 2. 数据结构定义
// ==========================================

// 用于简单 CRUD 测试
struct SimpleUser {
    int                id;
    std::string        name;
    std::optional<int> score;
};

struct SimpleOrder {
    int         id      = 0;
    int         user_id = 0;
    int         amount  = 0;
    std::string product = "";
};

NEKO_BEGIN_NAMESPACE
// clang-format off
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = Object(
        "id",   make_tags<SqlTags {.primary_key = true, .not_null = true, .unique = true}>(&SimpleUser::id),
        "name", make_tags<SqlTags {.not_null = true}>(&SimpleUser::name), 
        "score",make_tags<SqlTags {.not_null = true}>(&SimpleUser::score));
};

template <>
struct Meta<SimpleOrder, void> {
    constexpr static auto value = Object(
        "id",       make_tags<SqlTags {.primary_key = true, .auto_increment = true}>(&SimpleOrder::id), 
        "user_id",  make_tags<SqlTags {.not_null = true}>(&SimpleOrder::user_id), 
        "amount",   make_tags<SqlTags {.not_null = true}>(&SimpleOrder::amount), 
        "product",  make_tags<SqlTags {.not_null = true}>(&SimpleOrder::product));
};
// clang-format on
NEKO_END_NAMESPACE

// 用于复杂类型测试 (Date, Blob, etc.)
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

// ==========================================
// 3. MySQL 测试套件
// ==========================================
class MySqlTestSuite {
public:
    // 辅助函数：从环境变量获取配置
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

        // MySQL 特有配置
        options.extra.insert(std::make_pair("InitCommand", "SET NAMES 'utf8mb4'"));
        options.extra.insert(std::make_pair("ConnectTimeout", "30"));
        // 增加读写超时，避免短暂网络波动导致查询中断
        options.extra.insert(std::make_pair("ReadTimeout", "30"));
        options.extra.insert(std::make_pair("WriteTimeout", "30"));
        // 1. 使用 TCP 协议 (默认是 Socket)
        options.extra.insert(std::make_pair("Protocol", "MYSQL_PROTOCOL_TCP"));

        // 2. 禁用 SSL 强制校验 (防止握手阶段因证书问题断开)
        options.extra.insert(std::make_pair("SslEnforce", "false"));
        options.extra.insert(std::make_pair("SslVerifyServerCert", "false"));
        options.extra.insert(std::make_pair("DefaultAuth", "mysql_native_password"));

        return options;
    }

    static auto setup_db() -> IoTask<SqlDatabase> {
        auto options = get_options();
        // 重试打开连接，缓解临时网络/服务抖动
        IoResult<SqlDatabase> open_ret     = Unexpected(SqlError::UnknownError);
        int                   attempts     = 0;
        const int             max_attempts = 3;
        while (attempts < max_attempts) {
            open_ret = co_await SqlDatabase::open("mysql", options);
            if (open_ret)
                break;
            attempts++;
            ILIAS_WARN("mysql-test", "MySQL open attempt {} failed: {}", attempts, open_ret.error().message());
        }
        if (!open_ret) {
            ILIAS_ERROR("mysql-test", "Failed to open MySQL after {} attempts: {}", max_attempts,
                        open_ret.error().message());
            throw std::runtime_error("Failed to connect to MySQL");
        }
        auto db = std::move(open_ret.value());

        // 清理旧表 (Drop Table if exists)
        co_await db.execute("DROP TABLE IF EXISTS common_simple_users");
        co_await db.execute("DROP TABLE IF EXISTS common_complex_persons");

        // 创建 SimpleUser 表
        const char *create_simple = "CREATE TABLE common_simple_users ("
                                    "id INTEGER PRIMARY KEY, "
                                    "name VARCHAR(100) NOT NULL, "
                                    "score INTEGER"
                                    ")";
        auto        r1            = co_await db.execute(create_simple);
        if (!r1)
            ILIAS_ERROR("mysql-test", "Create simple_users failed: {}", r1.error().message());

        // 创建 Person 表
        // 注意：email 使用 VARCHAR(255) 以支持 UNIQUE 索引
        const char *create_complex = "CREATE TABLE common_complex_persons ("
                                     "id INTEGER PRIMARY KEY, "
                                     "name VARCHAR(100) NOT NULL, "
                                     "age INTEGER, "
                                     "email VARCHAR(255) UNIQUE, "
                                     "born DATETIME, "
                                     "promise BLOB, "
                                     "val1 TINYINT, "
                                     "val2 INTEGER"
                                     ")";
        auto        r2             = co_await db.execute(create_complex);
        if (!r2)
            ILIAS_ERROR("mysql-test", "Create common_complex_persons failed: {}", r2.error().message());

        co_return db;
    }

    // --- 场景 1: 复杂类型映射 (SqlDate, Blob, Reflection) ---
    static auto test_complex_types() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_complex_types");

        std::vector<Person> persons = {
            {1,
             "Alice",
             18,
             "alice@test.com",
             SqlDate(2025, 6, 20, 10, 0, 0),
             {std::byte {0xDE}, std::byte {0xAD}},
             'A',
             100},
            {2, "Bob", 20, "bob@test.com", SqlDate(2024, 1, 1), {std::byte {0xBE}, std::byte {0xEF}}, 'B', 200}};

        // 1. 插入 (Prepare with Struct)
        const char *insert_sql = "INSERT INTO common_complex_persons (id, name, age, email, born, promise, val1, val2) "
                                 "VALUES (:id, :name, :age, :email, :born, :promise, :val1, :val2)";
        auto        stmt_ret   = co_await db.prepare<Person>(insert_sql);
        CO_ASSERT_VAL(stmt_ret);
        auto stmt = std::move(stmt_ret.value());

        for (auto &p : persons) {
            stmt.reset(); // 重置绑定状态
            auto bind_ret = stmt.bind(p);
            CO_EXPECT_RESULT(bind_ret);
            auto ret = co_await stmt.execute();
            CO_EXPECT_RESULT(ret);
        }

        // 2. 查询验证
        auto query_ret = co_await db.query<Person>("SELECT * FROM common_complex_persons ORDER BY id");
        CO_ASSERT_VAL(query_ret);
        auto result = std::move(query_ret.value());
        int  count  = 0;
        ilias_for_await(auto &p, result.range()) {
            const auto &expected = persons[count];
            EXPECT_EQ(p.id, expected.id);
            EXPECT_EQ(p.name, expected.name);
            EXPECT_EQ(p.email, expected.email);
            // 简单验证日期字符串
            EXPECT_EQ(p.born.toString(), expected.born.toString());
            // 验证 Blob
            EXPECT_EQ(p.promise.size(), expected.promise.size());
            EXPECT_EQ(p.promise[0], expected.promise[0]);
            count++;
        }
        EXPECT_EQ(count, 2);
        ILIAS_INFO("mysql-test", ">>> test_complex_types PASSED");
        co_return {};
    }

    // --- 场景 2: 事务与批量插入 ---
    static auto test_batch_insert_transaction() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_batch_insert_transaction");

        auto tx = (co_await db.transaction()).value();
        auto stmt =
            (co_await tx.prepare("INSERT INTO common_simple_users (id, name, score) VALUES (:id, :name, :score)"))
                .value();

        const int TOTAL_ROWS = 50;
        for (int i = 0; i < TOTAL_ROWS; ++i) {
            stmt.reset();
            // 手动 Bind 参数 (index based)
            auto bind_ret = stmt.bind(i, "User_" + std::to_string(i), i * 10);
            CO_EXPECT_RESULT(bind_ret);
            auto ret = co_await stmt.execute();
            if (!ret) {
                ILIAS_ERROR("mysql-test", "Insert failed: {}", ret.error().message());
            }
        }

        auto commit_ret = co_await tx.commit();
        CO_EXPECT_RESULT(commit_ret);

        // 验证数量
        auto count_ret = co_await db.query<std::tuple<int>>("SELECT count(*) FROM common_simple_users");
        int  count     = 0;
        ilias_for_await(auto val, count_ret.value().range()) {
            count = std::get<0>(val);
        }
        EXPECT_EQ(count, TOTAL_ROWS);
        ILIAS_INFO("mysql-test", ">>> test_batch_insert_transaction PASSED");
        co_return {};
    }

    // --- 场景 3: 分页查询 (LIMIT/OFFSET) ---
    static auto test_pagination() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_pagination");

        // 插入 20 条数据
        auto tx   = (co_await db.transaction()).value();
        auto stmt = (co_await tx.prepare("INSERT INTO common_simple_users VALUES (:id, :name, :score)")).value();
        for (int i = 0; i < 20; ++i) {
            stmt.reset();
            stmt.bind(i, "U" + std::to_string(i), i); // score = id
            co_await stmt.execute();
        }
        co_await tx.commit();

        // MySQL 支持 LIMIT ?, ? 或 LIMIT :lim OFFSET :off
        // 查询 score 倒序 (19, 18, ...), 取 5 条, 偏移 5 条 -> 应该得到 14, 13, 12, 11, 10
        auto ret_query = co_await db.query_with(
            "SELECT score FROM common_simple_users ORDER BY score DESC LIMIT :lim OFFSET :off", 5, 5);
        CO_ASSERT_VAL(ret_query);

        std::vector<int> scores;
        int              val;
        ilias_for_await([[maybe_unused]] auto rc, ret_query.value().range()) {
            ret_query.value().load(0, val);
            scores.push_back(val);
        }

        EXPECT_EQ(scores.size(), 5);
        if (!scores.empty()) {
            EXPECT_EQ(scores[0], 14);
            EXPECT_EQ(scores[4], 10);
        }
        ILIAS_INFO("mysql-test", ">>> test_pagination PASSED");
        co_return {};
    }

    // --- 场景 4: 批量更新与删除 ---
    static auto test_bulk_update_delete() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_bulk_update_delete");

        // 准备数据
        auto stmt = (co_await db.prepare("INSERT INTO common_simple_users VALUES (?, 'init', 10)")).value();
        for (int i = 0; i < 10; ++i) {
            stmt.reset();
            stmt.bind(i);
            co_await stmt.execute();
        }

        // 更新 id >= 5 的
        auto ret_up = co_await db.execute_with("UPDATE common_simple_users SET score = 999 WHERE id >= :id", 5);
        CO_ASSERT_VAL(ret_up);
        EXPECT_EQ(ret_up.value(), 5); // 5,6,7,8,9

        // 删除 score = 999 的
        auto ret_del = co_await db.execute("DELETE FROM common_simple_users WHERE score = 999");
        CO_ASSERT_VAL(ret_del);
        EXPECT_EQ(ret_del.value(), 5);

        // 检查剩余
        auto ret_count = co_await db.query<int>("SELECT count(*) FROM common_simple_users");
        int  count     = 0;
        ilias_for_await(auto v, ret_count.value().range()) {
            count = std::get<0>(v);
        }
        EXPECT_EQ(count, 5); // 0,1,2,3,4 还在

        ILIAS_INFO("mysql-test", ">>> test_bulk_update_delete PASSED");
        co_return {};
    }

    // --- 场景 5: 错误处理 ---
    static auto test_errors() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_errors");

        // 1. 语法错误
        auto ret1 = co_await db.execute("SELECT * FROM non_existent_table_xyz");
        CO_EXPECT_NOT_RESULT(ret1); // 应该报错

        // 2. 唯一键冲突
        // common_complex_persons 的 email 是 UNIQUE 的
        co_await db.execute("INSERT INTO common_complex_persons (id, name, email) VALUES (1, 'A', 'u@test.com')");
        auto ret2 =
            co_await db.execute("INSERT INTO common_complex_persons (id, name, email) VALUES (2, 'B', 'u@test.com')");
        CO_EXPECT_NOT_RESULT(ret2); // 应该报错
        // ILIAS_INFO("mysql-test", "Expected error: {}", ret2.error().message());

        ILIAS_INFO("mysql-test", ">>> test_errors PASSED");
        co_return {};
    }

    // --- 场景 6: NULL 值处理 ---
    static auto test_null_handling() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_null_handling");

        // 插入 score 为 NULL
        auto ret =
            co_await db.execute("INSERT INTO common_simple_users (id, name, score) VALUES (999, 'NullGuy', NULL)");
        CO_ASSERT_VAL(ret);

        auto q = co_await db.query<SimpleUser>("SELECT * FROM common_simple_users WHERE id = 999");
        CO_ASSERT_VAL(q);

        ilias_for_await(auto &u, q.value().range()) {
            EXPECT_EQ(u.id, 999);
            EXPECT_FALSE(u.score.has_value());
            ILIAS_INFO("mysql-test", "Read NULL int as: {}", u.score.has_value());
        }
        ILIAS_INFO("mysql-test", ">>> test_null_handling PASSED");
        co_return {};
    }

    // --- 场景 7: 事务回滚测试 ---
    static auto test_transaction_rollback() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_transaction_rollback");

        // 开启事务
        auto tx = (co_await db.transaction()).value();

        // 在事务中插入一条“脏数据”
        // 使用 ID 8888 标记这条应该被回滚的数据
        auto exec_ret =
            co_await tx.execute("INSERT INTO common_simple_users (id, name, score) VALUES (8888, 'ShouldVanish', 0)");
        CO_ASSERT_VAL(exec_ret);

        // 此时在同一个事务连接中，理论上是可以查到这条数据的（取决于隔离级别）
        // 但我们要验证的是回滚后的最终一致性
        auto query_ret = co_await db.query<int>("SELECT count(*) FROM common_simple_users WHERE id = 8888");
        CO_ASSERT_VAL(query_ret);

        int count = -1;
        ilias_for_await(auto val, query_ret.value().range()) {
            count = std::get<0>(val);
        }
        // 期望数量为 1，说明插入成功
        EXPECT_EQ(count, 1);

        // 3. 执行回滚
        auto rb_ret = co_await tx.rollback();
        EXPECT_TRUE(rb_ret);

        // 4. 验证数据确实不存在了
        query_ret = co_await db.query<int>("SELECT count(*) FROM common_simple_users WHERE id = 8888");
        CO_ASSERT_VAL(query_ret);

        count = -1;
        ilias_for_await(auto val, query_ret.value().range()) {
            count = std::get<0>(val);
        }

        // 期望数量为 0，说明插入被撤销了
        EXPECT_EQ(count, 0);

        ILIAS_INFO("mysql-test", ">>> test_transaction_rollback PASSED");
        co_return {};
    }

    // --- 场景 8: 析构自动回滚测试 ---
    static auto test_raii_rollback() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_raii_rollback");

        {
            // 作用域开始
            auto tx = (co_await db.transaction()).value();
            co_await tx.execute("INSERT INTO common_simple_users (id, name, score) VALUES (7777, 'RAII_Test', 0)");
            // 注意：这里故意不调用 tx.commit()，直接离开作用域

            auto query_ret = co_await db.query<int>("SELECT count(*) FROM common_simple_users WHERE id = 7777");
            CO_ASSERT_VAL(query_ret);

            int count = -1;
            ilias_for_await(auto val, query_ret.value().range()) {
                count = std::get<0>(val);
            }
            // 期望数量为 1，说明插入成功
            EXPECT_EQ(count, 1);
        }
        // 此时 tx 被析构，应该触发 syncRollback 或类似的机制

        // 验证数据不存在
        auto q     = co_await db.query<int>("SELECT count(*) FROM common_simple_users WHERE id = 7777");
        int  count = 0;
        ilias_for_await(auto val, q.value().range()) {
            count = std::get<0>(val);
        }
        EXPECT_EQ(count, 0);

        ILIAS_INFO("mysql-test", ">>> test_raii_rollback PASSED");
        co_return {};
    }
    static auto test_form_interface() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_form_full_coverage");
        // 如果存在上次的测试数据，先清空
        co_await db.execute("DROP TABLE IF EXISTS common_users_full_test");
        // 1. 创建表 (Create)
        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "common_users_full_test");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        for (int i = 0; i < 100; ++i) {
            auto insert_ret = co_await users.insert(i, fmtlib::format("User{}", i), i * 10 + 1);
            CO_ASSERT_VAL(insert_ret);
        }
        ILIAS_INFO("mysql-test", ">>> Insert 100 users finished");

        {
            auto ret = co_await users.count()
                           .where(users.sql(&SimpleUser::score) > 500 && users.sql(&SimpleUser::id) < 60)
                           .query();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 10);
        }

        {
            auto ret = co_await users.count().where("id"_sql < 5 || "id"_sql >= 95).query();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 10);
        }

        {
            auto ret = co_await users.count()
                           .where(users.sql(&SimpleUser::id) < 5 ||
                                  (users.sql(&SimpleUser::id) >= 95 && users.sql(&SimpleUser::score) > 970))
                           .query();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 8);
        }

        {
            auto ret = co_await users.select("id").where("name"_sql == "User50").query();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int id = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, id);
            }
            EXPECT_EQ(id, 50);
        }

        {
            auto ret = co_await users.select(users.sql(&SimpleUser::id), users.sql(&SimpleUser::score))
                           .orderBy("score", true) // true for DESC
                           .offset(1)
                           .limit(2)
                           .query();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            std::vector<int> ids;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                auto [id, score] = row;
                ids.push_back(id);
            }

            EXPECT_EQ(ids.size(), 2);
            if (ids.size() == 2) {
                EXPECT_EQ(ids[0], 98);
                EXPECT_EQ(ids[1], 97);
            }
        }

        {
            // 将 ID=10 的用户分数修改为 9999
            SimpleUser u10 {10, "User10_Modified", 9999};

            // 注意：Update 依赖 Form 能够正确识别 Primary Key
            auto update_ret =
                co_await users.update()
                    .set(users.sql(&SimpleUser::score) = u10.score, users.sql(&SimpleUser::name) = u10.name)
                    .where(users.sql(&SimpleUser::id) == u10.id)
                    .execute();
            CO_ASSERT_VAL(update_ret);
            EXPECT_EQ(update_ret.value(), 1); // 影响行数应为 1

            // 验证修改
            auto ret = co_await users.select("score, name").where(users.sql(&SimpleUser::id) == 10).query();
            auto res = std::move(ret.value());

            int         score = 0;
            std::string name;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, score);
                res.load(1, name);
            }
            EXPECT_EQ(score, 9999);
            EXPECT_EQ(name, "User10_Modified");
        }

        {
            // 删除 ID=20 的用户

            auto remove_ret = co_await users.remove().where(users.sql(&SimpleUser::id) == 20).execute();
            CO_ASSERT_VAL(remove_ret);
            EXPECT_EQ(remove_ret.value(), 1);

            // 验证不存在
            auto ret   = co_await users.count().where(users.sql(&SimpleUser::id) == 20).query();
            auto res   = std::move(ret.value());
            int  count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 0);
        }

        {
            // 连续查询ID == (20, 40) 的用户
            int id    = 20;
            int count = 20;

            ilias_for_await(auto &ret, users.select().where(users.sql(&SimpleUser::id) == id).loop(count)) {
                CO_ASSERT_VAL(ret);
                SqlResult<SimpleUser> res = std::move(ret.value());
                ilias_for_await(auto &user, res.range()) {
                    EXPECT_EQ(user.id, id);
                    EXPECT_EQ(user.name, "User" + std::to_string(id));
                    EXPECT_EQ(user.score, id * 10 + 1);
                }
                id++;
            }
        }
        co_await users.print();

        ILIAS_INFO("mysql-test", ">>> test_form_full_coverage PASSED");
        co_return {};
    }

    static auto test_join_features() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_join_features");

        // 1. 清理并创建表
        co_await db.execute("DROP TABLE IF EXISTS common_test_users_join");
        co_await db.execute("DROP TABLE IF EXISTS common_test_orders_join");

        auto users_ret  = co_await Form<SimpleUser, MysqlTag>::create(db, "common_test_users_join");
        auto orders_ret = co_await Form<SimpleOrder, MysqlTag>::create(db, "common_test_orders_join");
        CO_ASSERT_VAL(users_ret);
        CO_ASSERT_VAL(orders_ret);

        auto users  = std::move(users_ret.value());
        auto orders = std::move(orders_ret.value());

        // 2. 准备数据
        // 用户: 1(Alice), 2(Bob), 3(Charlie)
        co_await users.insert(1, "Alice", 100);
        co_await users.insert(2, "Bob", 200);
        co_await users.insert(3, "Charlie", 50);

        // 订单: Alice买了两单，Bob买了一单，Charlie没买
        co_await orders.insert(101, 1, 500, "Apple"); // Alice
        co_await orders.insert(102, 1, 50, "Banana"); // Alice
        co_await orders.insert(103, 2, 900, "TV");    // Bob

        ILIAS_INFO("mysql-test", ">>> Data prepared");

        // =========================================================
        // 测试场景 1: 别名(Alias) + 投影(Projection) + 过滤(Where)
        // SQL 意图:
        // SELECT u.name, o.product, o.amount
        // FROM users AS u
        // JOIN orders AS o ON u.id = o.user_id
        // WHERE o.amount > 100
        // =========================================================
        {
            // 1. 定义别名
            auto u = users.as("u");
            auto o = orders.as("o");

            // 2. 构建查询
            // 注意：select() 中的参数决定了 execute() 返回的 tuple 类型
            // 返回类型推导为: std::vector<std::tuple<std::string, std::string, int>>
            auto ret = co_await u.join(o)
                           .on(u.col(&SimpleUser::id) == o.col(&SimpleOrder::user_id)) // ON u.id = o.user_id
                           .select(u.col(&SimpleUser::name), o.col(&SimpleOrder::product), o.col(&SimpleOrder::amount))
                           .where(o.col(&SimpleOrder::amount) > 100) // WHERE o.amount > 100
                           .query();

            CO_ASSERT_VAL(ret);
            auto result = std::move(ret.value());

            using resultType                  = std::tuple<std::string, std::string, int>;
            std::vector<resultType> true_rows = {{"Alice", "Apple", 500}, {"Bob", "TV", 900}};
            int                     idx       = 0;
            ilias_for_await(auto &row, result.range()) {
                if (idx >= (int)true_rows.size()) {
                    ILIAS_ERROR("mysql-test", ">>> test_join_features FAILED: result size mismatch");
                    co_return {};
                }
                // 验证第一行 (假设顺序保持插入顺序，或数据库默认排序)
                // 使用结构化绑定解包 tuple
                const auto &[name1, prod1, amt1] = row;
                const auto &[name2, prod2, amt2] = true_rows[idx++];
                EXPECT_EQ(name1, name2);
                EXPECT_EQ(prod1, prod2);
                EXPECT_EQ(amt1, amt2);
            }
        }

        // =========================================================
        // 测试场景 2: 获取完整对象 (Select *)
        // SQL 意图: SELECT * FROM users JOIN orders ...
        // =========================================================
        {
            // 如果不调用 select()，默认返回参与 Join 的所有 Form 对应的实体对象
            // 返回类型推导为: std::vector<std::tuple<SimpleUser, SimpleOrder>>
            auto ret = co_await users.join(orders)
                           .on(users.col(&SimpleUser::id) == orders.col(&SimpleOrder::user_id))
                           .where(users.col(&SimpleUser::name) == "Alice")
                           .query();

            CO_ASSERT_VAL(ret);
            auto result = std::move(ret.value());

            using resultType                  = std::tuple<SimpleUser, SimpleOrder>;
            std::vector<resultType> true_rows = {{SimpleUser {1, "Alice", 100}, SimpleOrder {101, 1, 500, "Apple"}},
                                                 {SimpleUser {1, "Alice", 100}, SimpleOrder {102, 1, 50, "Banana"}}};
            int                     idx       = 0;
            ilias_for_await(auto &row, result.range()) {
                if (idx >= (int)true_rows.size()) {
                    ILIAS_ERROR("mysql-test", ">>> test_join_features FAILED: result size mismatch");
                    co_return {};
                }
                // 验证第一行 (假设顺序保持插入顺序，或数据库默认排序)
                // 使用结构化绑定解包 tuple
                const auto &[user, order]           = row;
                const auto &[true_user, true_order] = true_rows[idx++];
                EXPECT_EQ(user.id, true_user.id);
                EXPECT_EQ(user.name, true_user.name);
                EXPECT_EQ(order.id, true_order.id);
                EXPECT_EQ(order.user_id, true_order.user_id);
                EXPECT_EQ(order.amount, true_order.amount);
                EXPECT_EQ(order.product, true_order.product);
            }
        }

        ILIAS_INFO("mysql-test", ">>> test_join_features PASSED");
        co_return {};
    }
};

// ==========================================
// 4. 执行入口
// ==========================================
ILIAS_NAMESPACE::Task<void> run_all_tests() {
    try {
        co_await MySqlTestSuite::test_complex_types();
        co_await MySqlTestSuite::test_batch_insert_transaction();
        co_await MySqlTestSuite::test_pagination();
        co_await MySqlTestSuite::test_bulk_update_delete();
        co_await MySqlTestSuite::test_errors();
        co_await MySqlTestSuite::test_null_handling();
        co_await MySqlTestSuite::test_transaction_rollback();
        co_await MySqlTestSuite::test_raii_rollback();
        co_await MySqlTestSuite::test_form_interface();
        co_await MySqlTestSuite::test_join_features();
    } catch (const std::exception &e) {
        ILIAS_ERROR("mysql-test", "Exception caught: {}", e.what());
        EXPECT_TRUE(false) << "Exception in runner: " << e.what();
    }
}

TEST(SQL, MySqlSuite) {
    run_all_tests().wait();
}

int main(int argc, char **argv) {
    cpptrace::init();
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}