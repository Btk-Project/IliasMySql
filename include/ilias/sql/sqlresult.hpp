#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>

#include "ilias/sql/interfaces.hpp"
#include "ilias/sql/detail/coverter.hpp"

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

    void storage(std::shared_ptr<void> ptr) { mStorage.push_back(std::move(ptr)); }
    void storage(std::initializer_list<std::shared_ptr<void>> ptrs) {
        mStorage.insert(mStorage.end(), ptrs.begin(), ptrs.end());
    }
    void cleanStorage() { mStorage.clear(); }

private:
    template <typename U>
    auto unpack(SqlValueView &value, U &u) -> IoResult<void>;

    template <typename U>
    friend class SqlResult;

protected:
    // sqlite need binders lifetime after result
    std::vector<std::shared_ptr<void>> mStorage;
    std::unique_ptr<IResultSet>        mImp;
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
    auto range() -> Generator<T>;

    template <typename U>
    friend class SqlResult;
};

template <typename U>
auto SqlResult<void>::unpack(SqlValueView &ret, U &value) -> IoResult<void> {
    return SqlValueConverter<std::decay_t<U>>::convert(ret, value);
}

template <typename U>
auto SqlResult<void>::load(int index, U &value) -> IoResult<void> {
    auto ret = mImp->getValue(index);
    if (!ret) {
        ILIAS_TRACE("ilias-sql", "Failed to load column '{}': {}", index, ret.error().message());
        return Unexpected(ret.error());
    }
    // ILIAS_TRACE("ilias-sql", "load {} : {}", index, ret.value());
    return unpack(*ret, value);
}

template <typename U>
auto SqlResult<void>::load(std::string_view name, U &value) -> IoResult<void> {
    auto ret = mImp->getValue(name);
    if (!ret) {
        ILIAS_TRACE("ilias-sql", "Failed to load column '{}': {}", name, ret.error().message());
        return Unexpected(ret.error());
    }
    // ILIAS_TRACE("ilias-sql", "load {} : {}", name, ret.value());
    return unpack(*ret, value);
}

template <typename... Args>
    requires(!NEKO_NAMESPACE::detail::has_names_meta<std::decay_t<Args>> && ...)
auto SqlResult<void>::range(Args &...args) -> Generator<IoResult<void>> {
    while (1) {
        auto rc = co_await mImp->next();
        if (!rc) {
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
                    value, [this, &ret](auto &field, std::string_view name) { ret = ret ? load(name, field) : ret; });
            };
            [&handler]<std::size_t... I>(std::index_sequence<I...>, auto tuple) {
                (handler(std::get<I>(tuple)), ...);
            }(std::make_index_sequence<sizeof...(Args)>(), std::tie(value...));
        }
        else {
            int  idx     = 0;
            auto handler = [this, &ret, &idx](auto &value) {
                using ObjT = std::decay_t<decltype(value)>;
                NEKO_NAMESPACE::Reflect<ObjT>::forEach(value, [this, &ret, &idx](auto &field) {
                    ret = ret ? load(idx++, field) : ret;
                    if (!ret) {
                        ILIAS_TRACE("ilias-sql", "Failed to load field '{}': {}", idx, ret.error().message());
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
auto SqlResult<T>::range() -> Generator<T> {
    T value;
    if constexpr (NEKO_NAMESPACE::detail::is_std_tuple<T>()) {
        ilias_for_await(auto rc,
                        std::apply([this](auto &...args) { return (SqlResult<void>::range(args...)); }, value)) {
            if (rc) {
                co_yield value;
                value = T {};
            }
            else {
                ILIAS_WARN("ilias-sql", "range faild {}", rc.error().message());
            }
        }
    }
    else {
        ilias_for_await(auto rc, SqlResult<void>::range(value)) {
            if (rc) {
                co_yield value;
                value = T {};
            }
            else {
                ILIAS_WARN("ilias-sql", "range faild {}", rc.error().message());
            }
        }
    }
}

ILIAS_SQL_NS_END
