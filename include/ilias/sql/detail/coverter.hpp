#pragma once

#include "../global/global.hpp"

#include <optional>
#include <type_traits>
#include <ilias/io/error.hpp>

#include "../types.hpp"
#include "../sqlerror.hpp"
#include "../interfaces.hpp"

ILIAS_SQL_NS_BEGIN

namespace detail {
template <typename T>
struct is_span : std::false_type {};
template <typename T, std::size_t Extent>
struct is_span<std::span<T, Extent>> : std::true_type {};
template <typename T>
inline constexpr bool is_span_v = is_span<T>::value;

// 2. 检查是否为 std::array
template <typename T>
struct is_std_array : std::false_type {};
template <typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};
template <typename T>
inline constexpr bool is_std_array_v = is_std_array<T>::value;

// 3. 检查是否为“视图”类型
template <typename T>
inline constexpr bool is_view_v = is_span_v<T> || std::is_same_v<T, std::string_view>;

// 4. 检查是否为我们支持的“动态容器”
template <typename T>
struct is_resizable_container : std::false_type {};
template <typename T, typename Alloc>
struct is_resizable_container<std::vector<T, Alloc>> : std::true_type {};
template <typename CharT, typename Traits, typename Alloc>
struct is_resizable_container<std::basic_string<CharT, Traits, Alloc>> : std::true_type {};
template <typename T>
inline constexpr bool is_resizable_container_v = is_resizable_container<T>::value;

// 5. 检查是否为“固定大小缓冲区”
template <typename T>
inline constexpr bool is_fixed_size_buffer_v =
    std::is_array_v<std::remove_reference_t<T>> || is_std_array_v<std::remove_reference_t<T>>;

template <typename SourceType, typename TargetType>
static auto convert_binary_data(std::span<SourceType> source_data, TargetType &value) -> IoResult<void> {
    using CleanTargetType = std::remove_cvref_t<TargetType>;
    // --- 分支1: 目标是视图类型 (零拷贝) ---
    if constexpr (is_view_v<CleanTargetType>) {
        using ViewElementType      = typename CleanTargetType::value_type;
        const size_t element_count = source_data.size_bytes() / sizeof(ViewElementType);

        // 确保不会因为类型大小不匹配而创建出错误的视图
        if (source_data.size_bytes() % sizeof(ViewElementType) != 0) {
            ILIAS_WARN("ilias-sql", "Type not matched: {} -> {}", source_data, std::type_index(typeid(TargetType)));
            return Unexpected(SqlError::TypeNotMatched); // Or a more specific error
        }

        value = CleanTargetType(reinterpret_cast<const ViewElementType *>(source_data.data()), element_count);
        return {};
    }
    // --- 分支2: 目标是动态容器 (拷贝) ---
    else if constexpr (is_resizable_container_v<CleanTargetType>) {
        using ContainerElementType = typename CleanTargetType::value_type;
        const size_t element_count = source_data.size_bytes() / sizeof(ContainerElementType);

        if (source_data.size_bytes() % sizeof(ContainerElementType) != 0) {
            ILIAS_WARN("ilias-sql", "Type not matched: {} -> {}", source_data, std::type_index(typeid(TargetType)));
            return Unexpected(SqlError::TypeNotMatched);
        }

        auto begin_ptr = reinterpret_cast<const ContainerElementType *>(source_data.data());
        auto end_ptr   = begin_ptr + element_count;

        // 使用 assign 更通用，因为它适用于已存在的容器
        value.assign(begin_ptr, end_ptr);
        return {};
    }
    // --- 分支3: 目标是固定大小缓冲区 (C-array, std::array) (拷贝) ---
    else if constexpr (is_fixed_size_buffer_v<TargetType>) { // 注意这里用 TargetType 而非 CleanTargetType
        // std::span 可以完美地包装 C-array 和 std::array
        std::span destination_span(value);

        // 安全检查：防止缓冲区溢出
        const size_t bytes_to_copy = std::min(source_data.size_bytes(), destination_span.size_bytes());

        std::memcpy(destination_span.data(), source_data.data(), bytes_to_copy);

        // 如果源数据被截断，可以返回一个警告或特定错误
        if (bytes_to_copy < source_data.size_bytes()) {
            ILIAS_WARN("ilias-sql", "Data truncated: {} -> {}", source_data, std::type_index(typeid(TargetType)));
            return Unexpected(SqlError::DataTruncated);
        }
        // 如果需要，可以将缓冲区的剩余部分清零
        if (bytes_to_copy < destination_span.size_bytes()) {
            std::memset(static_cast<std::byte *>(destination_span.data()) + bytes_to_copy, 0,
                        destination_span.size_bytes() - bytes_to_copy);
        }
        return {};
    }
    else if constexpr (std::is_same_v<std::decay_t<SourceType>, char> && std::is_same_v<CleanTargetType, SqlDate>) {
        value = SqlDate(std::string_view {source_data.data(), source_data.size_bytes()});
        return {};
    }
    else {
        // 所有支持的二进制/字符串转换都已处理，如果走到这里，说明类型不匹配
        ILIAS_WARN("ilias-sql", "Type not matched: {} -> {}", source_data, std::type_index(typeid(TargetType)));
        return Unexpected(SqlError::TypeNotMatched);
    }
}
} // namespace detail

// 前置声明
template <typename T, typename Enable = void>
struct SqlBinder;

// ==========================================
// 1. 默认绑定器 (处理基础类型)
// ==========================================
template <typename T>
    requires(!DereferenceableAndNullable<T> && !std::is_enum_v<T>)
struct SqlBinder<T, void> {
    // 按索引绑定
    template <typename U>
    static auto bind(IStatement &stmt, int index, U &&value) -> IoResult<void> {
        return stmt.bind(index, std::forward<U>(value));
    }
    template <typename U>
    static auto bind(IStatement &stmt, std::string_view name, U &&value) -> IoResult<void> {
        return stmt.bind(name, std::forward<U>(value));
    }
};

// ==========================================
// 2. std::optional 特化支持
// ==========================================
template <typename T>
    requires DereferenceableAndNullable<T>
struct SqlBinder<T, void> {
    using InnerType = std::decay_t<decltype(*std::declval<T>())>;
    static auto bind(IStatement &stmt, int index, const T &value) -> IoResult<void> {
        if (value.has_value()) {
            return SqlBinder<InnerType>::bind(stmt, index, *value);
        }
        else {
            return stmt.bind(index, g_sql_null);
        }
    }

    static auto bind(IStatement &stmt, std::string_view name, const T &value) -> IoResult<void> {
        if (value.has_value()) {
            return SqlBinder<InnerType>::bind(stmt, name, *value);
        }
        else {
            return stmt.bind(name, g_sql_null);
        }
    }
};

// ==========================================
// 3. (可选) Enum 枚举支持
// ==========================================
template <typename T>
struct SqlBinder<T, std::enable_if_t<std::is_enum_v<T>>> {
    static auto bind(IStatement &stmt, int index, const T &value) -> IoResult<void> {
        using UnderlyingType = std::underlying_type_t<T>;
        return SqlBinder<UnderlyingType>::bind(stmt, index, static_cast<UnderlyingType>(value));
    }

    static auto bind(IStatement &stmt, std::string_view name, const T &value) -> IoResult<void> {
        using UnderlyingType = std::underlying_type_t<T>;
        return SqlBinder<UnderlyingType>::bind(stmt, name, static_cast<UnderlyingType>(value));
    }
};

ILIAS_SQL_NS_END