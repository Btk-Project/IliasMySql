#pragma once

#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sql_orm/detail/console_table.hpp"

#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_condition.hpp"
#include "ilias/sql_orm/detail/orm_builder.hpp"

ILIAS_SQL_NS_BEGIN
namespace detail {
template <typename Derived, typename T, typename BackendTag>
class TableOperations {
public:
    // 获取子类的引用 (CRTP 核心)
    Derived       &derived() { return *static_cast<Derived *>(this); }
    const Derived &derived() const { return *static_cast<const Derived *>(this); }

    auto columnDefinitionSchema(int idx) const -> std::string {
        auto columns = derived().getColumnNames();
        if (idx >= columns.size() || idx < 0) {
            return "";
        }
        return columnDefinitionSchema(columns[idx]);
    }

    auto columnDefinitionSchema(std::string name) const -> std::string {
        std::string result;
        NEKO_NAMESPACE::Reflect<T>::forEach([&](const auto &field, std::string_view fname, const SqlTags &tags) {
            if (name == fname) {
                result = detail::SchemaGenerator<BackendTag>::template generateColumnDefinition<
                    std::decay_t<decltype(field)>>(fname, tags);
            }
        });
        return result;
    }

    auto createTableSchema() const -> IoResult<std::string> {
        auto tablename = derived().getTableName();
        return detail::SchemaGenerator<BackendTag>::template generateCreateTable<T>(tablename);
    }

    auto indexStatementsSchema() const -> std::vector<std::string> {
        auto                                         columns = derived().getColumnNames();
        auto                                         tags    = derived().getColumnTags();
        std::vector<std::pair<std::string, SqlTags>> columnTags;
        for (auto i = 0; i < static_cast<int>(columns.size()); ++i) {
            columnTags.emplace_back(columns[i], tags[i]);
        }
        return detail::SchemaGenerator<BackendTag>::generateIndexStatements(derived().getTableName(), columnTags);
    }

    auto completeSchema() const -> std::vector<std::string> {
        return detail::SchemaGenerator<BackendTag>::template generateCompleteSchema<T>(derived().getTableName());
    }

    /**
     * @brief Validate the SqlTags configuration for the entire table
     *
     * Checks all field configurations for conflicts and invalid combinations.
     * @return Vector of validation error messages (empty if valid)
     */
    static decltype(auto) validateTableConfiguration() {
        std::vector<std::string> errors;

        T obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const SqlTags &tags) {
            using rawType    = detail::strip_wrapper_t<decltype(field)>;
            auto fieldErrors = tags.getValidationErrors<rawType>();
            for (const auto &error : fieldErrors) {
                errors.push_back(std::string(name) + ": " + error);
            }
        });

