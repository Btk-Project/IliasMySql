/**
 * @file sqlresult.hpp
 * @brief Result set wrapper with type-safe access
 *
 * This file provides the SqlResult template class for handling database
 * query results with type-safe access and generator-based iteration.
 */

#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>

#include <optional>

#include "ilias/sql/detail/reflection_metadata.hpp"
#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/detail/coverter.hpp"

ILIAS_SQL_NS_BEGIN

/**
 * @brief Forward declaration of SqlResult template
 */
template <typename T>
class SqlResult;

/**
 * @brief Specialization for void result sets (no type mapping)
 *
 * This specialization is used when no type mapping is required,
 * allowing direct access to the underlying IResultSet.
 */
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
    template <typename U>
    SqlResult(SqlResult<U> &&other) : mImp(std::move(other.mImp)) {}
    SqlResult(const SqlResult &)            = delete;
    SqlResult(SqlResult &&other)            = default;
    SqlResult &operator=(const SqlResult &) = delete;
    SqlResult &operator=(SqlResult &&other) = default;
    template <typename U>
    auto operator=(SqlResult<U> &&other) -> SqlResult & {
        mImp = std::move(other.mImp);
        return *this;
    }
    ~SqlResult() = default;
    template <typename U>
    auto load(int index, U &value) -> IoResult<void>;
    template <typename U>
    auto load(std::string_view name, U &value) -> IoResult<void>;
    template <typename... Args>
        requires(!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
    auto range(Args &...args) -> Generator<IoResult<void>>;
    template <typename... Args>
        requires(sizeof...(Args) > 0 && (NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...))
    auto range(Args &...value) -> Generator<IoResult<void>>;
    auto operator->() -> IResultSet * { return mImp.get(); }
    auto operator->() const -> const IResultSet * { return mImp.get(); }
    auto operator*() -> IResultSet & { return *mImp; }
    auto operator*() const -> const IResultSet & { return *mImp; }
    auto capabilities() const -> ResultCapabilities {
        if (!mImp) {
            return {};
        }
        return mImp->capabilities();
    }
    auto rowsFetched() const -> size_t {
        if (!mImp) {
            return 0;
        }
        return mImp->rowsFetched();
    }
    auto exactRowCount() const -> std::optional<size_t> {
        if (!mImp) {
            return std::nullopt;
        }
        return mImp->exactRowCount();
    }
    auto rowsAffected() const -> std::optional<size_t> {
        if (!mImp) {
            return std::nullopt;
        }
        return mImp->rowsAffected();
    }
    auto lastNativeError() const -> std::optional<NativeSqlError> {
        if (!mImp) {
            return std::nullopt;
        }
        return mImp->lastNativeError();
    }

    void storage(std::shared_ptr<void> ptr) { mStorage.push_back(std::move(ptr)); }
    void storage(std::initializer_list<std::shared_ptr<void>> ptrs) {
        mStorage.insert(mStorage.end(), ptrs.begin(), ptrs.end());
    }
    void cleanStorage() { mStorage.clear(); }

private:
    template <typename U>
    friend class SqlResult;

protected:
    // sqlite need binders lifetime after result
    std::vector<std::shared_ptr<void>> mStorage;
    std::unique_ptr<IResultSet>        mImp;
};

/**
 * @brief Typed result set wrapper with automatic type mapping
 *
 * This specialization provides type-safe access to query results,
 * automatically mapping database rows to C++ types using reflection.
 *
 * @tparam T The type to map each row to
 */
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
    template <typename U>
    SqlResult(SqlResult<U> &&other) : SqlResult<void>(std::move(other.mImp)) {}
    SqlResult(const SqlResult &)            = delete;
    SqlResult(SqlResult &&)                 = default;
    SqlResult &operator=(const SqlResult &) = delete;
    SqlResult &operator=(SqlResult &&other) = default;
    template <typename U>
    auto operator=(SqlResult<U> &&other) -> SqlResult<T> & {
        mImp = std::move(other.mImp);
        return *this;
    }
    ~SqlResult() = default;

    using SqlResult<void>::operator->;
    using SqlResult<void>::operator*;
    using SqlResult<void>::load;
    using SqlResult<void>::range;
    auto rangeResult() -> Generator<IoResult<T>>;
    auto range() -> Generator<T>;

    template <typename U>
    friend class SqlResult;
};

