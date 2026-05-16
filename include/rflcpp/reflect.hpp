// rflcpp/reflect.hpp - C++26 reflection façade over user types.
// SPDX-License-Identifier: MIT
//
// Public entry points:
//   * rflcpp::field_count_of<T>()      - number of non-static data members
//   * rflcpp::field_names_of<T>()      - std::array of names
//   * rflcpp::for_each_field(obj, fn)  - iterate at compile time
//   * rflcpp::to_tuple(obj)            - decompose into tuple of references
//   * rflcpp::from_tuple<T>(tuple)     - rebuild a value from a tuple
//   * rflcpp::type_name_of<T>()        - spelled identifier of T
//
// rflcpp requires P2996 unconditionally. If <meta> is missing the include
// below fires a hard #error -- there is no fallback path.

#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#if __has_include(<meta>)
#  include <meta>
#elif __has_include(<experimental/meta>)
#  include <experimental/meta>
#else
#  error "rflcpp requires C++26 static reflection: neither <meta> nor "       \
         "<experimental/meta> is available. Use a P2996-capable compiler "    \
         "(e.g. GCC >= 16 with -freflection)."
#endif

#include <rflcpp/concepts.hpp>
#include <rflcpp/policy.hpp>

namespace rflcpp {

namespace detail {

/// Access context for every reflection query, driven by `access_policy<T>`.
template <class T>
consteval auto rfl_ctx_for() noexcept {
    if constexpr (access_policy<T>::mode == access_mode::public_only)
        return std::meta::access_context::unprivileged();
    else
        return std::meta::access_context::unchecked();
}

template <reflectable_class T>
consteval auto nonstatic_data_members() {
    constexpr auto ctx = rfl_ctx_for<T>();
    constexpr auto N   = std::meta::nonstatic_data_members_of(^^T, ctx).size();
    auto v             = std::meta::nonstatic_data_members_of(^^T, ctx);
    std::array<std::meta::info, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = v[i];
    return out;
}

template <reflectable_class T>
consteval auto direct_bases() {
    constexpr auto ctx = rfl_ctx_for<T>();
    constexpr auto N   = std::meta::bases_of(^^T, ctx).size();
    auto v             = std::meta::bases_of(^^T, ctx);
    std::array<std::meta::info, N> out{};
    for (std::size_t i = 0; i < N; ++i) out[i] = v[i];
    return out;
}

} // namespace detail

template <reflectable_class T>
consteval std::size_t field_count_of() {
    return std::meta::nonstatic_data_members_of(
        ^^T, detail::rfl_ctx_for<T>()).size();
}

template <reflectable_class T>
consteval std::size_t base_count_of() {
    return std::meta::bases_of(^^T, detail::rfl_ctx_for<T>()).size();
}

template <reflectable_class T>
consteval auto field_names_of() {
    static constexpr auto members = detail::nonstatic_data_members<T>();
    std::array<std::string_view, members.size()> out{};
    std::size_t i = 0;
    template for (constexpr auto member : members) {
        out[i++] = std::meta::identifier_of(member);
    }
    return out;
}

/// Invoke `fn(name, ref)` for each non-static data member of `obj`, where
/// `name` is a compile-time `string_view` and `ref` is a reference to the
/// member (const-qualified iff `obj` is const).
template <class T, class F>
    requires reflectable_class<T>
constexpr void for_each_field(T&& obj, F&& fn) {
    static constexpr auto members =
        detail::nonstatic_data_members<std::remove_cvref_t<T>>();
    template for (constexpr auto member : members) {
        fn(std::string_view{std::meta::identifier_of(member)},
           obj.[: member :]);
    }
}

template <class T>
    requires reflectable_class<T>
constexpr auto to_tuple(T&& obj) {
    static constexpr auto members =
        detail::nonstatic_data_members<std::remove_cvref_t<T>>();
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::tie(obj.[: members[Is] :] ...);
    }(std::make_index_sequence<members.size()>{});
}

template <reflectable_class T, class Tuple>
constexpr T from_tuple(Tuple&& t) {
    return std::apply(
        [](auto&&... args) -> T { return T{std::forward<decltype(args)>(args)...}; },
        std::forward<Tuple>(t));
}

/// The spelled identifier of `T`, or `display_string_of` for types without
/// one (built-ins, unnamed types).
template <class T>
consteval std::string_view type_name_of() {
    constexpr auto r = ^^T;
    if (std::meta::has_identifier(r))
        return std::meta::identifier_of(r);
    return std::meta::display_string_of(r);
}

// NOTE: a generic `for_each_base` helper used to live here but had to be
// removed -- GCC 16's P2996 implementation immediate-escalates any constexpr
// function whose `template for` body contains a type splice, which makes it
// impossible to call non-constexpr serializer code through such a helper.
// Callers expand the base-traversal loop inline; see `write_members` and
// `shape_object` in <rflcpp/json.hpp>.

} // namespace rflcpp