        return errors;
    }

    static decltype(auto) getTimestampFields() {
        std::vector<std::string_view> timestampFields;
        T                             obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj,
                                            [&](const auto & /*field*/, std::string_view name, const SqlTags &tags) {
                                                if (tags.hasTimestampBehavior()) {
                                                    timestampFields.emplace_back(name);
                                                }
                                            });
        return timestampFields;
    }

    static decltype(auto) getCreatedAtFields() {
        std::vector<std::string_view> createdAtFields;
        T                             obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj,
                                            [&](const auto & /*field*/, std::string_view name, const SqlTags &tags) {
                                                if (tags.created_at) {
                                                    createdAtFields.emplace_back(name);
                                                }
                                            });
        return createdAtFields;
    }

    static decltype(auto) getUpdatedAtFields() {
        std::vector<std::string_view> updatedAtFields;
        T                             obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj,
                                            [&](const auto & /*field*/, std::string_view name, const SqlTags &tags) {
                                                if (tags.updated_at) {
                                                    updatedAtFields.emplace_back(name);
                                                }
                                            });
        return updatedAtFields;
    }

    // =========================================================
    // 1. 写入操作 (Insert, Update, Remove)
    // =========================================================

    // Insert: 批量插入
    template <typename Range>
        requires(std::ranges::range<Range> && std::is_same_v<std::decay_t<std::ranges::range_value_t<Range>>, T>)
    auto insert(Range &&items) -> IoTask<size_t> {
        if (std::empty(items))
            co_return 0;
        const auto              &first_item = *std::ranges::begin(items);
        std::vector<std::string> columnsToInsert;
        NEKO_NAMESPACE::Reflect<T>::forEach(
            first_item, [&](const auto &field, std::string_view name, const auto &tags) {
                // 如果字段是 created_at 并且它的值是空的，则跳过此列
                if (tags.created_at && is_sql_null(field) && Dialect<BackendTag>::support_timestamp_default()) {
                    return;
                }
                // 否则，将此列加入到 INSERT 语句中
                columnsToInsert.emplace_back(name); // 假设可以从tags获取列名
            });
        std::string rowPlaceholder = "(";
        for ([[maybe_unused]] int i = 0; i < (int)columnsToInsert.size(); ++i) {
            if (rowPlaceholder.back() != '(')
                rowPlaceholder += ", ";
            rowPlaceholder += "?";
        }
        rowPlaceholder += ")";
        std::vector<std::string> allRowsPlaceholder(std::size(items), rowPlaceholder);
        std::string sql = "INSERT INTO " + derived().getTableName() + " (" + detail::join_strs(columnsToInsert, ", ") +
                          ") VALUES " + detail::join_strs(allRowsPlaceholder, ", ");
        auto ret = co_await derived().db().prepare(sql);
        if (!ret)
            co_return Unexpected(ret.error());
        int                                bindIndex = 1;
        std::vector<std::shared_ptr<void>> binds;
        for (const auto &item : items) {
            if constexpr (Dialect<BackendTag>::support_timestamp_default()) {
                // if sql dialect support default timestamp, then we don't need to generate timestamp by ourselves
                NEKO_NAMESPACE::Reflect<T>::forEach(item, [&](const auto &field, const SqlTags &tags) {
                    using FieldType = std::decay_t<decltype(field)>;
                    if (tags.created_at && is_sql_null(field)) {
                        return;
                    }
                    SqlBinder<FieldType>::bind(**ret, bindIndex++, field);
                });
            }
            else {
                // if sql dialect doesn't support default timestamp, then we need to generate timestamp by ourselves
                NEKO_NAMESPACE::Reflect<T>::forEach(item, [&](const auto &field, const SqlTags &tags) {
                    using FieldType = std::decay_t<decltype(field)>;
                    if (tags.created_at) {
                        if constexpr (std::is_same_v<FieldType, std::string> || std::is_same_v<FieldType, SqlDate>) {
                            std::shared_ptr<FieldType> now = std::make_shared<FieldType>();
                            detail::TimestampUpdater {.created_at = true}(*now, tags);
                            SqlBinder<FieldType>::bind(**ret, bindIndex++, *now);
                            binds.push_back(now);
                            return;
                        }
                    }
                    SqlBinder<FieldType>::bind(**ret, bindIndex++, field);
                });
            }
        }
        co_return co_await ret->execute();
    }
    template <typename... Args>
        requires(std::is_constructible_v<T, Args...> && sizeof...(Args) > 0)
    auto emplace(Args &&...args) -> IoTask<size_t> {
        co_return co_await insert(std::array {T {std::forward<Args>(args)...}});
    }
    auto insert() {
        if constexpr (Dialect<BackendTag>::support_timestamp_default()) {
            return detail::InsertBuilder<T>(derived().db(), derived().getTableName(), derived().getColumnNames());
        }
        else {
            auto insertBuilder =
                detail::InsertBuilder<T>(derived().db(), derived().getTableName(), derived().getColumnNames());
            T obj;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](auto &field, std::string_view name, const SqlTags &tags) {
                if (tags.created_at) {
                    detail::TimestampUpdater {.created_at = true}(field, tags);
                    insertBuilder.set(detail::SqlVariable(name) = std::move(field));
                }
            });
        }
    }
    // Update Builder
    auto update() {
        if constexpr (Dialect<BackendTag>::support_timestamp_update()) {
            return detail::UpdateBuilder(derived().db(), derived().getTableName());
        }
        else {
            auto updateBuilder = detail::UpdateBuilder(derived().db(), derived().getTableName());
            T    obj;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](auto &field, std::string_view name, const SqlTags &tags) {
                if (tags.updated_at) {
                    detail::TimestampUpdater {.updated_at = true}(field, tags);
                    updateBuilder.set(detail::SqlVariable(name) = std::move(field));
                    return;
                }
            });
            return updateBuilder;
        }
    }
    // Remove Builder
    auto remove() { return detail::DeleteBuilder(derived().db(), derived().getTableName()); }

    // =========================================================
    // 2. 查询操作 (Select, Count)
    // =========================================================

    // Select
    template <typename... Us, template <typename U> typename... Ts>
        requires(detail::HasSqlMethod<Ts<Us>> && ...)
    auto select(Ts<Us>... args) const {
        return detail::ProjectedSelectBuilder<Us...>(derived().db(), derived().tableRef(), {args.sql()...});
    }

    template <typename... Ts>
        requires(detail::HasSqlMethod<Ts> && ...)
    auto select(Ts... args) const {
        return detail::ProjectedSelectBuilder<>(derived().db(), derived().tableRef(), {args.sql()...});
    }

    auto select() const {
        auto builder = detail::ProjectedSelectBuilder<T>(derived().db(), derived().tableRef());
        return builder;
    }

    auto select(const std::string &columns) const {
        auto builder = detail::SelectBuilder(derived().db(), derived().tableRef(), {columns});
        return builder;
    }

    auto count() const {
        return detail::ProjectedSelectBuilder<int>(derived().db(), derived().tableRef(), {"COUNT(*)"});
    }

    auto count(const std::string &column) const {
        return detail::ProjectedSelectBuilder<int>(derived().db(), derived().tableRef(), {"COUNT(" + column + ")"});
    }

    template <typename U>
    auto count(detail::TypedColumn<U> column) const {
        return detail::ProjectedSelectBuilder<int>(derived().db(), derived().tableRef(),
                                                   {"COUNT(" + column.sql() + ")"});
    }

    // =========================================================
    // 3. 联表操作 (Join)
    // =========================================================

    template <typename TableT>
    auto join(TableT &other, const std::string &type = "INNER") {
        return detail::JoinedSelectBuilder<Derived, TableT>(derived(), other, type);
    }

    template <typename TableT>
    auto leftJoin(TableT &other) {
        return join(other, "LEFT");
    }
    template <typename TableT>
    auto rightJoin(TableT &other) {
        return join(other, "RIGHT");
    }

    template <typename M>
    auto getColumnIndex(M T::*memberPtr) const -> int {
        T             *tmp  = nullptr;
        std::ptrdiff_t ptr  = (char *)&(tmp->*memberPtr) - (char *)tmp;
        auto           item = derived().getColumnIndex().find(ptr);
        if (item == derived().getColumnIndex().end())
            return -1;
        return item->second;
    }

    template <typename M>
    auto getColumnName(M T::*memberPtr) const -> IoResult<std::string> {
        auto index = getColumnIndex(memberPtr);
        if (index == -1)
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
        return derived().getColumnNames().at(index);
    }

    template <typename M>
    auto getColumnTag(M T::*memberPtr) const -> IoResult<SqlTags> {
        auto index = getColumnIndex(memberPtr);
        if (index == -1)
            return Unexpected(std::make_error_code(std::errc::invalid_argument));
        return derived().getColumnNames().at(index);
    }

    // =========================================================
    // 4. 列访问 DSL (col, sql)
    // =========================================================

    template <typename M>
    auto col(M T::*memberPtr) const {
        return detail::TypedColumn<std::decay_t<M>>(derived().getAlias() + "." + getColumnName(memberPtr).value());
    }

    template <typename M>
    auto sql(M T::*memberPtr) const {
        return detail::TypedColumn<std::decay_t<M>>(getColumnName(memberPtr).value());
    }

    auto print(std::ostream &stream = std::cout) -> Task<void> {
        co_return co_await print(50, stream); // 默认列宽限制为50字符
    }

    auto print(size_t maxColumnWidth, std::ostream &stream = std::cout) -> Task<void> {
        auto ret = co_await select().query();
        if (!ret) {
            ILIAS_ERROR("ilias-sql", "Print failed: {}", ret.error().message());
            co_return;
        }
        detail::ConsoleTable table(derived().tableRef(), derived().getColumnNames(), maxColumnWidth);
        SqlResult<T>         res = std::move(ret.value());
        ilias_for_await([[maybe_unused]] auto obj, res.range()) {
            std::vector<std::string> rowStrings;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field) {
                using FieldType = std::decay_t<decltype(field)>;
                if constexpr (std::is_same_v<FieldType, std::vector<char>> || std::is_same_v<FieldType, SqlBlob>) {
                    rowStrings.push_back("(BLOB " + std::to_string(field.size()) + " bytes)");
                }
                else if constexpr (requires(FieldType field) { detail::to_string_view(field); }) {
                    rowStrings.push_back(detail::to_string_view(field));
                }
                else {
                    static_assert(std::is_void_v<FieldType>, "Field type is not convertible to string");
                }
            });
            table.addRow(rowStrings);
        }
        table.print(stream);
        co_return;
    }
};
} // namespace detail
ILIAS_SQL_NS_END
