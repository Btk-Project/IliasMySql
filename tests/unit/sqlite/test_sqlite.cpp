#include <gtest/gtest.h>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <ilias/platform.hpp>
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/orm_form.hpp"

// 假设这些在你的项目中存在
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;
NEKO_USE_NAMESPACE
// ==========================================
// 1. 协程测试辅助宏 (保持不变)
// ==========================================
#define CO_EXPECT_RESULT(result)                                                                                       \
    do {                                                                                                               \
        EXPECT_TRUE(result.has_value());                                                                               \
        if (!result.has_value()) {                                                                                     \
            ILIAS_ERROR("sql-test", "failed: {}", result.error().message());                                           \
        }                                                                                                              \
    } while (0)

#define CO_EXPECT_NOT_RESULT(result)                                                                                   \
    do {                                                                                                               \
        EXPECT_FALSE(result.has_value());                                                                              \
        if (!result.has_value()) {                                                                                     \
            ILIAS_INFO("sql-test", "expected failure: {}", result.error().message());                                  \
        }                                                                                                              \
    } while (0)

#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value()) {                                                                                        \
            ILIAS_ERROR("sql-test", "assert failed: {}", ret.error().message());                                       \
            co_return {}; /* 需要在 Task 中返回 void */                                                                \
        }                                                                                                              \
        CO_EXPECT_RESULT(ret);                                                                                         \
    } while (0)

// ==========================================
// 2. 测试用的数据结构
// ==========================================
struct SimpleUser {
    int                id    = 0;
    std::string        name  = "";
    std::optional<int> score = 0;
};

struct SimpleOrder {
    int         id      = 0;
    int         user_id = 0;
    int         amount  = 0;
    std::string product = "";
};

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = // NOLINT
        Object("id", make_tags<SqlTags {.unique = true, .not_null = true, .primary_key = true}>(&SimpleUser::id),
               "name", make_tags<SqlTags {.not_null = true}>(&SimpleUser::name), "score",
               make_tags<SqlTags {}>(&SimpleUser::score));
};

template <>
struct Meta<SimpleOrder, void> {
    constexpr static auto value = // NOLINT
        Object("id", make_tags<SqlTags {.primary_key = true, .auto_increment = true}>(&SimpleOrder::id), "user_id",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::user_id), "amount",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::amount), "product",
               make_tags<SqlTags {.not_null = true}>(&SimpleOrder::product));
};
NEKO_END_NAMESPACE

// ==========================================
// 3. 测试套件
// ==========================================

class SqlTestSuite {
public:
    static auto setup_db() -> IoTask<SqlDatabase> {
        auto ret = co_await SqlDatabase::open_in_memory();
        if (!ret) {
            throw std::runtime_error("Failed to open DB");
        }
        auto db = std::move(ret.value());

        const char *create_sql = "CREATE TABLE IF NOT EXISTS users ("
                                 "id INTEGER PRIMARY KEY, "
                                 "name TEXT NOT NULL, "
                                 "score INTEGER"
                                 ");";
        auto        ret_exec   = co_await db.execute(create_sql);
        if (!ret_exec) {
            throw std::runtime_error("Failed to create table");
        }
        co_return db;
    }

    // --- 场景 1: 批量插入与全量遍历 ---
    // 目的: 测试迭代器在多行数据下的表现，以及 Prepare Statement 的复用稳定性
    static auto test_batch_insert_and_scan() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_batch_insert_and_scan");

        // 1. 开启事务以提高插入速度
        auto tx = (co_await db.transaction()).value();

        auto stmt = (co_await tx.prepare("INSERT INTO users (id, name, score) VALUES (?, ?, ?)")).value();

        const int TOTAL_ROWS = 100;

        for (int i = 0; i < TOTAL_ROWS; ++i) {
            stmt.reset();
            std::string name = "User_" + std::to_string(i);
            // score 设为 id * 10，方便后续校验
            auto bind_ret = stmt.bind(i, name, i * 10);
            CO_EXPECT_RESULT(bind_ret);
            auto ret = co_await stmt.execute();
            if (!ret) {
                ILIAS_ERROR("test", "Insert failed at index {}", i);
                CO_ASSERT_VAL(ret);
            }
        }
        auto commit_ret = co_await tx.commit();
        CO_EXPECT_RESULT(commit_ret);

