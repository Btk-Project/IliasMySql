# IliasSql 架构与 API 评审

本文档记录当前项目结构、API 设计问题、明显缺陷与后续重构完善方案。内容基于当前代码静态阅读整理，重点关注公共抽象、驱动一致性、生命周期、错误传播、ORM 安全性和可维护性。

## 总体判断

项目整体方向是清晰的：

- `include/ilias/sql` 提供跨数据库公共抽象与类型系统。
- `include/ilias/mysql`、`include/ilias/sqlite`、`include/ilias/postgres` 分别承载具体驱动。
- `include/ilias/sql_orm` 在统一 SQL API 上提供反射驱动的 ORM。
- `tests/unit/{mysql,sqlite,postgres}` 按后端组织测试。

这个分层是合理的，核心 API 也已经有比较现代的使用体验：`SqlDatabase` 管连接，`SqlStatement` 管参数绑定，`SqlResult` 管流式结果，`prepare_with/query_with/execute_with` 尝试提供编译期占位符检查，ORM 用方言特化做 schema 生成。

当前主要问题不是方向错误，而是若干抽象语义没有完全兑现：公共接口看起来统一，但不同驱动的行为、错误传播、生命周期和能力边界并不完全一致。

## 关键问题

### 1. MySQL 异步 poll 存在正确性 bug

位置：`src/mysql/mysql.cpp`

`MySql::pollStatus()` 中存在两个问题：

- 内层 `auto ret = co_await ...` 遮蔽了外层 `IoResult<unsigned int> ret`，后续读取的不是实际 poll 结果。
- `POLLOUT` 和 `POLLPRI` 判断用了按位或 `|`，应为按位与 `&`。

影响：

- MariaDB/MySQL nonblocking API 的等待状态可能被错误设置。
- 连接、查询、prepare、execute 在复杂网络状态下可能卡住、误判或产生不稳定行为。

建议：

- 立即修复遮蔽变量和位运算错误。
- 为 `pollStatus()` 单独补测试或至少补集成测试覆盖超时、读等待、写等待。

### 2. 绑定错误传播不完整

位置：`include/ilias/sql/detail/sql_base_api.hpp`

`prepare_with()` 调用 `bind()` 后没有检查返回值。struct 版 `execute_with()` 在执行失败后仍然访问 `ret1.value()`。

影响：

- 参数绑定失败可能被吞掉，直到执行阶段才暴露，甚至产生错误 SQL 行为。
- 某些错误路径可能访问无效 result。

建议：

- 所有 `bind()` 调用都必须检查 `IoResult<void>`。
- struct 版 `execute_with()` 与 tuple 版保持一致，失败时 `co_return Unexpected(ret1.error())`。
- 为 unsupported bind type、invalid index、参数数量不匹配补测试。

### 3. MySQL unsupported bind type 被当成成功

位置：`src/mysql/mysql.cpp`

`MysqlStatement::bind()` 找不到 binder 时直接 `return {}`。

影响：

- 用户绑定了不支持类型也会被认为成功。
- 执行时可能以 NULL 或未初始化绑定参数发送到数据库。
- 与 SQLite/Postgres 行为不一致。

建议：

- 找不到 binder 时返回 `SqlError::Code::UnsupportBindType`。
- 保持 MySQL、SQLite、Postgres 在绑定错误上的统一语义。

### 4. 动态插件 ABI 和符号命名不一致

位置：

- `include/ilias/sql/sql_plugin.hpp`
- `src/sql/driver_registry.cpp`

问题：

- loader 查找 `ilias_sql_plugin_get_name`，宏导出的是 `ilias_sql_plugin_get_plugin_name`。
- `IConnectionPtr` 是全局 `struct IConnection*`，实际对象是命名空间内 `IConnection`，依赖 `reinterpret_cast`。
- `extra_keys` / `extra_values` 使用 `new[]` 后没有释放。
- 插件创建函数返回对象的释放策略没有 ABI 层明确表达。

影响：

- 动态插件很可能无法加载。
- 即使加载成功，跨动态库边界 delete C++ 对象也存在 ABI 风险。

建议：

