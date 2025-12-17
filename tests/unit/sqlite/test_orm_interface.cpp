#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cmath>
#include <limits>
#include <optional>

#include <ilias/platform.hpp>
#include "ilias/sql_orm/orm_form.hpp"
#include "ilias/sql/sqldatabase.hpp"
#include "ilias/sql/sqlresult.hpp"
#include "ilias/sqlite/sqlite.hpp"
#include "../backtrace.hpp"

ILIAS_SQL_USE_NAMESPACE;
using namespace ILIAS_NAMESPACE;
NEKO_USE_NAMESPACE

// ... [原有宏定义 CO_ASSERT_VAL 保持不变] ...
#define CO_ASSERT_VAL(ret)                                                                                             \
    do {                                                                                                               \
        if (!ret.has_value()) {                                                                                        \
            ILIAS_ERROR("orm-test", "assert failed: {}", ret.error().message());                                       \
            co_return {};                                                                                              \
        }                                                                                                              \
    } while (0)

// ==========================================
// 新增：全类型覆盖测试实体
// ==========================================
struct ComplexModel {
    std::optional<int64_t> id         = 0;
    char                   tiny_val   = 0;    // TinyInt
    int32_t                int_val    = 0;    // Int
    int64_t                big_val    = 0;    // BigInt
    float                  float_val  = 0.0f; // Float
    double                 double_val = 0.0;  // Double
    std::string            text_val;          // Text
    std::vector<std::byte> blob_val;          // Binary
    // 假设 SqlDate 内部表现为字符串或时间戳，这里演示其作为成员
    // 如果 SqlDate 是库内建类型，直接使用。
    std::string date_val; // 模拟 Timestamp/Date (SQLite常以Text存储)

    // 可空类型测试
    std::optional<int>         opt_int;
    std::optional<std::string> opt_text;

    // 唯一性约束字段
    std::string unique_code;
};

// 辅助：构建 Blob
std::vector<std::byte> make_blob(const std::string &s) {
    std::vector<std::byte> b;
    b.reserve(s.size());
    for (char c : s)
        b.push_back(static_cast<std::byte>(c));
    return b;
}

template <typename T, size_t N>
std::vector<std::byte> make_blob(const T (&arr)[N]) {
    std::vector<std::byte> b;
    if (N == 0) {
        return b;
    }
    if (sizeof(T) % sizeof(char) != 0) {
        throw std::runtime_error("make_blob: element type size is not multiple of char size");
    }
    auto stride = sizeof(T) / sizeof(char);
    b.reserve(N * stride);
    for (auto &e : arr) {
        const char *p = reinterpret_cast<const char *>(&e);
        for (size_t i = 0; i < stride; ++i) {
            b.push_back(static_cast<std::byte>(p[i]));
        }
    }
    return b;
}

// 辅助：比较 Blob
bool blob_eq(const std::vector<std::byte> &a, const std::string &s) {
    if (a.size() != s.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i] != static_cast<std::byte>(s[i]))
            return false;
    }
    return true;
}

NEKO_BEGIN_NAMESPACE
template <>
struct Meta<ComplexModel, void> {
    constexpr static auto value =
        Object("id", make_tags<SqlTags {.primary_key = true, .auto_increment = true}>(&ComplexModel::id), "tiny_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::tiny_val), "int_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::int_val), "big_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::big_val), "float_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::float_val), "double_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::double_val), "text_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::text_val), "blob_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::blob_val), "date_val",
               make_tags<SqlTags {.not_null = true}>(&ComplexModel::date_val), "opt_int",
               make_tags<SqlTags {.not_null = false}>(&ComplexModel::opt_int), "opt_text",
               make_tags<SqlTags {.not_null = false}>(&ComplexModel::opt_text), "unique_code",
               make_tags<SqlTags {.unique = true, .not_null = true}>(&ComplexModel::unique_code));
};
NEKO_END_NAMESPACE

class OrmFullCoverageTests {
public:
    static auto setup_db() -> IoTask<SqlDatabase> {
        auto ret = co_await SqlDatabase::open_in_memory();
        if (!ret)
            throw std::runtime_error("open db failed");
        auto db = std::move(ret.value());
        co_return db;
    }

