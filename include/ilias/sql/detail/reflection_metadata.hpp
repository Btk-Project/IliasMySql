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
constexpr auto reflectedFieldName(std::string_view reflectedName, const Tags &tags) -> std::string_view {
    const auto renamed = NEKO_NAMESPACE::tag_access::recursive_name(tags);
    return renamed.empty() ? reflectedName : renamed;
}

template <typename T, std::size_t... Is>
constexpr auto reflectedFieldNames(std::index_sequence<Is...>) {
    constexpr auto names = NEKO_NAMESPACE::Reflect<T>::names();
    constexpr auto tags  = NEKO_NAMESPACE::Reflect<T>::field_tags;
    return std::array<std::string_view, sizeof...(Is)> {reflectedFieldName(names[Is], std::get<Is>(tags))...};
}

template <typename T>
constexpr auto reflectedFieldNames() {
    return reflectedFieldNames<T>(std::make_index_sequence<NEKO_NAMESPACE::Reflect<T>::value_count> {});
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

template <typename T, auto MemberPtr, std::size_t... Is>
consteval auto reflectedMemberPointerIndex(std::index_sequence<Is...>) -> int {
    static_assert(std::is_member_object_pointer_v<decltype(MemberPtr)>, "ORM column selector must be a member pointer");
    static_assert(requires(T &obj) { obj.*MemberPtr; },
                  "ORM column member pointer must belong to the mapped entity type");

    constexpr auto accessors = NEKO_NAMESPACE::detail::ReflectHelper<T>::getValues();
    constexpr auto rawNames  = NEKO_NAMESPACE::Reflect<T>::names();
    constexpr auto memberName = memberPointerFieldName<MemberPtr>();
    auto matches = [&]<std::size_t I, typename Accessor>(const Accessor &accessor) constexpr -> bool {
        using AccessorT = std::remove_cvref_t<Accessor>;
        if constexpr (std::is_member_object_pointer_v<AccessorT> && std::is_same_v<AccessorT, decltype(MemberPtr)>) {
            return accessor == MemberPtr;
        }
        else if constexpr (requires { typename T::Neko::_neko_serializer_args_tuple; } &&
                           std::is_invocable_v<AccessorT, T &> &&
                           std::is_lvalue_reference_v<std::invoke_result_t<AccessorT, T &>>) {
            return memberName == rawNames[I];
        }
        return false;
    };
    auto matchesAt = [&]<std::size_t I>() constexpr -> bool {
        using Accessors = decltype(accessors);
        if constexpr (requires { std::tuple_size<std::remove_cvref_t<Accessors>>::value; }) {
            return matches.template operator()<I>(std::get<I>(accessors));
        }
        else if constexpr (I == 0) {
            return matches.template operator()<I>(accessors);
        }
        return false;
    };

    int result = -1;
    ((matchesAt.template operator()<Is>() ? (result = static_cast<int>(Is), true) : false) || ...);
    return result;
}

template <typename T, auto MemberPtr>
consteval auto reflectedMemberPointerIndex() -> int {
    return reflectedMemberPointerIndex<T, MemberPtr>(
        std::make_index_sequence<NEKO_NAMESPACE::Reflect<T>::value_count> {});
}

} // namespace detail

ILIAS_SQL_NS_END