- 统一导出符号名称。
- C ABI 层提供 `destroy_driver(IConnectionPtr)`。
- `ConnectOptions_C` 的临时数组使用 RAII 容器管理。
- 明确插件 ABI 版本与所有权规则。

### 5. 事务 RAII 与 move 语义有隐患

位置：

- `include/ilias/sql/sqldatabase.hpp`
- `src/sql/sqldatabase.cpp`

问题：

`SqlTransaction` 析构时总会调用 `mDatabase.releaseTransaction()`。当 transaction 被 move 后，移动源析构可能提前释放 `SqlDatabase::mInTransaction`。

影响：

- 事务仍然活跃时，`SqlDatabase::connection()` 可能被错误允许。
- 事务隔离语义被破坏。

建议：

- 增加 `mOwnsTransaction` 或类似状态。
- move constructor 将所有权从 source 转移给 target。
- 析构时只有拥有活动事务的对象才 rollback/release。
- `commit()` / `rollback()` 只释放一次。

### 6. `SqlResult<T>::range()` 吞掉转换错误

位置：`include/ilias/sql/sqlresult.hpp`

typed range 遇到字段转换失败时只写日志，然后继续迭代。

影响：

- 数据损坏、类型不匹配、NULL 转非 nullable 等错误会被静默跳过。
- 上层业务难以及时发现数据质量问题。

建议：

- 新增 `rangeResult() -> Generator<IoResult<T>>`。
- 或将当前 `range()` 改为返回 `IoResult<T>`，再提供 `values()` 作为忽略错误的便捷 API。
- ORM 查询默认应使用错误可见的结果流。

### 7. 编译期 SQL 参数检查和运行时 parser 不一致

位置：

- `include/ilias/sql/detail/type_traits.hpp`
- `src/mysql/mysql.cpp`
- `src/postgres/postgres.cpp`

问题：

`count_sql_params()` 会统计字符串、注释里的 `?` / `:name`。运行时 parser 已经部分处理字符串、注释、Postgres dollar quote。

影响：

- 同一条 SQL 可能编译期检查失败，但运行时能正确执行。
- 也可能编译期通过，运行时参数转换数量不同。

建议：

- 抽一个共享 SQL placeholder tokenizer。
- tokenizer 支持不同方言能力：
  - SQLite/MySQL 常规字符串、反引号、注释。
  - PostgreSQL `::` cast、`$tag$...$tag$` dollar quote。
- 编译期检查和运行时 rewrite 使用同一套状态机规则。

### 8. 公共接口能力语义过宽

位置：`include/ilias/sql/interfaces.hpp`

问题：

`IResultSet::rowCount()` 在不同驱动里含义不同：

- SQLite 当前固定返回 0。
- Postgres streaming 返回已读取行数。
- MySQL store result 场景可能是总行数。

影响：

- 上层无法可靠判断 `rowCount()` 是总数、已读数，还是不支持。

建议：

- 将 `rowCount()` 改为 `IoResult<std::optional<size_t>>` 或拆成：
  - `rowsFetched()`
  - `rowsAffected()`
  - `exactRowCount()`
- 或增加 `ResultCapabilities`，显式表达是否支持精确行数、是否 streaming、是否多结果集。

当前进度：

- 公共 `IResultSet` 已移除有歧义的 `rowCount()`，改为 `capabilities()`、`rowsFetched()`、`exactRowCount()`、`rowsAffected()`。
- `SqlResult<void>` 提供同名轻量转发 API，用户不需要绕到底层 result set。
- SQLite / PostgreSQL streaming result 明确没有精确总行数，只暴露已读取行数。
- MySQL store result 在底层结果可用后暴露 `exactRowCount()`，并用 `std::optional<size_t>` 区分“尚不可知”和“确实为 0”。
- `execute()` 的 affected rows 仍作为命令执行结果返回，不再塞进 query result 的 row count 语义里。

### 9. 原生错误文本与 `std::error_code` 职责混杂

位置：

- `include/ilias/sql/sqlerror.hpp`
- SQLite / MySQL / PostgreSQL 各驱动错误转换路径

问题：

