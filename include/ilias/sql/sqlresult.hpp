#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>

#include "ilias/sql/interfaces.hpp"

ILIAS_SQL_NS_BEGIN
template <typename T>
class SqlResult;

template <>
class SqlResult<void> {
public:
    using value_type      = void;
    using reference       = void;
    using const_reference = void;
    using pointer         = void;
    using const_pointer   = void;

public:
    SqlResult() = default;
    SqlResult(std::unique_ptr<IResultSet> imp) : mImp(std::move(imp)) {}
    SqlResult(const SqlResult &) = delete;
    SqlResult(SqlResult &&other) : mImp(std::move(other.mImp)) {}
    SqlResult &operator=(const SqlResult &) = delete;
    SqlResult &operator=(SqlResult &&other) {
        if (this != &other) {
            mImp = std::move(other.mImp);
        }
        return *this;
    }
    ~SqlResult() = default;
    template <typename U>
    auto load(int index, U &value) -> IoResult<void>;
    template <typename U>
    auto load(std::string_view name, U &value) -> IoResult<void>;
    template <typename... Args>
    auto range(Args &...args) -> Generator<IoResult<void>>;
    template <typename U>
        requires(!std::is_class_v<U>) ||
                (std::is_class_v<U> && NEKO_NAMESPACE::detail::has_values_meta<std::decay_t<U>>)
    auto range(U &value) -> Generator<IoResult<void>>;
    auto operator->() -> IResultSet * { return mImp.get(); }
    auto operator->() const -> const IResultSet * { return mImp.get(); }
    auto operator*() -> IResultSet & { return *mImp; }
    auto operator*() const -> const IResultSet & { return *mImp; }
    template <typename T>
    operator SqlResult<T>() {
        return SqlResult<T>(std::move(mImp));
    }

private:
    template <typename U>
    auto unpack(SqlValue &value, U &u) -> IoResult<void>;

protected:
    std::unique_ptr<IResultSet> mImp;
};

template <typename T>
class SqlResult : public SqlResult<void> {
public:
    using value_type      = T;
    using reference       = T &;
    using const_reference = const T &;
    using pointer         = T *;
    using const_pointer   = const T *;

public:
    SqlResult() = default;
    SqlResult(std::unique_ptr<IResultSet> imp) : SqlResult<void>(std::move(imp)) {}
    SqlResult(const SqlResult &) = delete;
    SqlResult(SqlResult &&other) : SqlResult<void>(std::move(other.mImp)) {}
    SqlResult &operator=(const SqlResult &) = delete;
    SqlResult &operator=(SqlResult &&other) {
        if (this != &other) {
            mImp = std::move(other.mImp);
        }
        return *this;
    }
    ~SqlResult() = default;
    template <typename U>
    operator SqlResult<U>() {
        return SqlResult<U>(std::move(mImp));
    }

    using SqlResult<void>::operator->;
    using SqlResult<void>::operator*;
    using SqlResult<void>::load;
    using SqlResult<void>::range;
    auto range() -> Generator<T>;
};

template <typename U>
auto SqlResult<void>::unpack(SqlValue &ret, U &value) -> IoResult<void> {
    switch ((SqlValueType)ret.index()) {
        case SqlValueType::kNull:
            return {};
        case SqlValueType::kChar: {
            if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kChar>::type, U>) {
                value = get<SqlValueType::kChar>(ret);
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kInt: {
            if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kInt>::type, U>) {
                value = get<SqlValueType::kInt>(ret);
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kBigInt: {
            if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kBigInt>::type, U>) {
                value = get<SqlValueType::kBigInt>(ret);
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kFloat: {
            if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kFloat>::type, U>) {
                value = get<SqlValueType::kFloat>(ret);
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kDouble: {
            if constexpr (std::is_convertible_v<SqlValueTraits<SqlValueType::kDouble>::type, U>) {
                value = get<SqlValueType::kDouble>(ret);
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kText: {
            if constexpr (std::is_constructible_v<U, SqlValueTraits<SqlValueType::kText>::type>) {
                value = U(std::move(get<SqlValueType::kText>(ret)));
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kBlob: {
            if constexpr (std::is_constructible_v<U, SqlValueTraits<SqlValueType::kBlob>::type>) {
                value = U(std::move(get<SqlValueType::kBlob>(ret)));
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        case SqlValueType::kDate: {
            if constexpr (std::is_constructible_v<U, SqlValueTraits<SqlValueType::kDate>::type>) {
                value = U(std::move(get<SqlValueType::kDate>(ret)));
                return {};
            }
            else {
                return Unexpected(make_error_code(std::errc::invalid_argument));
            }
        }
        default:
            return Unexpected(make_error_code(std::errc::invalid_argument));
    }
}

template <typename U>
auto SqlResult<void>::load(int index, U &value) -> IoResult<void> {
    auto ret = mImp->getValue(index);
    if (!ret) {
        return Unexpected(ret.error());
    }
    // ILIAS_INFO("ilias-sql", "index: {}, value: {}", index, *ret);
    return unpack(*ret, value);
}

template <typename U>
auto SqlResult<void>::load(std::string_view name, U &value) -> IoResult<void> {
    auto ret = mImp->getValue(name);
    if (!ret) {
        return Unexpected(ret.error());
    }
    // ILIAS_INFO("ilias-sql", "name: {}, value: {}", name, *ret);
    return unpack(*ret, value);
}

template <typename... Args>
auto SqlResult<void>::range(Args &...args) -> Generator<IoResult<void>> {
    while (1) {
        auto rc = co_await mImp->next();
        if (!rc) {
            break;
        }
        if (!*rc) {
            break;
        }
        int            idx = 0;
        IoResult<void> ret = {};
        ((ret = ret ? load(idx++, args) : ret) && ...);
        co_yield ret;
    }
}

template <typename U>
    requires(!std::is_class_v<U>) || (std::is_class_v<U> && NEKO_NAMESPACE::detail::has_values_meta<std::decay_t<U>>)
auto SqlResult<void>::range(U &value) -> Generator<IoResult<void>> {
    while (1) {
        auto rc = co_await mImp->next();
        if (!rc) {
            break;
        }
        if (!*rc) {
            break;
        }
        IoResult<void> ret = {};
        NEKO_NAMESPACE::Reflect<U>::forEach(value, [this, &ret](auto &field, std::string_view name) {
            ret = ret ? load(name, field) : ret;
            // ILIAS_INFO("ilias-sql", "field: {}, value: {}", name, field);
        });
        co_yield ret;
    }
}

template <typename T>
auto SqlResult<T>::range() -> Generator<T> {
    T value;
    if constexpr (NEKO_NAMESPACE::detail::is_std_tuple<T>()) {
        ilias_for_await(auto rc,
                        std::apply([this](auto &...args) { return (SqlResult<void>::range(args...)); }, value)) {
            if (!rc) {
                co_return;
            }
            co_yield value;
        }
    }
    else {
        ilias_for_await(auto rc, SqlResult<void>::range(value)) {
            if (!rc) {
                co_return;
            }
            co_yield value;
        }
    }
}

ILIAS_SQL_NS_END
