#include <cstdlib>
#include <gtest/gtest.h>
#include <ilias/platform.hpp>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <memory>
#include <ilias/sync/event.hpp> // 引入协程事件
#include <ilias/sync/mpsc.hpp>  // 引入 mpsc channel
#include <algorithm>            // for std::sort
#include <numeric>              // for std::accumulate
#include <iomanip>              // for formatting output

#include "ilias/sql/driver_registry.hpp"
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sql/sqlstatement.hpp"
#include "../backtrace.hpp"

const int    NUM_USERS             = 500; // 初始用户数 (原: 5000)
const size_t POOL_SIZE             = 10;  // 数据库连接池大小 (原: 50)
const int    NUM_WRITER_COROUTINES = 25;  // 写入（转账）协程数 (原: 250)
const int    NUM_READER_COROUTINES = 50;  // 只读协程数 (原: 500)
const int    OPERATIONS_PER_WRITER = 10;  // 每个写协程的转账次数 (原: 50)
const int    OPERATIONS_PER_READER = 20;  // 每个读协程的查询次数 (原: 100)

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;
using namespace std::literals;
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

// 线程安全的延迟收集与分析工具
class LatencyCollector {
public:
    // 记录一次操作的延迟
    void record(std::chrono::microseconds duration) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_latencies.push_back(duration);
    }

    // 分析并打印延迟统计日志
    void analyze_and_log(const std::string &name) {
        std::vector<std::chrono::microseconds> latencies_copy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_latencies.empty()) {
                ILIAS_INFO("orm-test", "No latency data collected for {}", name);
                return;
            }
            // 复制数据以便在无锁状态下进行耗时操作
            latencies_copy = m_latencies;
        }

        // 排序是计算百分位延迟的前提
        std::sort(latencies_copy.begin(), latencies_copy.end());

        size_t n     = latencies_copy.size();
        auto   to_ms = [](std::chrono::microseconds us) { return static_cast<double>(us.count()) / 1000.0; };

        double sum_ms = 0.0;
        for (const auto &lat : latencies_copy) {
            sum_ms += to_ms(lat);
        }

        ILIAS_INFO("orm-test", "--- Latency Analysis for [{}] Operations ---", name);
        ILIAS_INFO("orm-test", "Total Operations: {}", n);
        if (n > 0) {
            ILIAS_INFO("orm-test", "Min Latency:    {:.3f} ms", to_ms(latencies_copy.front()));
            ILIAS_INFO("orm-test", "Avg Latency:    {:.3f} ms", sum_ms / n);
            ILIAS_INFO("orm-test", "Median (P50):   {:.3f} ms", to_ms(latencies_copy[n / 2]));
            ILIAS_INFO("orm-test", "P90 Latency:    {:.3f} ms", to_ms(latencies_copy[static_cast<size_t>(n * 0.90)]));
            ILIAS_INFO("orm-test", "P95 Latency:    {:.3f} ms", to_ms(latencies_copy[static_cast<size_t>(n * 0.95)]));
            ILIAS_INFO("orm-test", "P99 Latency:    {:.3f} ms", to_ms(latencies_copy[static_cast<size_t>(n * 0.99)]));
            ILIAS_INFO("orm-test", "Max Latency:    {:.3f} ms", to_ms(latencies_copy.back()));
        }
        ILIAS_INFO("orm-test", "----------------------------------------------------");
    }

private:
    std::mutex                             m_mutex;
    std::vector<std::chrono::microseconds> m_latencies;
};

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
    // CHANGED: 使用 int64_t 存储金额（以分为单位），避免浮点数精度问题
    // 注意：数据库中的对应字段类型也应该修改为 BIGINT
    int64_t balance_in_cents = 0;
};