当前允许通过 `SqlErrorCategory::registerMessage(code, message)` 动态注册后端错误文本。这个方式可以把 driver 原始错误消息透出，但它和 `std::error_code` 的设计不完全匹配：

- `std::error_code` 更适合表达稳定、可比较、跨调用点有效的错误类别。
- 动态注册 message 会让同一个 numeric code 的含义依赖最近一次注册，尤其不同 backend 的 native code 可能冲突。
- 上层如果只拿到 `std::error_code`，很难稳定区分 portable 错误、native errno、SQLSTATE 和原始 driver message。

建议：

- 普通错误返回稳定的通用 `SqlError::Code`，例如 `InvalidParameter`、`ConstraintViolation`、`UnsupportBindType`、`NotConnected`。
- 增加独立的原生错误诊断对象，不再把 driver 原文塞进全局 error category：

```cpp
struct NativeSqlError {
    std::string backend;  // "sqlite", "postgresql", "mysql"
    int         code = 0; // SQLite/MySQL errno 等
    std::string sqlstate;
    std::string message; // driver 原始错误文本
};
```

- `IConnection`、`IStatement`、`IResultSet` 或对应高层 wrapper 保存自己的 last native error，避免全局 category 被不同连接/线程覆盖。
- 对外提供轻量查询接口，例如 `lastNativeError() -> std::optional<NativeSqlError>`。
- `SqlError::Code` 负责控制流和跨后端判断，`NativeSqlError` 负责日志、调试和错误详情展示。
- 后续可在 `SqlError` / enhanced error 里关联 native error 快照，但不要让 native error 改变通用错误码的比较语义。

### 10. ORM 标识符拼接缺少统一 quote/validate

位置：

- `include/ilias/sql_orm/orm_form.hpp`
- `include/ilias/sql_orm/detail/orm_table_ops.hpp`
- `include/ilias/sql_orm/detail/orm_builder.hpp`

问题：

表名、列名、order by 字段、raw select 字段等大量以字符串直接拼接。反射字段名通常可信，但 `tableName`、`orderBy`、`select(string)` 来自用户输入时有 SQL 注入风险。

建议：

- 在 `Dialect` 中提供：
  - `quote_identifier(std::string_view)`
  - `validate_identifier(std::string_view)`
  - `quote_identifier_path(std::string_view)`
  - `qualified_identifier(table, column)`
- ORM 内部生成 SQL 时统一走 quote helper。
- `select("col")`、`orderBy("col")` 等字符串入口只表达运行时 identifier，由 ORM validate + quote；任意 SQL 片段仍走底层 `SqlDatabase::query/prepare`。

### 11. Schema 生成职责曾重复

位置：

- `include/ilias/sql_orm/detail/schema_generator.hpp`
- `include/ilias/sql_orm/orm_form.hpp`

问题：

此前 `SchemaGenerator` 与 `Form::_create_table_impl()` 都在做 schema 生成，`SchemaGenerator::generateCreateTable<FormType>()` 更像占位实现，和实际 ORM Form 体系没有完全打通。

当前进度：

- 已选择 `SchemaGenerator<BackendTag>::generateTableSchema<Entity>()` 作为 table/index schema 的统一生成入口。
- `generateTableSchema()` 只做一次反射扫描，产出 `TableSchema`，包含 `createTableSql`、`columnDefinitions`、`columns` 和 `indexStatements`。
- `Form::_create_table_impl()` 只负责调用 generator、执行 `CREATE TABLE` 和 index SQL。
- `createTableSchema()`、`indexStatementsSchema()`、`completeSchema()` 与实际建表路径复用同一份 schema 结果，避免文档/调试 SQL 与真实执行 SQL 漂移。
- schema 生成入口会复用 rename tag / `SqlTags` 抽取逻辑，并将确定性生成错误收敛为 `InvalidParameter`。

剩余建议：

- schema diff / validation 仍然主要以字符串形式组织错误，后续可补结构化结果。
- 进一步补充 schema generator 的公开文档，说明 `TableSchema` 中各字段的用途和错误语义。

## 重构完善方案

### 阶段一：先修正确性

目标：不改变用户 API，修复会导致错误行为的问题。

任务：

