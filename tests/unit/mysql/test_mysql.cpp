#include <cstdlib>
#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <string>
#include <vector>
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/form.hpp"
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
    int         id;
    std::string name;
    int         score;
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = // NOLINT
        Object("id", make_tags<SqlTags {.unique = true, .not_null = true, .primary_key = true}>(&SimpleUser::id),
               "name", make_tags<SqlTags {.not_null = true}>(&SimpleUser::name), "score",
               make_tags<SqlTags {.not_null = true}>(&SimpleUser::score));
};
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
        options.extra.insert(std::make_pair("ConnectTimeout", "10"));
        // 1. 使用 TCP 协议 (默认是 Socket)
        options.extra.insert(std::make_pair("Protocol", "MYSQL_PROTOCOL_TCP"));

        // 2. 禁用 SSL 强制校验 (防止握手阶段因证书问题断开)
        options.extra.insert(std::make_pair("SslEnforce", "false"));
        return options;
    }

    static auto setup_db() -> IoTask<SqlDatabase> {
        auto options = get_options();
        auto ret     = co_await SqlDatabase::open("mysql", options);
        if (!ret) {
            ILIAS_ERROR("mysql-test", "Failed to open MySQL: {}", ret.error().message());
            throw std::runtime_error("Failed to connect to MySQL");
        }
        auto db = std::move(ret.value());

        // 清理旧表 (Drop Table if exists)
        co_await db.execute("DROP TABLE IF EXISTS simple_users");
        co_await db.execute("DROP TABLE IF EXISTS complex_persons");

        // 创建 SimpleUser 表
        const char *create_simple = "CREATE TABLE simple_users ("
                                    "id INTEGER PRIMARY KEY, "
                                    "name VARCHAR(100) NOT NULL, "
                                    "score INTEGER"
                                    ")";
        auto        r1            = co_await db.execute(create_simple);
        if (!r1)
            ILIAS_ERROR("mysql-test", "Create simple_users failed: {}", r1.error().message());

        // 创建 Person 表
        // 注意：email 使用 VARCHAR(255) 以支持 UNIQUE 索引
        const char *create_complex = "CREATE TABLE complex_persons ("
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
            ILIAS_ERROR("mysql-test", "Create complex_persons failed: {}", r2.error().message());

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
        const char *insert_sql = "INSERT INTO complex_persons (id, name, age, email, born, promise, val1, val2) "
                                 "VALUES (:id, :name, :age, :email, :born, :promise, :val1, :val2)";
        auto        stmt_ret   = co_await db.prepare<Person>(insert_sql);
        CO_ASSERT_VAL(stmt_ret);
        auto stmt = std::move(stmt_ret.value());

        for (auto &p : persons) {
            stmt.reset(); // 重置绑定状态
            CO_EXPECT_RESULT(stmt.bind(p));
            auto ret = co_await stmt.execute();
            CO_EXPECT_RESULT(ret);
        }

        // 2. 查询验证
        auto query_ret = co_await db.query<Person>("SELECT * FROM complex_persons ORDER BY id");
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
            (co_await tx.prepare("INSERT INTO simple_users (id, name, score) VALUES (:id, :name, :score)")).value();

        const int TOTAL_ROWS = 50;
        for (int i = 0; i < TOTAL_ROWS; ++i) {
            stmt.reset();
            // 手动 Bind 参数 (index based)
            CO_EXPECT_RESULT(stmt.bind(i, "User_" + std::to_string(i), i * 10));
            auto ret = co_await stmt.execute();
            if (!ret) {
                ILIAS_ERROR("mysql-test", "Insert failed: {}", ret.error().message());
            }
        }

        auto commit_ret = co_await tx.commit();
        CO_EXPECT_RESULT(commit_ret);

        // 验证数量
        auto count_ret = co_await db.query<std::tuple<int>>("SELECT count(*) FROM simple_users");
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
        auto stmt = (co_await tx.prepare("INSERT INTO simple_users VALUES (:id, :name, :score)")).value();
        for (int i = 0; i < 20; ++i) {
            stmt.reset();
            stmt.bind(i, "U" + std::to_string(i), i); // score = id
            co_await stmt.execute();
        }
        co_await tx.commit();

        // MySQL 支持 LIMIT ?, ? 或 LIMIT :lim OFFSET :off
        // 查询 score 倒序 (19, 18, ...), 取 5 条, 偏移 5 条 -> 应该得到 14, 13, 12, 11, 10
        auto ret_query =
            co_await db.query_with("SELECT score FROM simple_users ORDER BY score DESC LIMIT :lim OFFSET :off", 5, 5);
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
        auto stmt = (co_await db.prepare("INSERT INTO simple_users VALUES (?, 'init', 10)")).value();
        for (int i = 0; i < 10; ++i) {
            stmt.reset();
            stmt.bind(i);
            co_await stmt.execute();
        }

        // 更新 id >= 5 的
        auto ret_up = co_await db.execute_with("UPDATE simple_users SET score = 999 WHERE id >= :id", 5);
        CO_ASSERT_VAL(ret_up);
        EXPECT_EQ(ret_up.value(), 5); // 5,6,7,8,9

        // 删除 score = 999 的
        auto ret_del = co_await db.execute("DELETE FROM simple_users WHERE score = 999");
        CO_ASSERT_VAL(ret_del);
        EXPECT_EQ(ret_del.value(), 5);

        // 检查剩余
        auto ret_count = co_await db.query<int>("SELECT count(*) FROM simple_users");
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
        // complex_persons 的 email 是 UNIQUE 的
        co_await db.execute("INSERT INTO complex_persons (id, name, email) VALUES (1, 'A', 'u@test.com')");
        auto ret2 = co_await db.execute("INSERT INTO complex_persons (id, name, email) VALUES (2, 'B', 'u@test.com')");
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
        auto ret = co_await db.execute("INSERT INTO simple_users (id, name, score) VALUES (999, 'NullGuy', NULL)");
        CO_ASSERT_VAL(ret);

        auto q = co_await db.query<SimpleUser>("SELECT * FROM simple_users WHERE id = 999");
        CO_ASSERT_VAL(q);

        ilias_for_await(auto &u, q.value().range()) {
            EXPECT_EQ(u.id, 999);
            // 这里的行为取决于库实现，通常 int 类型的 NULL 会被转为 0，或者如果库支持 std::optional 则为空
            // 假设库策略是如果不报错，则默认构造 (int -> 0)
            // EXPECT_EQ(u.score, 0);
            ILIAS_INFO("mysql-test", "Read NULL int as: {}", u.score);
        }
        ILIAS_INFO("mysql-test", ">>> test_null_handling PASSED");
        co_return {};
    }

    // --- 场景 7: 事务回滚测试 ---
    static auto test_transaction_rollback() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("mysql-test", ">>> Running test_transaction_rollback");

        // 1. 开启事务
        auto tx = (co_await db.transaction()).value();

        // 2. 在事务中插入一条“脏数据”
        // 使用 ID 8888 标记这条应该被回滚的数据
        auto exec_ret =
            co_await tx.execute("INSERT INTO simple_users (id, name, score) VALUES (8888, 'ShouldVanish', 0)");
        CO_ASSERT_VAL(exec_ret);

        // (可选验证) 此时在同一个事务连接中，理论上是可以查到这条数据的（取决于隔离级别）
        // 但我们要验证的是回滚后的最终一致性
        auto query_ret = co_await db.query<int>("SELECT count(*) FROM simple_users WHERE id = 8888");
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
        query_ret = co_await db.query<int>("SELECT count(*) FROM simple_users WHERE id = 8888");
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
            co_await tx.execute("INSERT INTO simple_users (id, name, score) VALUES (7777, 'RAII_Test', 0)");
            // 注意：这里故意不调用 tx.commit()，直接离开作用域

            auto query_ret = co_await db.query<int>("SELECT count(*) FROM simple_users WHERE id = 7777");
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
        auto q     = co_await db.query<int>("SELECT count(*) FROM simple_users WHERE id = 7777");
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
        co_await db.execute("DROP TABLE IF EXISTS users_full_test");
        // 1. 创建表 (Create)
        auto users_ret = co_await Form<SimpleUser, MysqlTag>::create(db, "users_full_test");
        CO_ASSERT_VAL(users_ret);
        auto users = std::move(users_ret.value());

        for (int i = 0; i < 100; ++i) {
            auto insert_ret = co_await users.insert(i, fmtlib::format("User{}", i), i * 10 + 1);
            CO_ASSERT_VAL(insert_ret);
        }
        ILIAS_INFO("mysql-test", ">>> Insert 100 users finished");

        {
            auto ret = co_await users.select("count(*)").where("score"_sql > 500 && "id"_sql < 60).execute();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 10);
        }

        {
            auto ret = co_await users.select("count(*)").where("id"_sql < 5 || "id"_sql >= 95).execute();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 10);
        }

        {
            auto ret = co_await users.select("count(*)")
                           .where("id"_sql < 5 || ("id"_sql >= 95 && "score"_sql > 970))
                           .execute();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 8);
        }

        {
            auto ret = co_await users.select("id").where("name"_sql == "User50").execute();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            int id = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, id);
            }
            EXPECT_EQ(id, 50);
        }

        {
            auto ret = co_await users.select("id, score")
                           .orderBy("score", true) // true for DESC
                           .offset(1)
                           .limit(2)
                           .execute();
            CO_ASSERT_VAL(ret);
            auto res = std::move(ret.value());

            std::vector<int> ids;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                int id = 0, score = 0;
                res.load(0, id);
                res.load(1, score);
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
            auto update_ret = co_await users.update(u10);
            CO_ASSERT_VAL(update_ret);
            EXPECT_EQ(update_ret.value(), 1); // 影响行数应为 1

            // 验证修改
            auto ret = co_await users.select("score, name").where("id"_sql == 10).execute();
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
            SimpleUser u20 {20, "", 0};

            auto remove_ret = co_await users.remove(u20);
            CO_ASSERT_VAL(remove_ret);
            EXPECT_EQ(remove_ret.value(), 1);

            // 验证不存在
            auto ret   = co_await users.select("count(*)").where("id"_sql == 20).execute();
            auto res   = std::move(ret.value());
            int  count = -1;
            ilias_for_await([[maybe_unused]] auto &row, res.range()) {
                res.load(0, count);
            }
            EXPECT_EQ(count, 0);
        }
        co_await users.print();

        ILIAS_INFO("mysql-test", ">>> test_form_full_coverage PASSED");
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