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

template <typename T, auto MemberPtr>
consteval auto reflectedMemberPointerIndex() -> int {
    if constexpr (nekoproto::Reflect<T>::template hasMember<MemberPtr>()) {
        if constexpr (reflectedFieldIgnored(nekoproto::Reflect<T>::template tagOf<MemberPtr>())) {
            return -1;
        }
        return nekoproto::Reflect<T>::template indexOf<MemberPtr>();
    }
    return -1;
}

} // namespace detail

ILIAS_SQL_NS_END