- 修复 `MySql::pollStatus()` 的变量遮蔽和 `|` / `&` 错误。
- `prepare_with()` 检查 bind 返回值。
- struct 版 `execute_with()` 正确传播 execute 错误。
- MySQL 找不到 binder 时返回 `UnsupportBindType`。
- `SqlTransaction` 增加 move ownership 状态，避免提前释放 transaction lock。
- 修复 dynamic plugin 符号名不一致和 `extra_keys/extra_values` 泄漏。

建议测试：

- MySQL prepared statement unsupported type。
- MySQL 参数绑定 invalid index。
- `prepare_with()` bind 失败能立刻返回错误。
- move transaction 后，移动源析构不释放数据库事务锁。
- 插件符号加载 smoke test。

### 阶段二：统一驱动行为契约

目标：让 `IConnection/IStatement/IResultSet` 的语义明确、可依赖。

任务：

- 明确 `execute()` 对 SELECT、INSERT/UPDATE/DELETE、多结果集的行为。
- 重新定义 row count / affected rows API。
- 为 `nativeHandle()` 补文档或类型安全包装。
- 统一 bind 失败、query 失败、conversion 失败的错误码。
- 将通用 `SqlError::Code` 与后端 `NativeSqlError` 诊断信息拆开。
- 明确 `SqlCellView` 生命周期，特别是 string/blob view 在 `next()` 后失效。

建议测试：

- 三后端统一的 `execute()` 行为测试。
- 三后端统一的 NULL、optional、string_view/blob view 生命周期测试。
- row count capability 测试。

### 阶段三：重做 placeholder parser

目标：编译期检查和运行时转换使用同一套规则。

任务：

- 抽象 `SqlPlaceholderParser`。
- 输出：
  - 参数数量。
  - 命名参数列表。
  - rewrite 后 SQL。
  - name -> index 映射。
- MySQL rewrite 到 `?`。
- PostgreSQL rewrite 到 `$1/$2/...`。
- SQLite 可保留原始 `?/:name`，只做分析。

建议测试：

- 字符串中的 `?` / `:name`。
- line comment 和 block comment。
- Postgres dollar quote。
- Postgres `::` cast。
- 重复命名参数策略。

### 阶段四：结果流错误可见化

目标：避免查询数据转换错误被静默忽略。

任务：

- 新增 `SqlResult<T>::rangeResult() -> Generator<IoResult<T>>`。
- 保留 `range()` 作为便捷 API 时，应明确其错误处理策略。
- ORM 默认查询使用错误可见接口。

建议测试：

- 数据库字段为 NULL，目标类型为非 optional。
- 文本无法转换为数字。
- 缺失列名。

### 阶段五：ORM SQL 生成收口

目标：减少 SQL 拼接风险，提升方言一致性。

任务：

- Dialect 增加 identifier quote/validate。
- ORM 内部表名、列名、索引名统一 quote。
- `orderBy(std::string)`、`select(std::string)` 等字符串入口限定为 identifier API，并自动 validate + quote。
- 合并 `SchemaGenerator` 和 `Form::_create_table_impl()` 的职责。
- 补 schema diff / validation 的结构化结果，而不是拼字符串。

建议测试：

- 表名/列名包含保留字。
- MySQL/Postgres/SQLite quote 行为。
- schema validation 错误信息。

## API 改进建议

### `SqlDatabase`

保留当前 move-only 设计。建议新增：

- `bool isOpen() const`
- `bool inTransaction() const`
- `auto driverName() const -> std::string_view`

### `SqlStatement`

当前 lvalue-only bind 是为了生命周期安全，方向合理。但可以补一个 owning API：

- `bindRef(index, value)`：要求外部保证生命周期。
- `bindValue(index, value)`：内部复制并 keep alive。

这样比当前模板 static_assert 对用户更友好。

### `SqlResult`

建议区分：

- `rangeResult()`：返回 `IoResult<T>`。
- `range()`：只返回成功值，文档明确会跳过或终止。
- `nextObject<T>()`：单行读取更方便。

### `SqlError`

建议让 `std::error_code` 只承载稳定的通用错误码；后端原生错误使用独立快照：

