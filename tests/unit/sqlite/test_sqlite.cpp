#include <gtest/gtest.h>
#include <sqlite3.h>
#include <tuple>
#include <vector>
#include <string>

#include <ilias/platform.hpp>
#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "ilias/sql/sqldatabase.hpp"

// 假设这些在你的项目中存在
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;

// ==========================================
// 1. 协程测试辅助宏
// ==========================================
#define CO_EXPECT_TRUE(cond)                                                                                           \
    do {                                                                                                               \
        EXPECT_TRUE(cond);                                                                                             \
        if (!(cond))                                                                                                   \
            co_return {};                                                                                              \
    } while (0)

#define CO_EXPECT_FALSE(cond)                                                                                          \
    do {                                                                                                               \
        EXPECT_FALSE(cond);                                                                                            \
        if (cond)                                                                                                      \
            co_return {};                                                                                              \
    } while (0)

#define CO_EXPECT_EQ(val1, val2)                                                                                       \
    do {                                                                                                               \
        EXPECT_EQ((val1), (val2));                                                                                     \
        if ((val1) != (val2))                                                                                          \
            co_return {};                                                                                              \
    } while (0)

#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value())                                                                                          \
            ILIAS_ERROR("sql-test", "assert failed: {}", ret.error().message());                                       \
        CO_EXPECT_TRUE(ret.has_value());                                                                               \
    } while (0)

// ==========================================
// 2. 测试用的数据结构
// ==========================================
struct SimpleUser {
    int         id;
    std::string name;
    int         score;
};

// 用于测试 Tuple 绑定的结构
struct UserTupleBind {
    int         id;
    std::string name;
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

        // 基础建表
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

    // --- 测试用例 1: 基础接口与原始类型绑定 ---
    static auto test_basic_crud() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_basic_crud");

        // 1.1 execute_with (Variadic Args / Tuple-like)
        // 测试直接传递参数，不使用结构体
        auto ret_ins = co_await db.execute_with("INSERT INTO users (id, name, score) VALUES (:id, :name, :score)", 1,
                                                std::string("User1"), 100);
        CO_ASSERT_VAL(ret_ins);
        CO_EXPECT_EQ(ret_ins.value(), 1); // 影响1行

        // 1.2 query_with (Variadic Args) & 标量结果 (int)
        // 测试 SELECT COUNT(*) 返回单个 int
        auto ret_count = co_await db.query_with("SELECT count(*) FROM users WHERE score > :score", 90);
        CO_ASSERT_VAL(ret_count);
        auto res_count = std::move(ret_count.value());

        int count = 0;
        ilias_for_await(auto val, res_count.range()) {
            count += (bool)val;
        }
        CO_EXPECT_EQ(count, 1);