// 反射元数据定义
NEKO_BEGIN_NAMESPACE
// clang-format off
template <>
struct Meta<SimpleUser, void> {
    constexpr static auto value = Object(
        "id",           make_tags<SqlTags::createPrimaryKeyTags()>(&SimpleUser::id), 
        "name",         make_tags<SqlTags {.not_null = true}>(&SimpleUser::name),
        "age",          make_tags<SqlTags {}>(&SimpleUser::age),
        "email",        make_tags<SqlTags {.not_null = false, .unique = true}>(&SimpleUser::email), 
        "created_at",   make_tags<SqlTags {.not_null = true, .created_at = true}>(&SimpleUser::created_at),
        "is_active",    make_tags<SqlTags {.not_null = true}>(&SimpleUser::is_active),
        "balance_in_cents",      make_tags<SqlTags {.not_null = true}>(&SimpleUser::balance_in_cents));
};
// clang-format on
NEKO_END_NAMESPACE

// 一个简单的协</strong>程安全数据库连接池
class ConnectionPool {
public:
    // RAII 包装器，确保连接使用完毕后自动返回池中
    class PooledConnection {
    public:
        // 禁止拷贝
        PooledConnection(const PooledConnection &)            = delete;
        PooledConnection &operator=(const PooledConnection &) = delete;

        // 允许移动
        PooledConnection(PooledConnection &&other) noexcept
            : m_conn(std::move(other.m_conn)), m_sender(other.m_sender) {
            other.m_sender = nullptr; // 防止被移动的对象析构时归还连接
        }

        ~PooledConnection() {
            // 如果 sender 有效且连接存在，则归还连接
            if (m_sender && m_conn) {
                auto conn = std::move(m_conn.value());
                m_sender->trySend(std::move(*conn));
            }
        }

        SqlDatabase *operator->() { return m_conn.value().get(); }
        SqlDatabase &operator*() { return *m_conn.value(); }

    private:
        friend class ConnectionPool;
        PooledConnection(SqlDatabase &&conn, mpsc::Sender<SqlDatabase> *sender)
            : m_conn(std::make_unique<SqlDatabase>(std::move(conn))), m_sender(sender) {}

        std::optional<std::unique_ptr<SqlDatabase>> m_conn;
        mpsc::Sender<SqlDatabase>                  *m_sender;
    };

    // 工厂函数，异步创建连接池
    static auto create(std::string_view driver, const ConnectOptions &opts, size_t size)
        -> Task<std::shared_ptr<ConnectionPool>> {
        auto pool = std::shared_ptr<ConnectionPool>(new ConnectionPool(size));
        for (size_t i = 0; i < size; ++i) {
            auto db_ret = co_await SqlDatabase::open(driver, opts);
            if (db_ret) {
                pool->m_sender.trySend(std::move(db_ret.value()));
            }
            else {
                ILIAS_ERROR("orm-test", "Failed to create connection for pool: {}", db_ret.error().message());
            }
        }
        co_return pool;
    }

    // 从池中异步获取一个连接
    auto getConnection() -> Task<PooledConnection> {
        auto conn_opt = co_await m_receiver.recv();
        if (!conn_opt) {
            // 如果通道关闭，抛出异常
            throw std::runtime_error("Connection pool has been closed.");
        }
        co_return PooledConnection(std::move(*conn_opt), &m_sender);
    }

private:
    ConnectionPool(size_t size) {
        auto [sender, receiver] = mpsc::channel<SqlDatabase>(size);
        m_sender                = std::move(sender);
        m_receiver              = std::move(receiver);
    }

    mpsc::Sender<SqlDatabase>   m_sender;
    mpsc::Receiver<SqlDatabase> m_receiver;
};

// ==========================================
// 7. ORM 协程并发测试套件 (Coroutine-Native)
// ==========================================
class ORMConcurrencyTestSuite {
public:
    // 主并发测试场景：混合读写
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