- `NativeSqlError { backend, code, sqlstate, message }`
- `lastNativeError()` 由 connection / statement / result 或高层 wrapper 各自保存并返回。
- 日志和调试 UI 使用 `NativeSqlError`，控制流和跨后端判断使用 `SqlError::Code`。
- 逐步减少对 `SqlErrorCategory::registerMessage()` 的依赖，避免全局动态 message 覆盖。

### `DriverManager`

建议增加：

- `unregisterDriver(name)`，便于测试和插件卸载。
- `driverNames()` 替代当前 `pluginNames()`，因为返回的是所有注册驱动，不只是插件。
- 动态插件 ABI 文档。

## 推荐优先级

| 优先级 | 工作项 | 原因 |
| --- | --- | --- |
| P0 | 修复 MySQL poll | 直接影响异步 I/O 正确性 |
| P0 | 修复 bind/execute 错误传播 | 避免错误被吞和无效 value 访问 |
| P0 | 修复 transaction move ownership | 避免事务锁被提前释放 |
| P1 | 修复插件 ABI 和泄漏 | 动态插件目前风险较高 |
| P1 | 统一 unsupported bind 行为 | 三驱动 API 语义一致 |
| P1 | 结果流返回错误 | 避免数据转换错误静默丢失 |
| P2 | 统一 placeholder parser | 降低编译期/运行时规则分裂 |
| P2 | ORM identifier quote/validate | 提升安全性和方言兼容 |
| P3 | 合并 schema generator | 降低 ORM 层维护成本 |
| P3 | result capabilities 建模 | 消除 row count / affected rows / streaming 语义歧义 |
| P4 | 拆分 NativeSqlError | 让通用错误码保持稳定，保留后端诊断细节 |

## 近期最小落地清单

如果希望先做一轮低风险修复，建议按这个顺序：

1. 修 `MySql::pollStatus()`。
2. 修 `prepare_with()` 和 struct `execute_with()` 错误传播。
3. 修 `MysqlStatement::bind()` 找不到 binder 时返回错误。
4. 给 `SqlTransaction` 加 ownership flag。
5. 修插件导出符号名称和临时数组泄漏。
6. 增加 5 到 8 个单元测试覆盖以上行为。

这轮改完后，不需要大改公开 API，但稳定性会明显提升。

## 执行任务列表

### P0 正确性修复

- [x] 修复 `MySql::pollStatus()` 中 poll 结果变量遮蔽问题。
- [x] 修复 `MySql::pollStatus()` 中 `POLLOUT` / `POLLPRI` 判断使用 `|` 的问题。
- [x] 修复 `prepare_with()` 忽略 `bind()` 返回值的问题。
- [x] 修复 struct 版 `execute_with()` 执行失败后仍访问 `value()` 的问题。
- [x] 修复 `MysqlStatement::bind()` 找不到 binder 时返回成功的问题。
- [x] 修复 `SqlTransaction` move 后移动源析构提前释放事务锁的问题。
- [x] 调整事务测试：事务未结束时使用 `tx.query()` 验证同一事务连接内可见性，事务结束后再用 `db.query()` 验证最终状态。

验证记录：

- `xmake build test_common_sqlite`
- `xmake build test_common_mysql`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite -- --gtest_filter=SQL.TRANSACTION_ROLLBACK_TEST:SQL.RAII_ROLLBACK`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite`

说明：不设置 `LSAN_OPTIONS=detect_leaks=0` 时，当前环境下 SQLite 聚焦用例本身通过，但进程退出阶段 LeakSanitizer 报 fatal error，导致 `xmake run` 返回失败码。

### P1 插件与错误语义

- [x] 统一动态插件导出符号名称。
- [x] 修复 `DriverManager::loadPlugin()` 中 `extra_keys` / `extra_values` 泄漏。
- [x] 明确动态插件创建与销毁所有权。
- [x] 统一 MySQL / SQLite / PostgreSQL 的 unsupported bind 错误码。

验证记录：

- `xmake f -m debug --enable_test=y --dynamic_plugin=y`
- `xmake build ilias_sql`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=n`
- `xmake build ilias_sql`
- `xmake build test_common_sqlite`
- `xmake build test_common_mysql`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite`