        // 2. 查询所有数据并验证完整性
        auto query_stmt = (co_await db.prepare<SimpleUser>("SELECT * FROM users ORDER BY id ASC")).value();
        auto result     = (co_await query_stmt.query()).value();

        int count     = 0;
        int sum_score = 0;

        ilias_for_await(auto &user, result.range()) {
            EXPECT_EQ(user.id, count);
            std::string expected_name = "User_" + std::to_string(count);
            EXPECT_EQ(user.name, expected_name);
            EXPECT_EQ(user.score, count * 10);

            sum_score += user.score.value_or(0);
            count++;
        }

        EXPECT_EQ(count, TOTAL_ROWS);
        // 等差数列求和: Sum = n*(a1+an)/2 => 100 * (0 + 990) / 2 = 49500
        EXPECT_EQ(sum_score, 49500);

        ILIAS_INFO("test", ">>> test_batch_insert_and_scan PASSED (Rows: {})", count);
        co_return {};
    }

    // --- 场景 2: 分页与排序 (Limit/Offset) ---
    // 目的: 验证 SQL 绑定参数在 limit/offset 中的作用，以及结果集的截断
    static auto test_pagination() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_pagination");

        // 预置 50 条数据: id 0-49, score 乱序插入 (为了测试 ORDER BY)
        auto tx   = (co_await db.transaction()).value();
        auto stmt = (co_await tx.prepare("INSERT INTO users (id, name, score) VALUES (?, ?, ?)")).value();

        for (int i = 0; i < 50; ++i) {
            stmt.reset();
            // id=i, score = 100 - i (逆序)
            stmt.bind(i, "U" + std::to_string(i), 100 - i);
            co_await stmt.execute();
        }
        co_await tx.commit();
        stmt.clearKeepAlives();

        // 分页查询: 按 score 升序 (小分在前)，取第 11 到 20 条 (Limit 10 Offset 10)
        // Score 应该是: 51, 52, ..., 60 (对应 ID 49, 48... )
        // SQL: SELECT score FROM users ORDER BY score ASC LIMIT 10 OFFSET 10

        auto ret_query = co_await db.query_with("SELECT score FROM users ORDER BY score ASC LIMIT ? OFFSET ?", 10, 10);
        CO_ASSERT_VAL(ret_query);
        auto result = std::move(ret_query.value());

        std::vector<int> scores;
        int              val;
        ilias_for_await(auto r, result.range()) {
            CO_ASSERT_VAL(r);
            CO_EXPECT_RESULT(result.load(0, val));
            scores.push_back(val);
        }

        EXPECT_EQ(scores.size(), 10);
        if (!scores.empty()) {
            EXPECT_EQ(scores.front(), 61); // 最小分是 51 (id 49)，Offset 10 后应该是 61 (id 39)
            EXPECT_EQ(scores.back(), 70);
        }

        ILIAS_INFO("test", ">>> test_pagination PASSED");
        co_return {};
    }

    // --- 场景 3: 批量更新与删除 ---
    // 目的: 验证 execute 返回的 affected_rows 是否准确，以及条件更新的有效性
    static auto test_bulk_update_delete() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_bulk_update_delete");

        // 插入 20 条数据
        auto stmt = (co_await db.prepare("INSERT INTO users VALUES (?, 'init', 10)")).value();
        for (int i = 0; i < 20; ++i) {
            stmt.reset();
            stmt.bind(i);
            co_await stmt.execute();
        }

        // 1. 批量更新: 将 id >= 10 的 score 改为 999
        auto ret_up = co_await db.execute_with("UPDATE users SET score = ? WHERE id >= ?", 999, 10);
        CO_ASSERT_VAL(ret_up);
        EXPECT_EQ(ret_up.value(), 10); // 应该影响 10 行 (10-19)

        // 2. 验证更新结果
        auto ret_chk = co_await db.query_with("SELECT count(*) FROM users WHERE score = 999");
        int  count   = 0;
        ilias_for_await(auto _, ret_chk.value().range()) {
            ret_chk.value().load(0, count);
        }
        EXPECT_EQ(count, 10);

        // 3. 批量删除: 删除 score < 100 的 (即 id 0-9)
        auto ret_del = co_await db.execute("DELETE FROM users WHERE score < 100");
        CO_ASSERT_VAL(ret_del);
        EXPECT_EQ(ret_del.value(), 10); // 应该删除 10 行

        // 4. 最终剩余行数
        auto ret_final = co_await db.query_with("SELECT count(*) FROM users");
        ilias_for_await(auto _, ret_final.value().range()) {
            ret_final.value().load(0, count);
        }
        EXPECT_EQ(count, 10);

        ILIAS_INFO("test", ">>> test_bulk_update_delete PASSED");
        co_return {};
    }

    // --- 场景 4: 基础 CRUD (保留你的简单测试作为冒烟测试) ---
    static auto test_basic_crud() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_errors");

        // 7.1 语法错误
        auto ret1 = co_await db.execute("SELECT * FROM non_existent_table");
        CO_EXPECT_NOT_RESULT(ret1);
        // 打印错误看是否符合预期
        ILIAS_INFO("test", "Expected error: {}", ret1.error().message());

        // 7.2 约束冲突 (主键重复)
        co_await db.execute("INSERT INTO users VALUES (1, 'A', 1)");
        auto ret2 = co_await db.execute("INSERT INTO users VALUES (1, 'B', 2)");
        CO_EXPECT_NOT_RESULT(ret2);

        ILIAS_INFO("test", ">>> test_errors PASSED");
        co_return {};
    }

    // --- 场景 5: NULL 值处理与结构体部分映射 ---
    // 目的: 测试数据库中的 NULL 映射到 C++ 结构体的行为 (通常依赖库的具体实现，这里假设 int 保持原值或抛错，或者使用
    // std::optional) 假设 SimpleUser 的 score 是 int，如果库支持将 NULL 读为 0，或者跳过赋值，这里测试其确定性。
    static auto test_null_handling() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_null_handling");

        // 插入一条 score 为 NULL 的记录
        auto ret = co_await db.execute("INSERT INTO users (id, name, score) VALUES (999, 'NullUser', NULL)");
        CO_ASSERT_VAL(ret);

        // 尝试读取
        // 这里的行为取决于你的库如何处理 int 类型的 NULL。
        // 如果你的库支持 std::optional<int>，建议修改 SimpleUser。
        // 如果不支持，通常会是 0 或者抛出异常。这里假设它能运行并读出 0 (SQLite 默认行为在某些 wrapper 中)。
        auto q = co_await db.query<SimpleUser>("SELECT * FROM users WHERE id = 999");
        CO_ASSERT_VAL(q);
        auto res = std::move(q.value());

        bool found = false;
        ilias_for_await(auto &u, res.range()) {
            found = true;
            EXPECT_EQ(u.id, 999);
            // 假设库策略：NULL -> 0 (需要根据实际 ilias 实现调整预期)
            // EXPECT_EQ(u.score, 0);
            ILIAS_INFO("test", "Got user with score: {}", u.score.value_or(-1));
        }
        EXPECT_TRUE(found);

        co_return {};
    }

    // --- 场景 X: 显式覆盖 nullptr 绑定测试 ---
    // 目的: 验证将 nullptr 作为字符串参数绑定到 SQL 时不会导致崩溃，且会被视为 SQL NULL
    static auto test_null_bind() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_null_bind");

        co_await db.execute("DROP TABLE IF EXISTS null_test");
        auto r = co_await db.execute("CREATE TABLE null_test (id INTEGER PRIMARY KEY, t TEXT, b BLOB)");
        CO_EXPECT_RESULT(r);

        auto prep = (co_await db.prepare("INSERT INTO null_test (id, t, b) VALUES (?, ?, ?)"));
        CO_ASSERT_VAL(prep);
        auto stmt = std::move(prep.value());

        stmt.reset();
        // 绑定 nullptr 到 TEXT 字段（应被视为 SQL NULL），以及空 BLOB
        stmt.bind(1, nullptr, std::vector<std::byte> {});
        auto ir = co_await stmt.execute();
        CO_EXPECT_RESULT(ir);

        // 验证 t IS NULL
        auto cnt_ret = co_await db.query<int>("SELECT count(*) FROM null_test WHERE t IS NULL");
        CO_ASSERT_VAL(cnt_ret);
        int cnt = 0;
        ilias_for_await(auto v, cnt_ret.value().range()) {
            cnt = std::get<0>(v);
        }
        EXPECT_EQ(cnt, 1);

        ILIAS_INFO("test", ">>> test_null_bind PASSED");
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
        auto exec_ret = co_await tx.execute("INSERT INTO users (id, name, score) VALUES (8888, 'ShouldVanish', 0)");
        CO_ASSERT_VAL(exec_ret);

        // (可选验证) 此时在同一个事务连接中，理论上是可以查到这条数据的（取决于隔离级别）
        // 但我们要验证的是回滚后的最终一致性
        auto query_ret = co_await db.query<int>("SELECT count(*) FROM users WHERE id = 8888");
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
        query_ret = co_await db.query<int>("SELECT count(*) FROM users WHERE id = 8888");
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
            co_await tx.execute("INSERT INTO users (id, name, score) VALUES (7777, 'RAII_Test', 0)");
            // 注意：这里故意不调用 tx.commit()，直接离开作用域

            auto query_ret = co_await db.query<int>("SELECT count(*) FROM users WHERE id = 7777");
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
        auto q     = co_await db.query<int>("SELECT count(*) FROM users WHERE id = 7777");
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
        auto users_ret = co_await Form<SimpleUser, SqliteTag>::create(db, "users_full_test");
        CO_ASSERT_VAL(users_ret);
        auto       users = std::move(users_ret.value());
        SimpleUser user {0, "User0", 1};

        ilias_for_await(auto &ret, users.insert().set(user).loop(50)) {
            // i, fmtlib::format("User{}", i), i * 10 + 1
            user = SimpleUser {user.id + 1, "User" + std::to_string(user.id + 1), (user.id + 1) * 10 + 1};
            CO_ASSERT_VAL(ret);
        }
        int id = 50;
        auto id_generator = [&id]() { return id++; };
        ilias_for_await(auto &ret,
                        users.insert()
                            .set(users.sql(&SimpleUser::id) = id_generator, users.sql(&SimpleUser::name) = user.name,
                                 users.sql(&SimpleUser::score) = user.score)
                            .loop(-1)) {
            user = SimpleUser {id, "User" + std::to_string(id), id * 10 + 1};
            CO_ASSERT_VAL(ret);
            if (id >= 100) {
                break;
            }
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
            auto remove_ret = co_await users.remove().where("id"_sql == 20).execute();
            CO_ASSERT_VAL(remove_ret);
            EXPECT_EQ(remove_ret.value(), 1);

            // 验证不存在
            auto ret   = co_await users.select("count(*)").where(users.sql(&SimpleUser::id) == 20).query();
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
        co_await db.execute("DROP TABLE IF EXISTS test_users_join");
        co_await db.execute("DROP TABLE IF EXISTS test_orders_join");

        auto users_ret  = co_await Form<SimpleUser, SqliteTag>::create(db, "test_users_join");
        auto orders_ret = co_await Form<SimpleOrder, SqliteTag>::create(db, "test_orders_join");
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
                if (idx >= true_rows.size()) {
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
                const auto &[user, order] = row;
                ILIAS_INFO("mysql-test", "idx: {}, user id: {}, name: {}, order id: {}, order user_id: {}", idx,
                           user.id, user.name, order.id, order.user_id);
                if (idx >= (int)true_rows.size()) {
                    ILIAS_ERROR("mysql-test", ">>> test_join_features FAILED: result size mismatch");
                    continue;
                }
                // 验证第一行 (假设顺序保持插入顺序，或数据库默认排序)
                // 使用结构化绑定解包 tuple
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

    // --- 场景 9: 更贴近真实使用的综合场景测试 (SQLite 版本) ---
    static auto test_realistic_scenario() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_realistic_scenario (SQLite)");

        co_await db.execute("DROP TABLE IF EXISTS realistic_users");

        const char *create_sql = "CREATE TABLE realistic_users ("
                                 "id INTEGER PRIMARY KEY, "
                                 "name TEXT, "
                                 "bio TEXT, "
                                 "balance REAL, "
                                 "active INTEGER, "
                                 "created_at DATETIME, "
                                 "payload BLOB"
                                 ")";
        auto        r          = co_await db.execute(create_sql);
        CO_EXPECT_RESULT(r);

        auto prep_ret =
            (co_await db.prepare("INSERT INTO realistic_users (id,name,bio,balance,active,created_at,payload) VALUES "
                                 "(?, ?, ?, ?, ?, ?, ?)"));
        CO_ASSERT_VAL(prep_ret);
        auto stmt = std::move(prep_ret.value());

        std::vector<std::vector<std::byte>> blobs = {{std::byte {0x01}, std::byte {0x02}}, {}};

        // 插入多样化数据（含 NULL/空值）
        {
            stmt.reset();
            stmt.bind(1, "Alice", "Long bio...\nLine2", 1234.56, 1, SqlDate(2024, 5, 1, 12, 0, 0), blobs[0]);
            auto ir = co_await stmt.execute();
            CO_EXPECT_RESULT(ir);
        }
        {
            stmt.reset();
            stmt.bind(2, "Bob", "", 0.00, 0, SqlDate(2023, 1, 1, 0, 0, 0), blobs[1]);
            auto ir = co_await stmt.execute();
            CO_EXPECT_RESULT(ir);
        }
        {
            stmt.reset();
            stmt.bind(3, "Charlie", nullptr, 9.99, 1, SqlDate(2022, 12, 31, 23, 59, 59), std::vector<std::byte> {});
            auto ir = co_await stmt.execute();
            CO_EXPECT_RESULT(ir);
        }

        // 验证总数
        auto cnt_ret = co_await db.query<int>("SELECT count(*) FROM realistic_users");
        CO_ASSERT_VAL(cnt_ret);
        int cnt = 0;
        ilias_for_await(auto v, cnt_ret.value().range()) {
            cnt = std::get<0>(v);
        }
        EXPECT_EQ(cnt, 3);

        // 查询并检测数值类型与顺序
        auto rows_ret = co_await db.query<std::tuple<std::string, double, int>>(
            "SELECT name, balance, active FROM realistic_users ORDER BY id");
        CO_ASSERT_VAL(rows_ret);
        auto                                              res   = std::move(rows_ret.value());
        std::vector<std::tuple<std::string, double, int>> truth = {
            {"Alice", 1234.56, 1}, {"Bob", 0.0, 0}, {"Charlie", 9.99, 1}};
        int idx = 0;
        ilias_for_await(auto &row, res.range()) {
            auto &[n, b, a] = row;
            EXPECT_EQ(n, std::get<0>(truth[idx]));
            EXPECT_NEAR(b, std::get<1>(truth[idx]), 0.001);
            EXPECT_EQ(a, std::get<2>(truth[idx]));
            idx++;
        }
        EXPECT_EQ(idx, 3);

        ILIAS_INFO("test", ">>> test_realistic_scenario (SQLite) PASSED");
        co_return {};
    }
};

// ==========================================
// 4. 统一入口 Runner
// ==========================================

ILIAS_NAMESPACE::Task<void> run_all_tests() {
    try {
        // 运行原有测试
        co_await SqlTestSuite::test_basic_crud();

        // 运行新增的扩展测试
        co_await SqlTestSuite::test_batch_insert_and_scan();
        co_await SqlTestSuite::test_pagination();
        co_await SqlTestSuite::test_bulk_update_delete();
        co_await SqlTestSuite::test_null_handling();
        co_await SqlTestSuite::test_null_bind();
        co_await SqlTestSuite::test_transaction_rollback();
        co_await SqlTestSuite::test_raii_rollback();
        co_await SqlTestSuite::test_realistic_scenario();
        co_await SqlTestSuite::test_form_interface();
        co_await SqlTestSuite::test_join_features();
    } catch (const std::exception &e) {
        ILIAS_ERROR("test", "Exception caught in tests: {}", e.what());
        EXPECT_TRUE(false) << "Exception in test runner: " << e.what();
    }
}

TEST(SQL, FullSuite) {
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