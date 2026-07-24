# anime-land 所需 ilias-sql 能力清单

这份清单定义 anime-land 对 ilias-sql 的完整依赖边界。应用层不得用后端 SQL 临时补齐其中任何一项。

## 连接与事务

- 统一异步打开、关闭、prepare/query/execute 和事务接口，覆盖 SQLite、SQLCipher 与 MySQL/MariaDB。
- SQLCipher 密钥由连接选项交给 SQLite driver；密钥是否可用由后续真实数据库操作暴露，不要求应用执行能力探测 PRAGMA。
- SQLite 外键等连接行为通过 driver option 显式配置；不得修改驱动默认值，应用也不执行 `PRAGMA foreign_keys`。
- driver 不隐式选择 journal mode；WAL 等性能/持久化策略不属于 anime-land 的正确性前置条件。

## Form 生命周期

- `Form::create_if_not_exists()` 能在数据库或事务上幂等创建反射声明的表，并直接返回可立即使用的 Form；不得在内部追加 `attach()` 或探测查询。
- `Form::attach()` 是兼容性附加：只要求所有映射列存在且类型可由目标 C++ 字段读取。
- attach 必须允许额外表、额外列，以及 NOT NULL、PRIMARY KEY、FOREIGN KEY、UNIQUE、DEFAULT、CHECK 和索引差异。
- 缺少映射列或列类型确实不可读取时，attach 返回包含表名和列名诊断的错误。
- 默认 attach 不推测未来 SQL 会写入什么值，因此不比较两侧的业务 CHECK 或其他值域约束。只有能够静态证明会让 Form 的所有合法读写发生固定冲突时，才值得定义额外的显式严格模式。
- 在没有具体、可解释的冲突类别前，不预设 strict level 枚举；将来增加的严格模式也不得改变默认兼容 attach 的语义。
- `Form::bind()` 在已经确认表可用后，把同一 Record 绑定到 `SqlDatabase` 或 `SqlTransaction`，不执行 Schema I/O。
- `getTableName()` 可用于把长期 Form 的关系名传给事务内 Form，避免业务代码重复散落表名。

## Schema 生成

- 反射元数据可以生成普通列、可空列、默认值、自增主键、复合主键、复合索引、外键及删除动作。
- Schema generator 通过 dialect 生成后端 SQL，调用方只提供 Record 和关系名。
- `create_if_not_exists()` 面对已存在的表时按 `IF NOT EXISTS` 语义成功；创建声明中的约束不被反向当作 attach 的等价条件。

## 类型化读写

- Form 支持完整 Record 与投影查询、where、join、排序、limit/offset、insert、update 和 delete。
- 所有构建器同时支持 `SqlDatabase` 和 `SqlTransaction`。
- insert 支持按列赋值，避免依赖数据库额外列或列顺序。
- upsert 支持多列冲突目标、替换、`COALESCE` 合并、greatest 合并和 do-nothing，并由 dialect 生成 SQLite/MySQL 语法。
- 数据库存在额外列时，只要这些列有默认值或允许 NULL，Form 的显式列写入不受影响。

## 错误与诊断

- prepare、bind、query、execute、结果解码和事务提交的错误必须原样向上传递。
- wrapper 暴露最近一次后端原生错误码和消息，便于区分缺表、缺列、类型错误和物理约束失败。
- attach 不代替真实写入验证；例如 upsert 所需 UNIQUE 不存在时，由 upsert 返回数据库真实错误。

## 验收矩阵

- 既有表比 Record 多表或多列：attach 成功。
- Record 与既有表的 UNIQUE/主键/NULL/default/check/index 声明不同：attach 成功。
- 缺少 Record 实际读取的列：attach 失败并指出列名。
- 映射列类型不可读取：attach 失败并指出列名和数据库类型。
- 既有表经 `create_if_not_exists()` 打开：直接返回 Form，且不追加 attach、不删除、不重建、不清洗数据。
- SQLite driver option 可以显式配置外键行为，未提供 option 时保持 SQLite 默认值。
- 复合键 upsert 在 SQLite 与 MySQL 生成正确语法，缺少物理唯一约束时错误来自实际 execute。
- 事务内 bind 后的 CRUD 与数据库直属 Form 行为一致。