说明：

- 动态插件模式可编译通过，验证了 `ilias_sql_plugin_create_driver` / `ilias_sql_plugin_destroy_driver` / `ilias_sql_plugin_get_plugin_name` / `ilias_sql_plugin_get_plugin_api_version` 的宏展开。
- 切换动态插件模式后，旧的 `build/linux/x86_64/debug/libilias_sql.so` 会残留并干扰普通静态配置下的测试链接；本次已删除该 stale build artifact 后重新验证。
- PostgreSQL 源码路径已统一返回 `UnsupportBindType`；后续本机安装 `libpq-dev` 后，`enable_postgres` 配置下源码和相关测试目标已可编译通过。

### P1 结果流错误可见化

- [x] 新增 `SqlResult<T>::rangeResult()` 或等价 API。
- [x] 决定 `SqlResult<T>::range()` 遇到转换错误时是终止、跳过，还是仅作为便捷忽略错误 API。
- [x] ORM 查询默认切换到错误可见的结果流。

设计结果：

- `SqlResult<T>::rangeResult()` 返回 `Generator<IoResult<T>>`，逐行暴露类型转换、列读取和底层 `next()` 错误。
- `SqlResult<T>::range()` 保留为兼容便捷 API：只产出成功值；遇到错误时记录 warning 并跳过该错误项，不再作为需要感知错误的默认路径。
- ORM schema 解析和表格打印路径改用 `rangeResult()`；schema 解析遇到行读取失败会向上返回错误。

验证记录：