    // 测试 1: 完整的数据类型往返（Round-Trip），验证精度和二进制安全
    static auto test_all_types_round_trip() -> IoTask<void> {
        auto db       = (co_await setup_db()).value();
        auto form_ret = co_await Form<ComplexModel, SqliteTag>::create(db, "complex_models");
        CO_ASSERT_VAL(form_ret);
        auto form = std::move(form_ret.value());

        ComplexModel m;
        m.id          = 0;
        m.tiny_val    = 127;
        m.int_val     = 123456;
        m.big_val     = std::numeric_limits<int64_t>::max(); // Max Int64
        m.float_val   = 3.14159f;
        m.double_val  = 1.23456789012345;
        m.text_val    = "Hello \n 'World' \"Quote\"";  // 特殊字符测试
        m.blob_val    = make_blob("\x00\x01\xFF\xFE"); // 二进制测试
        m.date_val    = "2023-10-01 12:00:00";
        m.opt_int     = 999;
        m.opt_text    = "Optional String";
        m.unique_code = "U001";
        ILIAS_INFO("orm-test", "Inserting ComplexModel with all types, blob {}", SqlValueView {m.blob_val});
        // 插入
        auto ins_ret = co_await form.insert(m);
        CO_ASSERT_VAL(ins_ret);

        // 读取
        auto query_ret = co_await form.select().where(form.col(&ComplexModel::unique_code) == "U001").query();
        CO_ASSERT_VAL(query_ret);

        int count = 0;
        ilias_for_await(auto &row, query_ret.value().range()) {
            auto [id, t, i, b, f, d, txt, blb, dt, oi, ot, uc] = row;

            EXPECT_EQ(t, m.tiny_val);
            EXPECT_EQ(i, m.int_val);
            EXPECT_EQ(b, m.big_val);
            EXPECT_FLOAT_EQ(f, m.float_val);
            EXPECT_DOUBLE_EQ(d, m.double_val);
            EXPECT_EQ(txt, m.text_val);
            EXPECT_TRUE(std::equal(blb.begin(), blb.end(), m.blob_val.begin()));
            EXPECT_EQ(dt, m.date_val);

            EXPECT_TRUE(oi.has_value());
            EXPECT_EQ(*oi, *m.opt_int);
            EXPECT_TRUE(ot.has_value());
            EXPECT_EQ(*ot, *m.opt_text);

            count++;
        }
        EXPECT_EQ(count, 1);
        co_await form.print();
        co_return {};
    }

    // 测试 2: Null 值处理与 std::optional 映射
    static auto test_null_semantics() -> IoTask<void> {
        auto db   = (co_await setup_db()).value();
        auto form = (co_await Form<ComplexModel, SqliteTag>::create(db, "complex_models")).value();

        // 插入包含 std::nullopt 的数据
        auto ins_ret = co_await form.insert(1, 'c', 100, 1000, 1.1f, 2.2, "Text", make_blob("b"), "2023-01-01",
                                            std::nullopt, // opt_int is NULL
                                            std::nullopt, // opt_text is NULL
                                            "U_NULL_TEST");
        CO_ASSERT_VAL(ins_ret);

        // 1. 在 C++ 侧验证读取结果
        auto q1 = co_await form.select().where(form.col(&ComplexModel::unique_code) == "U_NULL_TEST").query();
        CO_ASSERT_VAL(q1);
        ilias_for_await(auto &row, q1.value().range()) {
            auto [id, t, i, b, f, d, txt, blb, dt, oi, ot, uc] = row;
            EXPECT_FALSE(oi.has_value()); // 应该是 nullopt
            EXPECT_FALSE(ot.has_value()); // 应该是 nullopt
        }

        // 2. 使用 SQL 语义进行筛选 (WHERE opt_int IS NULL)
        // 假设 ORM 重载了 == nullptr 或者有 is_null() 方法，或者是用 execute raw sql 验证
        // 这里尝试用 raw where 验证数据确实落盘为 NULL
        auto q2 = co_await form.select().where("opt_int"_sql == nullptr).query();
        CO_ASSERT_VAL(q2);
        int null_cnt = 0;
        ilias_for_await(auto &row, q2.value().range()) {
            null_cnt++;
            auto [id, t, i, b, f, d, txt, blb, dt, oi, ot, uc] = row;
            EXPECT_FALSE(oi.has_value()); // 应该是 nullopt
            EXPECT_FALSE(ot.has_value()); // 应该是 nullopt
            EXPECT_EQ(id, 1);
            EXPECT_EQ(t, 'c');
        }
        EXPECT_EQ(null_cnt, 1);

        co_return {};
    }