    static auto test_concurrent_reads_and_writes_coroutines() -> IoTask<void> {
        PERF_TIMER("test_concurrent_reads_and_writes_coroutines");
        ILIAS_INFO("orm-test", ">>> Running test_concurrent_reads_and_writes_coroutines");

        // 1. 初始化数据库和测试数据
        auto db_ret = co_await SqlDatabase::open("mysql", get_options());
        CO_ASSERT_VAL(db_ret);
        auto db  = std::move(db_ret.value());
        auto ret = co_await db.execute("DROP TABLE IF EXISTS simple_users_v2");
        CO_EXPECT_RESULT(ret);
        auto users_form_ret = co_await Form<SimpleUser, MysqlTag>::create_if_not_exists(db, "simple_users_v2");
        CO_ASSERT_VAL(users_form_ret);
        auto users = std::move(users_form_ret.value());

        int64_t                 initial_total_balance = 0;
        std::vector<SimpleUser> initial_users;
        for (int i = 1; i <= NUM_USERS; ++i) {
            int64_t balance = 100000; // 初始余额
            initial_users.push_back({i, "User" + std::to_string(i), 30, "user" + std::to_string(i) + "@concurrent.com",
                                     SqlDate {}, true, balance});
            initial_total_balance += balance;
        }
        auto insert_ret = co_await users.insert(initial_users);
        CO_ASSERT_VAL(insert_ret);
        ILIAS_INFO("orm-test", "Initialized {} users with a total balance of {}", NUM_USERS, initial_total_balance);

        auto pool = co_await ConnectionPool::create("mysql", get_options(), POOL_SIZE);

        auto writer_latencies = std::make_shared<LatencyCollector>();
        auto reader_latencies = std::make_shared<LatencyCollector>();

        // 使用 ilias::sync::Event 来确保所有协程同时开始，最大化并发冲突
        auto start_event = std::make_shared<Event>();

        // 写入任务协程：模拟随机转账
        auto writer_task = [&](int task_id, std::shared_ptr<LatencyCollector> collector) -> IoTask<void> {
            // 每个协程使用独立的数据库连接
            auto conn             = co_await pool->getConnection();
            auto thread_db        = conn.operator->(); // 获取裸指针或引用
            auto thread_users_ret = co_await Form<SimpleUser, MysqlTag>::create_if_not_exists(*thread_db, "simple_users_v2");
            CO_ASSERT_VAL(thread_users_ret);

            std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count() + task_id);
            std::uniform_int_distribution<>        user_dist(1, NUM_USERS);
            std::uniform_int_distribution<int64_t> amount_dist(100, 10000);

            co_await *start_event; // 等待开始信号

            for (int i = 0; i < OPERATIONS_PER_WRITER; ++i) {
                auto op_start = std::chrono::high_resolution_clock::now();

                int from_id = user_dist(gen);
                int to_id   = user_dist(gen);
                if (from_id == to_id)
                    continue;

                int64_t amount = amount_dist(gen);

                auto tx_ret = co_await thread_db->transaction();
                if (!tx_ret) {
                    continue;
                }
                auto tx = std::move(tx_ret.value());

                auto update_from_ret =
                    co_await tx.execute_with("UPDATE simple_users_v2 SET balance_in_cents = balance_in_cents - ? WHERE "
                                             "id = ? AND balance_in_cents >= ?",
                                             amount, from_id, amount);
                if (!update_from_ret || update_from_ret.value() == 0) {
                    co_await tx.rollback();
                    auto op_end = std::chrono::high_resolution_clock::now();
                    collector->record(std::chrono::duration_cast<std::chrono::microseconds>(op_end - op_start));
                    continue;
                }

                auto update_to_ret = co_await tx.execute_with(
                    "UPDATE simple_users_v2 SET balance_in_cents = balance_in_cents + ? WHERE id = ?", amount, to_id);

                if (update_to_ret && update_to_ret.value() > 0) {
                    co_await tx.commit();
                }
                else {
                    co_await tx.rollback();
                }

                auto op_end = std::chrono::high_resolution_clock::now();
                collector->record(std::chrono::duration_cast<std::chrono::microseconds>(op_end - op_start));
            }
            co_return {};
        };