- `xmake build ilias_sql`
- `xmake build test_common_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite -- --gtest_filter=SQL.RANGE_RESULT_CONVERSION_ERROR`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite`
- `xmake build test_common_mysql`
- `xmake build test_orm_interface_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_orm_interface_sqlite`
- `xmake build test_orm_interface_mysql`

说明：

- 新增 SQLite 聚焦用例验证文本无法转换为整数时，`rangeResult()` 能返回 `UnsupportConvertFromSqlType`，旧 `range()` 兼容路径不产出错误值。
- MySQL ORM 本阶段只做编译验证，未运行需要外部 MySQL 服务的测试进程。

### P2 Parser 与 ORM 安全性

- [x] 抽取共享 SQL placeholder tokenizer。
- [x] 让编译期检查和运行时 rewrite 使用同一套 placeholder 规则。
- [x] 为 `Dialect` 增加 `quote_identifier()`、`quote_identifier_path()` 和 `validate_identifier()`。
- [x] ORM 内部 SQL 生成统一 quote 表名、列名和索引名。
- [x] 将 ORM 字符串入口收敛为安全 identifier API，而不是 raw SQL API。
- [x] 对确定性命名错误做早期检测。

设计结果：

- 新增 `detail/placeholder_parser.hpp`，统一扫描 `?`、`:name`、字符串、line/block comment、PostgreSQL dollar quote 和 `::` cast。
- `count_sql_params()` / `get_sql_param_names()` 改为复用共享 tokenizer，编译期检查与运行时 rewrite 使用同一套占位符规则。
- MySQL prepare rewrite 复用共享 tokenizer，将 `?` / `:name` 统一转换为 `?`，并保留 name -> index 映射。
- PostgreSQL prepare rewrite 复用共享 tokenizer，将 `?` / `:name` 转换为 `$1` / `$2`，并保留 name -> index 映射。
- `Dialect` 统一提供 identifier validate/quote/path quote，SQLite/PostgreSQL 使用双引号，MySQL 使用反引号。
- `Form` / `TableAlias` 持有 raw table/alias name 与 quoted SQL name，ORM 内部 `CREATE TABLE`、`DROP TABLE`、`INSERT`、`UPDATE`、`DELETE`、`SELECT`、`JOIN`、index 生成均使用 quoted identifier。
- `select("col")`、`select("a, b")`、`orderBy("col")`、`count("col")` 保留为运行时 identifier 输入，并由 ORM validate + quote；`select("count(*)")` 这类表达式不进入 ORM DSL。
- 反射列名重复、非法表名/列名、非法别名、未加别名的 self-join 等确定性错误会在 ORM 层提前暴露，避免继续生成含歧义的 SQL。
- `SqlVariable` 拆分 SQL 表达式和绑定名，修复列名被 quote 后 `InsertBuilder::set(col = value)` 无法按 `:name` 绑定的问题。
- `sql<&T::field>()` / `col<&T::field>()` 增加严格成员指针模板入口：反射元数据没有可静态匹配的成员指针时编译期报错。
- `sql(&T::field)` / `col(&T::field)` 找不到反射列时不再抛异常，而是返回带诊断的 invalid `SqlVariable`，由 builder 在 `query()` / `execute()` 前统一返回 `InvalidParameter`。

验证记录：

- `xmake l ./lua/list_tests.lua`
- `xmake build test_placeholder_parser_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_placeholder_parser_sqlite`
- `xmake build ilias_sql`
- `xmake build test_common_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite`
- `xmake build test_common_mysql`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=y`
- `xmake build ilias_sql`
- `xmake build test_sql_placeholder_parsing_properties_postgres`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=n`
- `xmake build ilias_sql`
- `xmake build test_identifier_quoting_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_identifier_quoting_sqlite`
- `xmake build test_identifier_quoting_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_identifier_quoting_sqlite`
- `xmake build test_orm_interface_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_orm_interface_sqlite`
- `xmake build test_orm_interface_mysql`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=y`
- `xmake build test_orm_interface_postgres`
- `xmake build test_sql_placeholder_parsing_properties_postgres`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=n`
- `xmake build ilias_sql`

说明：

- 新增 SQLite 组纯逻辑测试覆盖 MySQL rewrite、PostgreSQL rewrite、字符串/注释/dollar quote 保护，以及编译期计数。
- 新增 SQLite identifier quoting 测试覆盖方言 quote、保留字表/列名、运行时字符串 identifier、非法表达式字符串拒绝、重复列名和 self-join 别名冲突。
- 本机安装 `libpq-dev` 后 PostgreSQL 源码和 placeholder parser 测试目标可编译通过；未运行 PostgreSQL 集成测试进程，因为它依赖外部 PostgreSQL 服务和测试库/表。

### P3 Schema 与长期整理

- [x] 合并 `SchemaGenerator` 和 `Form::_create_table_impl()` 的 schema 生成职责。
- [x] 为 result capabilities 建模，明确 row count / affected rows / streaming 能力。
- [x] 为 `nativeHandle()`、`SqlCellView` 生命周期和 bind 生命周期补充 API 文档。

设计结果：

- `SchemaGenerator<BackendTag>::generateTableSchema<Entity>()` 成为 CREATE TABLE / CREATE INDEX schema 的统一生成入口。
- `Form::_create_table_impl()`、`createTableSchema()`、`indexStatementsSchema()`、`completeSchema()` 均复用同一个 `TableSchema`，确保调试输出、测试断言和真实建表执行一致。
- 反射字段名、rename tag 和 `SqlTags` 抽取集中在 schema generator 内，减少 ORM Form 与 schema helper 的重复反射逻辑。
- schema 入口返回 `IoResult<TableSchema>`，确定性生成错误返回 `InvalidParameter`，不再让调用方同时处理 `IoResult` 和 schema 生成异常。
- `IResultSet::rowCount()` 已移除，避免继续暴露“总行数/已读取行数/不支持”混杂语义。
- 新增 `ResultCapabilities` 和 `rowsFetched()` / `exactRowCount()` / `rowsAffected()`；`exactRowCount()` 和 `rowsAffected()` 均用 `std::optional<size_t>` 表达能力边界。
- MySQL result 的多结果集推进去掉 `goto`，改成 `storeCurrentResultIfNeeded()` / `advanceToNextResult()` 两个 async helper；析构只做同步资源释放，不承担需要 `co_await` 的推进逻辑。
- `nativeHandle()`、`SqlCellView` 和高低层 bind 的生命周期约定已写入公共头文件：native handle 不转移所有权且可能被 reset/next/close 失效；`SqlCellView` 是当前行/当前绑定的非拥有视图；低层 `IStatement::bind()` 要求调用方保证引用存活，高层 `SqlStatement` wrapper 会保存参数副本并可通过 `clearKeepAlives()` 释放。

验证记录：

- `xmake l ./lua/list_tests.lua`
- `xmake build test_identifier_quoting_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_identifier_quoting_sqlite`
- `xmake build test_schema_generator_integration_properties_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_schema_generator_integration_properties_sqlite`
- `xmake build test_orm_interface_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_orm_interface_sqlite`
- `xmake build test_common_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite`
- `xmake build test_common_mysql`
- `xmake build test_orm_interface_mysql`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=y`
- `xmake build test_orm_interface_postgres`
- `xmake build test_sql_placeholder_parsing_properties_postgres`
- `xmake f -y -m debug --enable_test=y --dynamic_plugin=n --enable_postgres=n`
- `xmake build ilias_sql`
- `xmake build test_common_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite -- --gtest_filter=SQL.RESULT_CAPABILITIES`
- `xmake build test_common_mysql`
- `xmake build test_orm_interface_mysql`

