#pragma once

#include <nekoproto/serialization/reflection.hpp>
#include <nekoproto/serialization/to_string.hpp>
#include "ilias/sql_orm/detail/schema_generator.hpp"
#include "ilias/sql_orm/detail/console_table.hpp"

#include "ilias/sql_orm/detail/orm_types.hpp"
#include "ilias/sql_orm/detail/orm_condition.hpp"
#include "ilias/sql_orm/detail/orm_builder.hpp"

#include <initializer_list>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

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
        T           obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view fname, const auto &tags) {
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                const auto columnName = detail::reflectedFieldName(fname, tags);
                if (name == columnName) {
                    result = detail::SchemaGenerator<BackendTag>::template generateColumnDefinition<
                        std::decay_t<decltype(field)>>(columnName, detail::extractSqlColumnMetadata(tags));
                }
            }
        });
        return result;
    }

    auto createTableSchema(bool ifNotExists = false) const -> IoResult<std::string> {
        auto tablename = derived().getTableName();
        ILIAS_TRY(auto schema,
                  detail::SchemaGenerator<BackendTag>::template generateTableSchema<T>(tablename, ifNotExists));
        return schema.createTableSql;
    }

    auto indexStatementsSchema() const -> std::vector<std::string> {
        auto schema = detail::SchemaGenerator<BackendTag>::template generateTableSchema<T>(derived().getTableName());
        if (!schema) {
            return {};
        }
        return schema->indexStatements;
    }

    auto completeSchema() const -> std::vector<std::string> {
        auto schema = detail::SchemaGenerator<BackendTag>::template generateTableSchema<T>(derived().getTableName());
        if (!schema) {
            return {};
        }
        return schema->completeStatements();
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
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view name, const auto &tags) {
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                using rawType    = detail::strip_wrapper_t<decltype(field)>;
                auto metadata    = detail::extractSqlColumnMetadata(tags);
                auto fieldErrors = metadata.template getValidationErrors<rawType>();
                for (const auto &error : fieldErrors) {
                    errors.push_back(std::string(detail::reflectedFieldName(name, tags)) + ": " + error);
                }
            }
        });

        return errors;
    }

    static decltype(auto) getTimestampFields() {
        std::vector<std::string_view> timestampFields;
        T                             obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto & /*field*/, std::string_view name, const auto &tags) {
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                auto sqlTags = detail::extractSqlTags(tags);
                if (sqlTags.hasTimestampBehavior()) {
                    timestampFields.emplace_back(detail::reflectedFieldName(name, tags));
                }
            }
        });
        return timestampFields;
    }

    static decltype(auto) getCreatedAtFields() {
        std::vector<std::string_view> createdAtFields;
        T                             obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto & /*field*/, std::string_view name, const auto &tags) {
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                auto sqlTags = detail::extractSqlTags(tags);
                if (sqlTags.created_at) {
                    createdAtFields.emplace_back(detail::reflectedFieldName(name, tags));
                }
            }
        });
        return createdAtFields;
    }

    static decltype(auto) getUpdatedAtFields() {
        std::vector<std::string_view> updatedAtFields;
        T                             obj;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto & /*field*/, std::string_view name, const auto &tags) {
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                auto sqlTags = detail::extractSqlTags(tags);
                if (sqlTags.updated_at) {
                    updatedAtFields.emplace_back(detail::reflectedFieldName(name, tags));
                }
            }
        });
        return updatedAtFields;
    }

    static auto quoteRuntimeColumn(std::string_view column) -> std::string {
        return Dialect<BackendTag>::quote_identifier_path(column);
    }

    static auto quoteRuntimeColumns(std::string_view columns) -> std::vector<std::string> {
        std::vector<std::string> quoted;
        std::size_t              start = 0;
        while (start <= columns.size()) {
            const auto comma = columns.find(',', start);
            quoted.push_back(quoteRuntimeColumn(columns.substr(start, comma - start)));
            if (comma == std::string_view::npos) {
                break;
            }
            start = comma + 1;
        }
        return quoted;
    }

    static auto quoteRuntimeColumns(std::initializer_list<std::string_view> columns) -> std::vector<std::string> {
        std::vector<std::string> quoted;
        quoted.reserve(columns.size());
        for (auto column : columns) {
            quoted.push_back(quoteRuntimeColumn(column));
        }
        return quoted;
    }

    static auto quoteRuntimeColumns(const std::vector<std::string> &columns) -> std::vector<std::string> {
        std::vector<std::string> quoted;
        quoted.reserve(columns.size());
        for (const auto &column : columns) {
            quoted.push_back(quoteRuntimeColumn(column));
        }
        return quoted;
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
        std::vector<std::string> quotedColumnsToInsert;
        NEKO_NAMESPACE::Reflect<T>::forEach(
            first_item, [&](const auto &field, std::string_view name, const auto &tags) {
                if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                    const auto sqlTags    = detail::extractSqlTags(tags);
                    const auto columnName = detail::reflectedFieldName(name, tags);
                    // 如果字段是 created_at 并且它的值是空的，则跳过此列
                    if (sqlTags.created_at && is_sql_null(field) && Dialect<BackendTag>::support_timestamp_default()) {
                        return;
                    }
                    // 否则，将此列加入到 INSERT 语句中
                    columnsToInsert.emplace_back(columnName);
                    quotedColumnsToInsert.emplace_back(Dialect<BackendTag>::quote_identifier(columnName));
                }
            });
        std::string rowPlaceholder = "(";
        for ([[maybe_unused]] int i = 0; i < (int)columnsToInsert.size(); ++i) {
            if (rowPlaceholder.back() != '(')
                rowPlaceholder += ", ";
            rowPlaceholder += "?";
        }
        rowPlaceholder += ")";
        std::vector<std::string> allRowsPlaceholder(std::size(items), rowPlaceholder);
        std::string sql = "INSERT INTO " + derived().tableRef() + " (" +
                          detail::join_strs(quotedColumnsToInsert, ", ") + ") VALUES " +
                          detail::join_strs(allRowsPlaceholder, ", ");
        auto ret = co_await derived().db().prepare(sql);
        if (!ret)
            co_return Err(ret.error());
        int                                bindIndex = 1;
        std::vector<std::shared_ptr<void>> binds;
        for (const auto &item : items) {
            if constexpr (Dialect<BackendTag>::support_timestamp_default()) {
                // if sql dialect support default timestamp, then we don't need to generate timestamp by ourselves
                NEKO_NAMESPACE::Reflect<T>::forEach(item, [&](const auto &field, const auto &tags) {
                    if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                        using FieldType    = std::decay_t<decltype(field)>;
                        const auto sqlTags = detail::extractSqlTags(tags);
                        if (sqlTags.created_at && is_sql_null(field)) {
                            return;
                        }
                        SqlBinder<FieldType>::bind(**ret, bindIndex++, field);
                    }
                });
            }
            else {
                // if sql dialect doesn't support default timestamp, then we need to generate timestamp by ourselves
                NEKO_NAMESPACE::Reflect<T>::forEach(item, [&](const auto &field, const auto &tags) {
                    if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                        using FieldType    = std::decay_t<decltype(field)>;
                        const auto sqlTags = detail::extractSqlTags(tags);
                        if (sqlTags.created_at) {
                            if constexpr (std::is_same_v<FieldType, std::string> ||
                                          std::is_same_v<FieldType, SqlDate>) {
                                std::shared_ptr<FieldType> now = std::make_shared<FieldType>();
                                detail::TimestampUpdater {.created_at = true}(*now, sqlTags);
                                SqlBinder<FieldType>::bind(**ret, bindIndex++, *now);
                                binds.push_back(now);
                                return;
                            }
                        }
                        SqlBinder<FieldType>::bind(**ret, bindIndex++, field);
                    }
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
        const auto columnNames = derived().getColumnNames();
        std::vector<std::string> columnRefs;
        columnRefs.reserve(columnNames.size());
        for (const auto &column : columnNames) {
            columnRefs.push_back(Dialect<BackendTag>::quote_identifier(column));
        }
        if constexpr (Dialect<BackendTag>::support_timestamp_default()) {
            return detail::InsertBuilder<T>(derived().db(), derived().tableRef(),
                                            columnNames, std::move(columnRefs));
        }
        else {
            auto insertBuilder = detail::InsertBuilder<T>(
                derived().db(), derived().tableRef(), columnNames,
                std::move(columnRefs));
            T obj;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](auto &field, std::string_view name, const auto &tags) {
                if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                    const auto sqlTags    = detail::extractSqlTags(tags);
                    const auto columnName = detail::reflectedFieldName(name, tags);
                    if (sqlTags.created_at) {
                        detail::TimestampUpdater {.created_at = true}(field, sqlTags);
                        insertBuilder.set(detail::SqlVariable(Dialect<BackendTag>::quote_identifier(columnName),
                                                              std::string(columnName)) = std::move(field));
                    }
                }
            });
            return insertBuilder;
        }
    }
    auto upsert() {
        return detail::UpsertBuilder<T, BackendTag>(derived().db(), derived().tableRef());
    }
    // Update Builder
    auto update() {
        if constexpr (Dialect<BackendTag>::support_timestamp_update()) {
            return detail::UpdateBuilder(derived().db(), derived().tableRef());
        }
        else {
            auto updateBuilder = detail::UpdateBuilder(derived().db(), derived().tableRef());
            T    obj;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](auto &field, std::string_view name, const auto &tags) {
                if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                    const auto sqlTags    = detail::extractSqlTags(tags);
                    const auto columnName = detail::reflectedFieldName(name, tags);
                    if (sqlTags.updated_at) {
                        detail::TimestampUpdater {.updated_at = true}(field, sqlTags);
                        updateBuilder.set(detail::SqlVariable(Dialect<BackendTag>::quote_identifier(columnName),
                                                              std::string(columnName)) = std::move(field));
                        return;
                    }
                }
            });
            return updateBuilder;
        }
    }

    template <typename Column, typename Value>
        requires(detail::HasSqlMethod<Column> && detail::SqlBindable<Value>)
    auto assignCoalesced(const Column &column, Value &&value) const
        -> detail::SqlAssignment {
        if (!detail::sqlNodeIsValid(column)) {
            return detail::SqlAssignment::invalid(
                detail::sqlNodeDiagnostic(column));
        }
        using Storage = StorageType_t<Value>;
        std::vector<std::shared_ptr<detail::SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<detail::ValueBinder<Storage>>(
            std::forward<Value>(value)));
        return {
            .sql = column.sql() + " = COALESCE(?, " + column.sql() + ")",
            .binders = std::move(binders),
            .diagnostic = {},
        };
    }

    template <typename Column, typename Value>
        requires(detail::HasSqlMethod<Column> && detail::SqlBindable<Value>)
    auto assignGreatest(const Column &column, Value &&value) const
        -> detail::SqlAssignment {
        if (!detail::sqlNodeIsValid(column)) {
            return detail::SqlAssignment::invalid(
                detail::sqlNodeDiagnostic(column));
        }
        using Storage = StorageType_t<Value>;
        std::vector<std::shared_ptr<detail::SqlStatementBinder>> binders;
        binders.push_back(std::make_shared<detail::ValueBinder<Storage>>(
            std::forward<Value>(value)));
        return {
            .sql = column.sql() + " = " +
                   Dialect<BackendTag>::greatest_value(column.sql(), "?"),
            .binders = std::move(binders),
            .diagnostic = {},
        };
    }

    // Remove Builder
    auto remove() { return detail::DeleteBuilder(derived().db(), derived().tableRef()); }

    // =========================================================
    // 2. 查询操作 (Select, Count)
    // =========================================================

    // Select
    template <typename... Us, template <typename U> typename... Ts>
        requires(detail::HasSqlMethod<Ts<Us>> && ...)
    auto select(Ts<Us>... args) const {
        return detail::ProjectedSelectBuilder<Us...>(derived().db(), derived().tableRef(), {args.sql()...},
                                                     &Dialect<BackendTag>::quote_identifier_path,
                                                     detail::collectSqlDiagnostics(args...));
    }

    template <typename... Ts>
        requires(detail::HasSqlMethod<Ts> && ...)
    auto select(Ts... args) const {
        return detail::ProjectedSelectBuilder<>(derived().db(), derived().tableRef(), {args.sql()...},
                                                &Dialect<BackendTag>::quote_identifier_path,
                                                detail::collectSqlDiagnostics(args...));
    }

    auto select(std::string_view columns) const {
        return detail::SelectBuilder(derived().db(), derived().tableRef(), quoteRuntimeColumns(columns),
                                     &Dialect<BackendTag>::quote_identifier_path);
    }

    auto select(std::initializer_list<std::string_view> columns) const {
        return detail::SelectBuilder(derived().db(), derived().tableRef(), quoteRuntimeColumns(columns),
                                     &Dialect<BackendTag>::quote_identifier_path);
    }

    auto select(const std::vector<std::string> &columns) const {
        return detail::SelectBuilder(derived().db(), derived().tableRef(), quoteRuntimeColumns(columns),
                                     &Dialect<BackendTag>::quote_identifier_path);
    }

    auto select() const {
        auto builder =
            detail::ProjectedSelectBuilder<T>(derived().db(), derived().tableRef(),
                                              &Dialect<BackendTag>::quote_identifier_path);
        return builder;
    }

    auto count() const {
        return detail::ProjectedSelectBuilder<int>(derived().db(), derived().tableRef(), {"COUNT(*)"},
                                                   &Dialect<BackendTag>::quote_identifier_path);
    }

    auto count(std::string_view column) const {
        return detail::ProjectedSelectBuilder<int>(derived().db(), derived().tableRef(),
                                                   {"COUNT(" + quoteRuntimeColumn(column) + ")"},
                                                   &Dialect<BackendTag>::quote_identifier_path);
    }

    template <typename U>
    auto count(detail::TypedColumn<U> column) const {
        return detail::ProjectedSelectBuilder<int>(derived().db(), derived().tableRef(),
                                                   {"COUNT(" + column.sql() + ")"},
                                                   &Dialect<BackendTag>::quote_identifier_path,
                                                   detail::collectSqlDiagnostics(column));
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
        T    obj;
        auto target = std::addressof(obj.*memberPtr);
        int  index  = 0;
        int  result = -1;
        NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](auto &field, std::string_view /*name*/, const auto &tags) {
            if (result != -1) {
                return;
            }
            if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                using Field = std::remove_cvref_t<decltype(field)>;
                if constexpr (std::is_same_v<Field, M>) {
                    if (std::addressof(field) == target) {
                        result = index;
                    }
                }
                ++index;
            }
        });
        return result;
    }

    template <typename M>
    auto getColumnName(M T::*memberPtr) const -> IoResult<std::string> {
        auto index = getColumnIndex(memberPtr);
        if (index == -1)
            return Err(std::make_error_code(std::errc::invalid_argument));
        return derived().getColumnNames().at(index);
    }

    template <typename M>
    auto getColumnTag(M T::*memberPtr) const -> IoResult<SqlTags> {
        auto index = getColumnIndex(memberPtr);
        if (index == -1)
            return Err(std::make_error_code(std::errc::invalid_argument));
        return derived().getColumnTags().at(index);
    }

    // =========================================================
    // 4. 列访问 DSL (col, sql)
    // =========================================================

    auto invalidColumnDiagnostic(std::string_view op) const -> std::string {
        return "ORM " + std::string(op) + " member pointer does not map to a reflected column in table '" +
               derived().getTableName() + "'";
    }

    template <typename M>
    auto col(M T::*memberPtr) const {
        auto nameRet = getColumnName(memberPtr);
        if (!nameRet) {
            return detail::TypedColumn<std::decay_t<M>>::invalid(invalidColumnDiagnostic("col"));
        }
        auto name = std::move(nameRet).value();
        return detail::TypedColumn<std::decay_t<M>>(derived().getAlias() + "." +
                                                        Dialect<BackendTag>::quote_identifier(name),
                                                    name);
    }

    template <auto MemberPtr>
        requires std::is_member_object_pointer_v<decltype(MemberPtr)>
    auto col() const {
        using Member = std::remove_cvref_t<decltype(std::declval<T &>().*MemberPtr)>;
        constexpr auto index = detail::reflectedMemberPointerIndex<T, MemberPtr>();
        if constexpr (index < 0) {
            static_assert(index >= 0,
                          "Member pointer does not map to ORM reflection metadata. "
                          "Use col(&T::field) for the runtime-checked path.");
        }
        else {
            constexpr auto names = detail::reflectedFieldNames<T>();
            constexpr auto nameView = names[static_cast<std::size_t>(index)];
            std::string    name(nameView);
            return detail::TypedColumn<std::decay_t<Member>>(derived().getAlias() + "." +
                                                                 Dialect<BackendTag>::quote_identifier(name),
                                                             name);
        }
    }

    template <typename M>
    auto sql(M T::*memberPtr) const {
        auto nameRet = getColumnName(memberPtr);
        if (!nameRet) {
            return detail::TypedColumn<std::decay_t<M>>::invalid(invalidColumnDiagnostic("sql"));
        }
        auto name = std::move(nameRet).value();
        return detail::TypedColumn<std::decay_t<M>>(Dialect<BackendTag>::quote_identifier(name), name);
    }

    template <auto MemberPtr>
        requires std::is_member_object_pointer_v<decltype(MemberPtr)>
    auto sql() const {
        using Member = std::remove_cvref_t<decltype(std::declval<T &>().*MemberPtr)>;
        constexpr auto index = detail::reflectedMemberPointerIndex<T, MemberPtr>();
        if constexpr (detail::reflectedMemberPointerIndex<T, MemberPtr>() < 0) {
            static_assert(detail::reflectedMemberPointerIndex<T, MemberPtr>() >= 0,
                          "Member pointer does not map to ORM reflection metadata. "
                          "Use sql(&T::field) for the runtime-checked path.");
        }
        else {
            std::string    name(detail::reflectedFieldNames<T>()[static_cast<std::size_t>(index)]);
            return detail::TypedColumn<std::decay_t<Member>>(Dialect<BackendTag>::quote_identifier(name), name);
        }
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
        ilias_for_await(auto row, res.rangeResult()) {
            if (!row) {
                ILIAS_ERROR("ilias-sql", "Print failed while loading row: {}", row.error().message());
                break;
            }
            auto obj = std::move(row.value());
            std::vector<std::string> rowStrings;
            NEKO_NAMESPACE::Reflect<T>::forEach(obj, [&](const auto &field, std::string_view /*name*/,
                                                         const auto &tags) {
                if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                    using FieldType = std::decay_t<decltype(field)>;
                    if constexpr (std::is_same_v<FieldType, std::vector<char>> || std::is_same_v<FieldType, SqlBlob>) {
                        rowStrings.push_back("(BLOB " + std::to_string(field.size()) + " bytes)");
                    }
                    else if constexpr (requires(FieldType value) { detail::to_string_view(value); }) {
                        rowStrings.push_back(detail::to_string_view(field));
                    }
                    else {
                        static_assert(std::is_void_v<FieldType>, "Field type is not convertible to string");
                    }
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
