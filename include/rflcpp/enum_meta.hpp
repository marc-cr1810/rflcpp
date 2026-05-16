// rflcpp/enum_meta.hpp - Compile-time enum reflection toolkit.
// SPDX-License-Identifier: MIT
//
// A magic_enum-flavored API powered by P2996 reflection. See docs for the
// full list of helpers; the highlights are `enum_name`, `enum_value`,
// `enum_entries`, `enum_cast`, `for_each_enum`, and the bit-flag helpers
// gated by `enum_flags_policy<E>::is_flags`.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if __has_include(<meta>)
#  include <meta>
#elif __has_include(<experimental/meta>)
#  include <experimental/meta>
#endif

#include <rflcpp/concepts.hpp>

namespace rflcpp {

template <class E>
concept enum_like = std::is_enum_v<std::remove_cvref_t<E>>;

namespace detail {

template <enum_like E>
consteval auto enumerator_infos() {
    constexpr auto N = std::meta::enumerators_of(^^E).size();
    auto v = std::meta::enumerators_of(^^E);
    std::array<std::meta::info, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = v[i];
    return out;
}

} // namespace detail

template <enum_like E>
consteval std::size_t enum_count() {
    return std::meta::enumerators_of(^^E).size();
}

template <enum_like E>
consteval auto enum_entries() {
    static constexpr auto infos = detail::enumerator_infos<E>();
    std::array<std::pair<E, std::string_view>, infos.size()> out{};
    std::size_t i = 0;
    template for (constexpr auto en : infos) {
        out[i++] = { [: en :], std::meta::identifier_of(en) };
    }
    return out;
}

template <enum_like E>
consteval auto enum_values() {
    static constexpr auto infos = detail::enumerator_infos<E>();
    std::array<E, infos.size()> out{};
    std::size_t i = 0;
    template for (constexpr auto en : infos) {
        out[i++] = [: en :];
    }
    return out;
}

template <enum_like E>
consteval auto enum_names() {
    static constexpr auto infos = detail::enumerator_infos<E>();
    std::array<std::string_view, infos.size()> out{};
    std::size_t i = 0;
    template for (constexpr auto en : infos) {
        out[i++] = std::meta::identifier_of(en);
    }
    return out;
}

/// Source-spelled identifier of `value`, or `"<unknown>"` if it doesn't
/// match any declared enumerator (legal for raw casts and bit-flag combos).
template <enum_like E>
constexpr std::string_view enum_name(E value) {
    static constexpr auto entries = enum_entries<E>();
    for (auto const& [v, n] : entries) if (v == value) return n;
    return "<unknown>";
}

template <enum_like E>
constexpr std::optional<E> enum_value(std::string_view name) {
    static constexpr auto entries = enum_entries<E>();
    for (auto const& [v, n] : entries) if (n == name) return v;
    return std::nullopt;
}

template <enum_like E>
constexpr std::optional<std::size_t> enum_index(E value) {
    static constexpr auto values = enum_values<E>();
    for (std::size_t i = 0; i < values.size(); ++i)
        if (values[i] == value) return i;
    return std::nullopt;
}

template <enum_like E>
constexpr std::optional<E> enum_at(std::size_t index) {
    static constexpr auto values = enum_values<E>();
    if (index >= values.size()) return std::nullopt;
    return values[index];
}

/// Pretty-printed `"E::name"`.
template <enum_like E>
constexpr std::string enum_format(E value) {
    std::string out;
    constexpr auto tn = []{
        if (std::meta::has_identifier(^^E)) return std::meta::identifier_of(^^E);
        return std::meta::display_string_of(^^E);
    }();
    out.append(tn);
    out += "::";
    out.append(enum_name(value));
    return out;
}

template <enum_like E>
constexpr bool enum_contains(std::string_view name) {
    return enum_value<E>(name).has_value();
}

template <enum_like E>
constexpr bool enum_contains(E value) {
    return enum_index(value).has_value();
}

/// Safer than `static_cast`: returns nullopt when `u` doesn't match a
/// declared enumerator.
template <enum_like E>
constexpr std::optional<E> enum_cast(std::underlying_type_t<E> u) {
    E candidate = static_cast<E>(u);
    if (enum_contains<E>(candidate)) return candidate;
    return std::nullopt;
}

template <enum_like E>
constexpr std::underlying_type_t<E> enum_underlying(E value) noexcept {
    return static_cast<std::underlying_type_t<E>>(value);
}

template <enum_like E>
constexpr std::underlying_type_t<E> enum_min() {
    using U = std::underlying_type_t<E>;
    static constexpr auto values = enum_values<E>();
    if constexpr (values.empty()) return U{};
    else {
        U m = static_cast<U>(values[0]);
        for (auto v : values)
            if (static_cast<U>(v) < m) m = static_cast<U>(v);
        return m;
    }
}

template <enum_like E>
constexpr std::underlying_type_t<E> enum_max() {
    using U = std::underlying_type_t<E>;
    static constexpr auto values = enum_values<E>();
    if constexpr (values.empty()) return U{};
    else {
        U m = static_cast<U>(values[0]);
        for (auto v : values)
            if (static_cast<U>(v) > m) m = static_cast<U>(v);
        return m;
    }
}

/// Calls `fn(value)` or `fn(value, name)` once per enumerator, in source order.
template <enum_like E, class F>
constexpr void for_each_enum(F&& fn) {
    static constexpr auto entries = enum_entries<E>();
    for (auto const& [v, n] : entries) {
        if constexpr (std::is_invocable_v<F, E, std::string_view>) fn(v, n);
        else                                                       fn(v);
    }
}

/// Compile-time visit: instantiates `fn.template operator()<value>()` once
/// per enumerator, exposing each value as an NTTP inside `fn`.
template <enum_like E, class F>
constexpr void template_for_each_enum(F&& fn) {
    static constexpr auto entries = enum_entries<E>();
    template for (constexpr auto e : entries) {
        fn.template operator()<e.first>();
    }
}

template <class E>
struct enum_flags_policy {
    static constexpr bool is_flags = false;
};

template <class E>
concept enum_flag_like = enum_like<E> && enum_flags_policy<E>::is_flags;

/// Decompose a bitmask value into its enumerator names. Leftover bits are
/// rendered as a `"0x..."` element so round-tripping stays lossless.
template <enum_like E>
constexpr std::vector<std::string> enum_flag_names(E value) {
    using U = std::underlying_type_t<E>;
    static constexpr auto entries = enum_entries<E>();
    std::vector<std::string> out;
    U remaining = static_cast<U>(value);
    for (auto const& [v, n] : entries) {
        U bits = static_cast<U>(v);
        if (bits != 0 && (remaining & bits) == bits) {
            out.emplace_back(n);
            remaining &= static_cast<U>(~bits);
        }
    }
    if (remaining != 0) {
        char buf[2 + 16 + 1];
        std::snprintf(buf, sizeof(buf), "0x%llx",
                      static_cast<unsigned long long>(remaining));
        out.emplace_back(buf);
    }
    return out;
}

/// Build a flag value from a range of names; nullopt on any unknown name.
template <enum_like E, class Range>
constexpr std::optional<E> enum_flags_from(const Range& names) {
    using U = std::underlying_type_t<E>;
    U acc = 0;
    for (std::string_view n : names) {
        if (auto v = enum_value<E>(n)) acc |= static_cast<U>(*v);
        else return std::nullopt;
    }
    return static_cast<E>(acc);
}

/// `"a|b|c"`-style representation, or `"0"` if no flags are set.
template <enum_like E>
constexpr std::string enum_flag_format(E value) {
    auto names = enum_flag_names(value);
    if (names.empty()) return "0";
    std::string out = names.front();
    for (std::size_t i = 1; i < names.size(); ++i) {
        out += '|';
        out += names[i];
    }
    return out;
}

} // namespace rflcpp