    // 测试 3: 复杂查询逻辑 (AND, OR, 大于小于, 排序)
    static auto test_complex_queries() -> IoTask<void> {
        auto db   = (co_await setup_db()).value();
        auto form = (co_await Form<ComplexModel, SqliteTag>::create(db, "complex_models")).value();

        std::vector<ComplexModel> data;

        // 准备数据
        for (int k = 0; k < 10; ++k) {
            data.emplace_back(std::nullopt, 'a' + k, k * 10, k * 100, (float)k, (double)k,
                              "Group" + std::to_string(k % 2), // Group0 or Group1
                              make_blob(""), "2023", std::nullopt, std::nullopt, "CODE_" + std::to_string(k));
        }
        auto ret = co_await form.insert(data);
        CO_ASSERT_VAL(ret);

        // 场景 A: 范围 + 逻辑与 (int_val >= 30 AND int_val <= 70)
        auto qA = co_await form.select()
                      .where(form.col(&ComplexModel::int_val) >= 30 && form.col(&ComplexModel::int_val) <= 70)
                      .query();
        CO_ASSERT_VAL(qA);
        int countA = 0;
        ilias_for_await(auto &row, qA.value().range()) {
            countA++;
            auto [id, t, i, b, f, d, txt, blb, dt, oi, ot, uc] = row;
            EXPECT_EQ(t, 'a' + (*id - 1));
            EXPECT_EQ(i, 10 * (*id - 1));
            EXPECT_EQ(b, 100 * (*id - 1)); // blob is empty
            EXPECT_EQ(txt, "Group" + std::to_string((*id - 1) % 2));
            EXPECT_EQ(uc, "CODE_" + std::to_string(*id - 1));
        }
        EXPECT_EQ(countA, 5); // 30, 40, 50, 60, 70

        // 场景 B: 混合类型逻辑 (Group1 AND double_val > 5.0)
        auto qB = co_await form.select()
                      .where(form.col(&ComplexModel::text_val) == "Group1" && form.col(&ComplexModel::double_val) > 5.0)
                      .query();
        CO_ASSERT_VAL(qB);
        // k=1,3,5,7,9 are Group1. k>5 are 7,9.
        int countB = 0;
        ilias_for_await([[maybe_unused]] auto &r, qB.value().range()) {
            countB++;
        }
        EXPECT_EQ(countB, 2);
        co_await form.print();
        // 场景 C: 排序 (Desc by id)
        auto qC = co_await form.select("id")
                      .where(form.col(&ComplexModel::id) < 3)
                      .orderBy("id", true) // true for desc
                      .query();
        CO_ASSERT_VAL(qC);
        std::vector<int64_t> ids;
        ilias_for_await(auto &ret, qC.value().range()) {
            CO_ASSERT_VAL(ret);
            int64_t id;
            qC.value().load(0, id);
            ids.push_back(id);
        }
        // id 从 1 开始， <3 为 1, 2。倒序应为 2, 1
        EXPECT_EQ(ids.size(), 2);
        if (ids.size() == 2) {
            EXPECT_EQ(ids[0], 2);
            EXPECT_EQ(ids[1], 1);
        }

        co_return {};
    }

    // 测试 4: 数据库约束 (Unique, Not Null)
    static auto test_constraints() -> IoTask<void> {
        auto db   = (co_await setup_db()).value();
        auto form = (co_await Form<ComplexModel, SqliteTag>::create(db, "complex_models")).value();

        // 1. 正常插入
        auto r1 = co_await form.insert(1, '1', 1, 1, 1.0, 1.0, "t", make_blob(""), "d", std::nullopt, std::nullopt,
                                       "UNIQUE_A");
        CO_ASSERT_VAL(r1);

        // 2. 违反 Unique 约束 (插入相同的 unique_code)
        // 期望：orm 应该捕获错误或返回 result 为 error
        // 注意：Ilias ORM 的 insert 返回 Result<int> (通常是 last_insert_id 或受影响行)
        // 失败时应包含错误信息
        auto r2 = co_await form.insert(2, '2', 2, 2, 2.0, 2.0, "t", make_blob(""), "d", std::nullopt, std::nullopt,
                                       "UNIQUE_A");

        if (r2.has_value()) {
            // 如果返回成功，检查是否真的插入了（某些配置下 INSERT OR IGNORE 可能会发生）
            auto c   = co_await form.count().where(form.col(&ComplexModel::unique_code) == "UNIQUE_A").query();
            int  cnt = 0;
            ilias_for_await(auto &row, c.value().range()) {
                CO_ASSERT_VAL(row);
                c.value().load(0, cnt);
            }
            EXPECT_EQ(cnt, 1) << "Unique constraint violated but duplicates found or ignored silently";
            if (cnt > 1) {
                ILIAS_WARN("sql-test", "Unique constraint failed to prevent duplicate");
            }
        }
        else {
            // 预期内的失败
            SUCCEED();
        }

        co_return {};
    }

    static ILIAS_NAMESPACE::Task<void> run_all() {
        try {
            co_await test_all_types_round_trip();
            co_await test_null_semantics();
            co_await test_complex_queries();
            co_await test_constraints();
        } catch (const std::exception &e) {
            ILIAS_ERROR("orm-test-full", "Exception: {}", e.what());
            EXPECT_TRUE(false) << e.what();
        }
        co_return;
    }
};

// 将新的测试套件加入 GoogleTest
TEST(ORM, FullTypeCoverage) {
    OrmFullCoverageTests::run_all().wait();
}

int main(int argc, char **argv) {
    cpptrace::init();
    ILIAS_LOG_SET_LEVEL(ILIAS_TRACE_LEVEL);
    ilias::PlatformContext ioContext;
    ioContext.install();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}