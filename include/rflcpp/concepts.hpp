// rflcpp/concepts.hpp - Concepts used throughout the public API.
// SPDX-License-Identifier: MIT
#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace rflcpp {

template <class T>
concept boolean_like = std::same_as<std::remove_cvref_t<T>, bool>;

template <class T>
concept numeric_like =
    std::is_arithmetic_v<std::remove_cvref_t<T>> && !boolean_like<T>;

template <class T>
concept string_like = std::convertible_to<const T&, std::string_view>;

namespace detail {
template <class T>
concept has_begin_end = requires(T& t) {
    { std::begin(t) };
    { std::end(t) };
};

template <class T>
concept has_mapped_type = requires { typename std::remove_cvref_t<T>::mapped_type; };
} // namespace detail

template <class T>
concept sequence_like =
    detail::has_begin_end<T> && !detail::has_mapped_type<T> && !string_like<T>;

template <class T>
concept map_like =
    detail::has_begin_end<T> && detail::has_mapped_type<T> &&
    string_like<typename std::remove_cvref_t<T>::key_type>;

namespace detail {
template <class>           struct is_optional               : std::false_type {};
template <class T>         struct is_optional<std::optional<T>> : std::true_type {};

template <class>           struct is_variant                : std::false_type {};
template <class... Ts>     struct is_variant<std::variant<Ts...>> : std::true_type {};
} // namespace detail

template <class T>
concept optional_like = detail::is_optional<std::remove_cvref_t<T>>::value;

template <class T>
concept variant_like = detail::is_variant<std::remove_cvref_t<T>>::value;

/// Class types that the reflection engine can usefully inspect: anything
/// that isn't a primitive, string, container, optional, or variant.
template <class T>
concept reflectable_class =
    std::is_class_v<std::remove_cvref_t<T>> &&
    !string_like<T> && !sequence_like<T> && !map_like<T> &&
    !optional_like<T> && !variant_like<T>;

} // namespace rflcpp