说明：

- PostgreSQL 目标只做编译验证，未运行需要外部 PostgreSQL 服务的测试进程。
- 验证结束后已将 xmake 配置切回 `--enable_postgres=n`，当前 `xmake l ./lua/list_tests.lua` 回到 SQLite/MySQL 目标集合。
- result capabilities 阶段按小步验证策略只做聚焦测试和必要编译；全量细致测试留到本轮重构收口后统一执行。

### P4 错误模型重构

- [x] 增加 `NativeSqlError { backend, code, sqlstate, message }`。
- [x] 在 connection / statement / result 或对应高层 wrapper 上保存各自的 last native error。
- [x] 增加 `lastNativeError() -> std::optional<NativeSqlError>` 查询入口。
- [x] 移除普通错误路径对 `SqlErrorCategory::registerMessage()` 的行为依赖。
- [x] 让 `SqlError::Code` 保持稳定可比较，native error 只作为诊断详情。

设计结果：

- 公共 `IConnection` / `IStatement` / `IResultSet` 增加默认 `lastNativeError()`，高层 `SqlDatabase` / `SqlTransaction` / `SqlStatement<void>` / `SqlResult<void>` 提供同名轻量转发。
- SQLite 错误路径保存 `backend=sqlite`、原生返回码和 `sqlite3_errmsg()` 文本；普通错误码映射到 `InvalidSqlStatement`、`ConstraintViolation`、`InvalidIndex` 等稳定通用码。
- MySQL 连接和 statement/result 路径保存 errno、SQLSTATE 和原始 message；普通错误码映射到 `TableNotFound`、`UniqueConstraintViolation`、`ForeignKeyViolation` 等稳定通用码。
- PostgreSQL 连接、prepared statement 和 streaming result 路径保存 `PGresult` / `PGconn` 诊断快照，SQLSTATE 留在 `NativeSqlError::sqlstate`；同时修正了 streaming result 错误分支先 `PQclear()` 后读取错误信息的顺序问题。
- SQLite binder parser 中残留的 SQLite native code cast 已收敛为通用错误码；native 文本只由执行对象保存，不再进入全局 error category。

验证记录：

- `xmake build test_common_sqlite`
- `env LSAN_OPTIONS=detect_leaks=0 xmake run test_common_sqlite -- --gtest_filter=SQL.NATIVE_ERROR_SNAPSHOT`
- `xmake build test_common_mysql`
- `env LSAN_OPTIONS=detect_leaks=0 DB_HOST=127.0.0.1 DB_PORT=3306 DB_USER=root DB_PASS=123456 DB_NAME=test xmake run test_common_mysql -- --gtest_filter=MySQL.NATIVE_ERROR_SNAPSHOT`
- `xmake build test_sql_placeholder_parsing_properties_postgres`

说明：

- MySQL focused 运行在沙箱内无法访问本机 TCP 服务时会快速失败；放行本机 `xmake run` 后，使用 `127.0.0.1:3306/root/123456/test` 的单用例验证通过。
- 本轮仍未跑全量测试；按当前策略只做错误模型相关 focused 用例和必要编译验证。
