#pragma once

#include "ilias/sql/global/global.hpp"

#include <nekoproto/serialization/reflection.hpp>

#include <array>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

ILIAS_SQL_NS_BEGIN

namespace detail {

template <typename Tags>
constexpr bool reflectedFieldIgnored(const Tags &tags) {
    return nekoproto::tag_query::get<nekoproto::tag_property::Ignore>(tags);
}

template <typename Tags>
consteval bool reflectedFieldTypeIgnored() {
    using RawTags = std::remove_cvref_t<Tags>;
    return reflectedFieldIgnored(RawTags {});
}

template <typename Tags>
constexpr auto reflectedFieldName(std::string_view reflectedName, const Tags &tags) -> std::string_view {
    const auto renamed = nekoproto::tag_query::get<nekoproto::tag_property::Name>(tags);
    return renamed.empty() ? reflectedName : renamed;
}

template <typename T, std::size_t... Is>
constexpr auto reflectedFieldNames(std::index_sequence<Is...>) {
    constexpr auto names = nekoproto::Reflect<T>::names();
    constexpr auto tags  = nekoproto::Reflect<T>::field_tags;
    return std::array<std::string_view, sizeof...(Is)> {reflectedFieldName(names[Is], std::get<Is>(tags))...};
}

template <typename T>
constexpr auto reflectedFieldNames() {
    return reflectedFieldNames<T>(std::make_index_sequence<nekoproto::Reflect<T>::value_count> {});
}

template <typename T, std::size_t... Is>
consteval auto reflectedSqlFieldCount(std::index_sequence<Is...>) -> std::size_t {
    constexpr auto tags = nekoproto::Reflect<T>::field_tags;
    return (std::size_t {!reflectedFieldIgnored(std::get<Is>(tags))} + ... + 0);
}

template <typename T>
consteval auto reflectedSqlFieldCount() -> std::size_t {
    return reflectedSqlFieldCount<T>(std::make_index_sequence<nekoproto::Reflect<T>::value_count> {});
}

template <typename T, std::size_t... Is>
consteval auto reflectedSqlFieldNames(std::index_sequence<Is...>) {
    constexpr auto                                            names = nekoproto::Reflect<T>::names();
    constexpr auto                                            tags  = nekoproto::Reflect<T>::field_tags;
    std::array<std::string_view, reflectedSqlFieldCount<T>()> result {};
    std::size_t                                               resultIndex = 0;
    (
        [&] {
            if constexpr (!reflectedFieldIgnored(std::get<Is>(tags))) {
                result[resultIndex++] = reflectedFieldName(names[Is], std::get<Is>(tags));
            }
        }(),
        ...);
    return result;
}

template <typename T>
consteval auto reflectedSqlFieldNames() {
    return reflectedSqlFieldNames<T>(std::make_index_sequence<nekoproto::Reflect<T>::value_count> {});
}

template <auto MemberPtr>
consteval auto memberPointerFieldName() -> std::string_view {
#if defined(_MSC_VER)
    constexpr std::string_view signature = __FUNCSIG__;
    constexpr std::string_view prefix    = "memberPointerFieldName<";
    auto                       start     = signature.find(prefix);
    if (start == std::string_view::npos) {
        return {};
    }
    start += prefix.size();
    auto end = signature.find(">(void)", start);
    if (end == std::string_view::npos) {
        end = signature.find('>', start);
    }
#else
    constexpr std::string_view signature = __PRETTY_FUNCTION__;
    constexpr std::string_view prefix    = "MemberPtr = ";
    auto                       start     = signature.find(prefix);
    if (start == std::string_view::npos) {
        return {};
    }
    start += prefix.size();
    auto end = signature.find(';', start);
    if (end == std::string_view::npos) {
        end = signature.find(']', start);
    }
#endif
    if (end == std::string_view::npos || end <= start) {
        return {};
    }
    auto value = signature.substr(start, end - start);
    auto scope = value.rfind("::");
    return scope == std::string_view::npos ? std::string_view {} : value.substr(scope + 2);
}

template <typename T, auto MemberPtr, std::size_t I, typename Accessor>
consteval auto reflectedMemberPointerMatchesAccessor(const Accessor &accessor) -> bool {

    using AccessorT = std::remove_cvref_t<Accessor>;

    // 普通成员对象指针：直接比较。
    if constexpr (std::is_member_object_pointer_v<AccessorT> && std::is_same_v<AccessorT, decltype(MemberPtr)>) {
        return accessor == MemberPtr;
    }
    // Neko serializer accessor：通过字段名称匹配。
    else if constexpr (requires { typename T::Neko::_neko_serializer_args_tuple; }) {
        // 必须分层判断，不能直接在同一个 && 表达式中使用
        // std::invoke_result_t，因为 AccessorT 可能不可调用。
        if constexpr (std::is_invocable_v<AccessorT, T &>) {
            using Result     = std::invoke_result_t<AccessorT, T &>;
            using MemberType = std::remove_pointer_t<decltype(MemberPtr)>;
            if constexpr (std::is_lvalue_reference_v<Result> &&
                          std::is_same_v<std::remove_cvref_t<Result>, MemberType>) {
                constexpr auto rawNames   = nekoproto::Reflect<T>::names();
                constexpr auto memberName = memberPointerFieldName<MemberPtr>();
                return memberName == rawNames[I];
            }
        }
    }

    return false;
}

template <typename T, auto MemberPtr, std::size_t I>
consteval auto reflectedMemberPointerMatchesAt() -> bool {
    constexpr auto tags = nekoproto::Reflect<T>::field_tags;

    if constexpr (reflectedFieldIgnored(std::get<I>(tags))) {
        return false;
    }
    else {
        constexpr auto accessors = nekoproto::detail::ReflectProvider<T>::accessors();
        using Accessors          = decltype(accessors);

        if constexpr (requires { std::tuple_size<std::remove_cvref_t<Accessors>>::value; }) {
            return reflectedMemberPointerMatchesAccessor<T, MemberPtr, I>(std::get<I>(accessors));
        }
        else if constexpr (I == 0) {
            return reflectedMemberPointerMatchesAccessor<T, MemberPtr, I>(accessors);
        }
        else {
            return false;
        }
    }
}

template <typename T, auto MemberPtr, std::size_t... Is>
consteval auto reflectedMemberPointerIndex(std::index_sequence<Is...>) -> int {

    static_assert(std::is_member_object_pointer_v<decltype(MemberPtr)>, "ORM column selector must be a member pointer");

    static_assert(
        requires(T &obj) { obj.*MemberPtr; }, "ORM column member pointer must belong to the mapped entity type");

    constexpr std::array<bool, sizeof...(Is)> matches {reflectedMemberPointerMatchesAt<T, MemberPtr, Is>()...};

    constexpr std::array<int, sizeof...(Is)> indices {static_cast<int>(Is)...};

    for (std::size_t i = 0; i < matches.size(); ++i) {
        if (matches[i]) {
            return indices[i];
        }
    }

    return -1;
}

template <typename T, auto MemberPtr>
consteval auto reflectedMemberPointerIndex() -> int {
    return reflectedMemberPointerIndex<T, MemberPtr>(
        std::make_index_sequence<nekoproto::Reflect<T>::value_count> {});
}

} // namespace detail

ILIAS_SQL_NS_END
