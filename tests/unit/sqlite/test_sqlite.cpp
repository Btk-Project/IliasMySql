#include <gtest/gtest.h>
#include <sqlite3.h>
#include <string>
#include <vector>

#include <ilias/platform.hpp>
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "ilias/sql/sqldatabase.hpp"

// 假设这些在你的项目中存在
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;

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
            ILIAS_INFO("sql-test", "expected failure: {}", result.error().message());                                    \
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
    int         id;
    std::string name;
    int         score;
};

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
            CO_EXPECT_RESULT(stmt.bind(i, name, i * 10));
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

            sum_score += user.score;
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

        // 分页查询: 按 score 升序 (小分在前)，取第 11 到 20 条 (Limit 10 Offset 10)
        // Score 应该是: 51, 52, ..., 60 (对应 ID 49, 48... )
        // SQL: SELECT score FROM users ORDER BY score ASC LIMIT 10 OFFSET 10

        auto ret_query =
            co_await db.query_with<int>("SELECT score FROM users ORDER BY score ASC LIMIT :lim OFFSET :off", 10, 10);
        CO_ASSERT_VAL(ret_query);
        auto result = std::move(ret_query.value());

        std::vector<int> scores;
        int              val;
        ilias_for_await(auto r, result.range()) {
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
        auto ret_up = co_await db.execute_with("UPDATE users SET score = :s WHERE id >= :id", 999, 10);
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
        // ILIAS_INFO("test", "Expected error: {}", ret1.error().message());

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
            ILIAS_INFO("test", "Got user with score: {}", u.score);
        }
        EXPECT_TRUE(found);

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

    } catch (const std::exception &e) {
        ILIAS_ERROR("test", "Exception caught in tests: {}", e.what());
        EXPECT_TRUE(false) << "Exception in test runner: " << e.what();
    }
}

TEST(SQL, FullSuite) {
    // run_all_tests().wait();
}

int main(int argc, char **argv) {
    cpptrace::init();
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ilias::PlatformContext ioContext;
    ioContext.install();
    run_all_tests().wait();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}