template <typename U>
auto SqlResult<void>::load(int index, U &arg) -> IoResult<void> {
    ILIAS_TRY(auto ret, mImp->getValue(index));
    ILIAS_TRY(arg, ret.as<U>());
    return {};
}

template <typename U>
auto SqlResult<void>::load(std::string_view name, U &arg) -> IoResult<void> {
    ILIAS_TRY(auto ret, mImp->getValue(name));
    ILIAS_TRY(arg, ret.as<U>());
    return {};
}

template <typename... Args>
    requires(!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlResult<void>::range(Args &...args) -> Generator<IoResult<void>> {
    while (1) {
        auto rc = co_await mImp->next();
        if (!rc) {
            co_yield Err(rc.error());
            break;
        }
        if (!*rc) {
            break;
        }
        IoResult<void> ret = {};
        if constexpr (sizeof...(Args) > 0) {
            int idx = 0;
            ((ret = ret ? load(idx++, args) : ret) && ...);
        }
        co_yield ret;
    }
}

template <typename... Args>
    requires(sizeof...(Args) > 0 && (NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...))
auto SqlResult<void>::range(Args &...value) -> Generator<IoResult<void>> {
    while (1) {
        auto rc = co_await mImp->next();
        if (!rc) {
            co_yield Err(rc.error());
            break;
        }
        if (!*rc) {
            break;
        }
        IoResult<void> ret = {};
        if constexpr (sizeof...(Args) == 1) {
            auto handler = [this, &ret](auto &value) {
                using ObjT = std::decay_t<decltype(value)>;
                NEKO_NAMESPACE::Reflect<ObjT>::forEach(
                    value, [this, &ret](auto &field, std::string_view name, const auto &tags) {
                        if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                            if (ret) {
                                ret = load(detail::reflectedFieldName(name, tags), field);
                            }
                        }
                    });
            };
            [&handler]<std::size_t... I>(std::index_sequence<I...>, auto tuple) {
                (handler(std::get<I>(tuple)), ...);
            }(std::make_index_sequence<sizeof...(Args)>(), std::tie(value...));
        }
        else {
            int  idx     = 0;
            auto handler = [this, &ret, &idx](auto &value) {
                using ObjT = std::decay_t<decltype(value)>;
                NEKO_NAMESPACE::Reflect<ObjT>::forEach(value, [this, &ret, &idx](auto &field, std::string_view /*name*/,
                                                                                 const auto &tags) {
                    if constexpr (!detail::reflectedFieldTypeIgnored<decltype(tags)>()) {
                        if (ret) {
                            ret = load(idx++, field);
                            if (!ret) {
                                ILIAS_TRACE("ilias-sql", "Failed to load field '{}': {}", idx, ret.error().message());
                            }
                        }
                    }
                });
            };
            [&handler]<std::size_t... I>(std::index_sequence<I...>, auto tuple) {
                (handler(std::get<I>(tuple)), ...);
            }(std::make_index_sequence<sizeof...(Args)>(), std::tie(value...));
        }
        co_yield ret;
    }
}

template <typename T>
auto SqlResult<T>::rangeResult() -> Generator<IoResult<T>> {
    T value;
    if constexpr (NEKO_NAMESPACE::detail::is_std_tuple<T>()) {
        ilias_for_await(auto rc,
                        std::apply([this](auto &...args) { return (SqlResult<void>::range(args...)); }, value)) {
            if (rc) {
                co_yield std::move(value);
                value = T {};
            }
            else {
                co_yield Err(rc.error());
                value = T {};
            }
        }
    }
    else {
        ilias_for_await(auto rc, SqlResult<void>::range(value)) {
            if (rc) {
                co_yield std::move(value);
                value = T {};
            }
            else {
                co_yield Err(rc.error());
                value = T {};
            }
        }
    }
}

template <typename T>
auto SqlResult<T>::range() -> Generator<T> {
    ilias_for_await(auto rc, rangeResult()) {
        if (rc) {
            co_yield std::move(rc.value());
        }
        else {
            ILIAS_WARN("ilias-sql", "range failed {}", rc.error().message());
        }
    }
}

ILIAS_SQL_NS_END
