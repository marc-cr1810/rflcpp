// rflcpp/derive.hpp - Reflection-derived equality, ordering, hash, and format.
// SPDX-License-Identifier: MIT
//
// Free helpers that walk a type's members at compile time:
//
//   rflcpp::derive::equal(a, b)    -> bool
//   rflcpp::derive::compare(a, b)  -> std::*_ordering
//   rflcpp::derive::hash(v)        -> size_t
//   rflcpp::derive::format(v)      -> string
//
// `derive::all<T>` (CRTP) wires these up as free operators for `T`.
// `derive::hash_for<T>` is a `std::hash<T>` specialization helper.

#pragma once

#include <compare>
#include <cstddef>
#include <format>
#include <functional>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>

#include <rflcpp/attributes.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/enum_meta.hpp>
#include <rflcpp/reflect.hpp>

namespace rflcpp::derive {

template <class T>
constexpr bool equal(const T& a, const T& b) {
    if constexpr (std::equality_comparable<T>) {
        return a == b;
    } else if constexpr (reflectable_class<T>) {
        return to_tuple(a) == to_tuple(b);
    } else {
        static_assert(sizeof(T) == 0,
            "rflcpp::derive::equal: type is neither equality-comparable "
            "nor a reflectable aggregate.");
    }
}

template <class T>
constexpr auto compare(const T& a, const T& b)
    -> decltype(std::declval<T>() <=> std::declval<T>())
    requires std::three_way_comparable<T>
{
    return a <=> b;
}

template <class T>
constexpr std::partial_ordering compare(const T& a, const T& b)
    requires (!std::three_way_comparable<T> && reflectable_class<T>)
{
    return to_tuple(a) <=> to_tuple(b);
}

namespace detail {

constexpr std::size_t hash_combine(std::size_t seed, std::size_t v) noexcept {
    return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

template <class T> constexpr std::size_t hash_one(const T& v);

} // namespace detail

template <class T>
constexpr std::size_t hash(const T& v) {
    if constexpr (requires { std::hash<T>{}(v); }) {
        return std::hash<T>{}(v);
    } else if constexpr (enum_like<T>) {
        using U = std::underlying_type_t<T>;
        return std::hash<U>{}(static_cast<U>(v));
    } else if constexpr (reflectable_class<T>) {
        std::size_t h = 0;
        for_each_field(v, [&](std::string_view, auto const& fv) {
            h = detail::hash_combine(h, detail::hash_one(fv));
        });
        return h;
    } else {
        static_assert(sizeof(T) == 0,
            "rflcpp::derive::hash: type is neither std::hash-able nor a "
            "reflectable aggregate.");
    }
}

namespace detail {
template <class T> constexpr std::size_t hash_one(const T& v) {
    return rflcpp::derive::hash(v);
}
} // namespace detail

template <class T> std::string format(const T& v);

namespace detail {

template <class T>
void format_one(std::ostream& os, const T& v) {
    if constexpr (std::is_same_v<std::remove_cvref_t<T>, bool>) {
        os << (v ? "true" : "false");
    } else if constexpr (enum_like<T>) {
        os << enum_name(v);
    } else if constexpr (std::is_arithmetic_v<T>) {
        os << v;
    } else if constexpr (string_like<T>) {
        os << '"' << std::string_view{v} << '"';
    } else if constexpr (requires { os << v; }) {
        os << v;
    } else if constexpr (reflectable_class<T>) {
        os << rflcpp::derive::format(v);
    } else {
        os << "<unprintable>";
    }
}

} // namespace detail

template <class T>
std::string format(const T& v) {
    std::ostringstream oss;
    if constexpr (reflectable_class<T>) {
        oss << type_name_of<T>() << '{';
        bool first = true;
        for_each_field(v, [&](std::string_view name, auto const& fv) {
            if (!first) oss << ", ";
            first = false;
            oss << name << '=';
            detail::format_one(oss, fv);
        });
        oss << '}';
    } else {
        detail::format_one(oss, v);
    }
    return oss.str();
}

/// CRTP base that wires up `==`, `<=>`, and `operator<<` for `T`.
template <class T>
struct all {
    friend bool operator==(const T& a, const T& b) {
        return rflcpp::derive::equal(a, b);
    }
    friend auto operator<=>(const T& a, const T& b) {
        return rflcpp::derive::compare(a, b);
    }
    friend std::ostream& operator<<(std::ostream& os, const T& v) {
        return os << rflcpp::derive::format(v);
    }
};

/// `std::hash<T>` opt-in helper:
///   template <> struct std::hash<my_type> : rflcpp::derive::hash_for<my_type> {};
template <class T>
struct hash_for {
    std::size_t operator()(const T& v) const noexcept {
        return rflcpp::derive::hash(v);
    }
};

} // namespace rflcpp::derive