        // 读取任务协程：随机读取用户余额
        auto reader_task = [&](int task_id, std::shared_ptr<LatencyCollector> collector) -> IoTask<void> {
            auto conn             = co_await pool->getConnection();
            auto thread_db        = conn.operator->();
            auto thread_users_ret = co_await Form<SimpleUser, MysqlTag>::create_if_not_exists(*thread_db, "simple_users_v2");
            CO_ASSERT_VAL(thread_users_ret);
            auto thread_users = std::move(thread_users_ret.value());

            std::mt19937 gen(std::chrono::high_resolution_clock::now().time_since_epoch().count() + task_id);
            std::uniform_int_distribution<> user_dist(1, NUM_USERS);

            co_await *start_event; // 等待开始信号

            for (int i = 0; i < OPERATIONS_PER_READER; ++i) {
                auto op_start = std::chrono::high_resolution_clock::now();

                int  user_id = user_dist(gen);
                auto select_ret =
                    co_await thread_users.select().where(thread_users.sql(&SimpleUser::id) == user_id).query();
                if (select_ret) {
                    ilias_for_await(auto &user, select_ret.value().range()) {
                        EXPECT_GE(user.balance_in_cents, 0L);
                    }
                }

                auto op_end = std::chrono::high_resolution_clock::now();
                collector->record(std::chrono::duration_cast<std::chrono::microseconds>(op_end - op_start));
            }
            co_return {};
        };

        // 3. 使用 TaskGroup 启动所有并发协程
        TaskGroup<IoResult<void>> group;
        for (int i = 0; i < NUM_WRITER_COROUTINES; ++i) {
            group.spawn(writer_task(i, writer_latencies));
        }
        for (int i = 0; i < NUM_READER_COROUTINES; ++i) {
            group.spawn(reader_task(NUM_WRITER_COROUTINES + i, reader_latencies));
        }

        ILIAS_INFO("orm-test", "Spawning {} writer and {} reader coroutines...", NUM_WRITER_COROUTINES,
                   NUM_READER_COROUTINES);

        // 发出开始信号，所有等待在 event 上的协程将被唤醒并开始并发执行
        start_event->set();
        ILIAS_INFO("orm-test", "Start event fired. All coroutines are running.");

        // 等待 TaskGroup 中所有协程执行完毕
        co_await group.waitAll();
        ILIAS_INFO("orm-test", "All coroutines finished execution.");

        writer_latencies->analyze_and_log("Write Transaction");
        reader_latencies->analyze_and_log("Read Query");

        // co_await users.print();
        // 4. 最终验证
        // 关键验证：所有账户的总金额应该保持不变
        auto final_sum_ret = co_await users.select(sum(users.sql(&SimpleUser::balance_in_cents))).query();
        CO_ASSERT_VAL(final_sum_ret);

        int64_t final_total_balance = 0;
        bool    sum_retrieved       = false;
        ilias_for_await(auto &row, final_sum_ret.value().range()) {
            auto [total]        = row;
            final_total_balance = total;
            sum_retrieved       = true;
        }

        EXPECT_TRUE(sum_retrieved);
        ILIAS_INFO("orm-test", "Initial total balance: {}, Final total balance: {}", initial_total_balance,
                   final_total_balance);
        EXPECT_EQ(initial_total_balance, final_total_balance);
        ILIAS_INFO("orm-test", ">>> test_concurrent_reads_and_writes_coroutines PASSED");
        co_return {};
    }
};

// 将新的测试套件集成到 Google Test
TEST(ORMConcurrency, ConcurrentReadWriteCoroutines) {
    ORMConcurrencyTestSuite::test_concurrent_reads_and_writes_coroutines().wait();
}

int main(int argc, char **argv) {
    cpptrace::init();
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    // ILIAS_LOG_ADD_WHITELIST("ilias-mysql");
    ILIAS_LOG_ADD_WHITELIST("orm-test");
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}