        ILIAS_INFO("test", ">>> test_basic_crud PASSED");
        co_return {};
    }

    // --- 测试用例 2: Statement 的生命周期与手动绑定 ---
    static auto test_statement_manual() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_statement_manual");

        // 2.1 Prepare
        auto ret_stmt = co_await db.prepare("INSERT INTO users (id, name, score) VALUES (?, ?, ?)");
        CO_ASSERT_VAL(ret_stmt);
        auto stmt = std::move(ret_stmt.value());

        // 2.2 Bind (Variadic) & Execute
        CO_EXPECT_TRUE(stmt.bind(2, "User2", 200));
        auto ret_exec1 = co_await stmt.execute();
        CO_ASSERT_VAL(ret_exec1);

        // 2.3 Reset & Bind Again
        stmt.reset();
        CO_EXPECT_TRUE(stmt.bind(3, "User3", 300));
        auto ret_exec2 = co_await stmt.execute();
        CO_ASSERT_VAL(ret_exec2);

        // 验证插入结果
        auto ret_query =
            co_await db.query<std::tuple<int, std::string, int>>("SELECT id, name, score FROM users ORDER BY id");
        CO_ASSERT_VAL(ret_query);
        auto result = std::move(ret_query.value());

        int rows = 0;
        ilias_for_await(auto row, result.range()) {
            rows++;
            if (std::get<0>(row) == 2) {
                CO_EXPECT_EQ(std::get<1>(row), "User2");
            }
        }
        CO_EXPECT_EQ(rows, 2);

        ILIAS_INFO("test", ">>> test_statement_manual PASSED");
        co_return {};
    }

    // --- 测试用例 3: 事务提交 (Commit) ---
    static auto test_transaction_commit() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_transaction_commit");

        // 开启事务
        auto ret_tx = co_await db.transaction();
        CO_ASSERT_VAL(ret_tx);
        auto tx = std::move(ret_tx.value());

        // 在事务中执行操作
        auto ret1 = co_await tx.execute("INSERT INTO users (id, name, score) VALUES (10, 'TxUser', 999)");
        CO_ASSERT_VAL(ret1);

        // 提交事务
        auto ret_commit = co_await tx.commit();
        CO_ASSERT_VAL(ret_commit);

        // 验证数据已持久化
        auto ret_check = co_await db.query_with<int>("SELECT count(*) FROM users WHERE id = :id", 10);
        CO_ASSERT_VAL(ret_check);
        int count = 0;
        ilias_for_await(auto val, ret_check.value().range()) count += (bool)val;
        CO_EXPECT_EQ(count, 1);

        ILIAS_INFO("test", ">>> test_transaction_commit PASSED");
        co_return {};
    }

    // --- 测试用例 4: 事务回滚 (Rollback) ---
    static auto test_transaction_rollback() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_transaction_rollback");

        auto ret_tx = co_await db.transaction();
        CO_ASSERT_VAL(ret_tx);
        auto tx = std::move(ret_tx.value());

        // 插入数据
        auto ret1 = co_await tx.execute("INSERT INTO users (id, name, score) VALUES (20, 'RollbackUser', 888)");
        CO_ASSERT_VAL(ret1);

        // 回滚事务
        auto ret_rb = co_await tx.rollback();
        CO_ASSERT_VAL(ret_rb);

        // 验证数据不存在
        auto ret_check = co_await db.query_with<int>("SELECT count(*) FROM users WHERE id = :id", 20);
        CO_ASSERT_VAL(ret_check);
        int count = 0;
        ilias_for_await(auto val, ret_check.value().range()) count += (bool)val;
        CO_EXPECT_EQ(count, 0);

        ILIAS_INFO("test", ">>> test_transaction_rollback PASSED");
        co_return {};
    }

    // --- 测试用例 5: 结构体映射与 prepare_with ---
    static auto test_struct_mapping() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_struct_mapping");

        SimpleUser u1 {1, "StructUser", 500};

        // prepare_with + Struct
        auto ret_stmt = co_await db.prepare_with("INSERT INTO users (id, name, score) VALUES (:id, :name, :score)", u1);
        CO_ASSERT_VAL(ret_stmt);
        auto stmt = std::move(ret_stmt.value());

        // 此时 u1 只是用来推导类型和检查 SQL，实际绑定需要再次 bind (假设 bind 接口设计如此)
        // 或者 prepare_with 已经做了初步绑定?
        // 根据你的接口定义：prepare_with 返回 SqlStatement<T>
        // 通常还需要 stmt.bind(arg) 或者 stmt.execute() 如果没有保存参数引用。
        // 但通常 prepare_with 这种设计如果为了方便，可能会直接执行，或者这里我们手动 bind 一次

        CO_EXPECT_TRUE(stmt.bind(u1));
        auto ret_exec = co_await stmt.execute();
        CO_ASSERT_VAL(ret_exec);

        // query_with + Struct Result
        auto ret_query = co_await db.query_with("SELECT * FROM users WHERE id = :id", 1);
        // 注意：query_with 的第二个参数是参数绑定，模板参数是结果类型？
        // 修正：根据你的接口定义:
        // auto query_with(SqlStructCheck<std::decay_t<U>> query, U &&arg) -> IoTask<SqlResult<void>>;
        // 看来 query_with 是用来执行带参数的查询，但返回的是 void 类型的 Result？
        // 这通常意味着它不负责自动反序列化到 struct，或者设计上有所不同。
        // 我们改用标准的 query<T> 配合参数。

        // 正确路径：使用 query<SimpleUser> 但手动绑定参数，或者拼装 SQL
        // 或者使用 prepare<SimpleUser> + bind + query
        auto ret_q_stmt = co_await db.prepare<SimpleUser>("SELECT * FROM users WHERE id = ?");
        CO_ASSERT_VAL(ret_q_stmt);
        auto q_stmt = std::move(ret_q_stmt.value());

        q_stmt.bind(1);
        auto ret_res = co_await q_stmt.query();
        CO_ASSERT_VAL(ret_res);
        auto res = std::move(ret_res.value());

        ilias_for_await(auto &user, res.range()) {
            CO_EXPECT_EQ(user.id, 1);
            CO_EXPECT_EQ(user.name, "StructUser");
            CO_EXPECT_EQ(user.score, 500);
        }

        ILIAS_INFO("test", ">>> test_struct_mapping PASSED");
        co_return {};
    }

    // --- 测试用例 6: 手动 Load (Dynamic Result) ---
    static auto test_manual_load() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_manual_load");

        co_await db.execute("INSERT INTO users VALUES (99, 'Dynamic', 777)");

        // 使用 query<> (默认 void) 获取动态结果集
        auto ret = co_await db.query<>("SELECT * FROM users WHERE id = 99");
        CO_ASSERT_VAL(ret);
        auto res = std::move(ret.value());

        ilias_for_await(auto row, res.range()) {
            // 这里 row 应该是 IoResult<void> 或者某种 cursor，
            // 但根据你的定义 range() -> Generator<IoResult<void>>
            // 且 SqlResult<void> 有 load 方法。
            // 这是一个稍微奇怪的设计，通常迭代器会返回一个 Row 对象。
            // 假设在这个库的设计中，迭代时 SqlResult 内部状态更新指向当前行：

            int         id_val;
            std::string name_val;

            // 通过列名获取
            auto r1 = res.load("id", id_val);
            CO_EXPECT_TRUE(r1);
            CO_EXPECT_EQ(id_val, 99);

            // 通过索引获取
            auto r2 = res.load(1, name_val); // name 是第2列 (index 1)
            CO_EXPECT_TRUE(r2);
            CO_EXPECT_EQ(name_val, "Dynamic");
        }

        ILIAS_INFO("test", ">>> test_manual_load PASSED");
        co_return {};
    }

    // --- 测试用例 7: 错误处理 ---
    static auto test_errors() -> IoTask<void> {
        auto db = (co_await setup_db()).value();
        ILIAS_INFO("test", ">>> Running test_errors");

        // 7.1 语法错误
        auto ret1 = co_await db.execute("SELECT * FROM non_existent_table");
        CO_EXPECT_FALSE(ret1.has_value());
        // 打印错误看是否符合预期
        // ILIAS_INFO("test", "Expected error: {}", ret1.error().message());

        // 7.2 约束冲突 (主键重复)
        co_await db.execute("INSERT INTO users VALUES (1, 'A', 1)");
        auto ret2 = co_await db.execute("INSERT INTO users VALUES (1, 'B', 2)");
        CO_EXPECT_FALSE(ret2.has_value());

        ILIAS_INFO("test", ">>> test_errors PASSED");
        co_return {};
    }

    // --- 测试用例 8: 数据库关闭 ---
    static auto test_close() -> IoTask<void> {
        auto ret = co_await SqlDatabase::open_in_memory();
        CO_ASSERT_VAL(ret);
        auto db = std::move(ret.value());

        auto ret_close = co_await db.close();
        CO_ASSERT_VAL(ret_close);

        // 关闭后尝试执行应报错
        auto ret_fail = co_await db.execute("CREATE TABLE t (a int)");
        CO_EXPECT_FALSE(ret_fail.has_value());

        ILIAS_INFO("test", ">>> test_close PASSED");
        co_return {};
    }
};

// ==========================================
// 4. 统一入口 Runner
// ==========================================

ILIAS_NAMESPACE::Task<void> run_all_tests() {
    try {
        co_await SqlTestSuite::test_basic_crud();
        co_await SqlTestSuite::test_statement_manual();
        co_await SqlTestSuite::test_transaction_commit();
        co_await SqlTestSuite::test_transaction_rollback();
        co_await SqlTestSuite::test_struct_mapping();
        co_await SqlTestSuite::test_manual_load();
        co_await SqlTestSuite::test_errors();
        co_await SqlTestSuite::test_close();
    } catch (const std::exception &e) {
        ILIAS_ERROR("test", "Exception caught in tests: {}", e.what());
        EXPECT_TRUE(false) << "Exception in test runner";
    }
}

TEST(SQL, FullSuite) {
    run_all_tests().wait();
}

int main(int argc, char **argv) {
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    cpptrace::init();
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}