// rflcpp/reflect.hpp - C++26 reflection façade over user types.
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

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

} // namespace detail

template <reflectable_class T>
consteval auto members_of() {
    using U = std::remove_cvref_t<T>;
    return std::meta::nonstatic_data_members_of(^^U, detail::rfl_ctx_for<U>());
}

template <reflectable_class T>
consteval std::size_t field_count_of() {
    return members_of<T>().size();
}

template <reflectable_class T>
consteval std::size_t base_count_of() {
    return std::meta::bases_of(^^T, detail::rfl_ctx_for<T>()).size();
}

template <reflectable_class T>
consteval auto field_names_of() {
    using U = std::remove_cvref_t<T>;
    constexpr auto N = field_count_of<U>();
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::array<std::string_view, N>{
            std::meta::identifier_of(members_of<U>()[Is])...
        };
    }(std::make_index_sequence<N>{});
}

/// Invoke `fn(name, ref)` for each non-static data member of `obj`, where
/// `name` is a compile-time `string_view` and `ref` is a reference to the
/// member (const-qualified iff `obj` is const).
template <class T, class F>
    requires reflectable_class<T>
constexpr void for_each_field(T&& obj, F&& fn) {
    using U = std::remove_cvref_t<T>;
    constexpr auto N = field_count_of<U>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fn(std::string_view{std::meta::identifier_of(members_of<U>()[Is])},
            obj.[: members_of<U>()[Is] :]), ...);
    }(std::make_index_sequence<N>{});
}

/// Invoke `fn.template operator()<MemberType>(name)` for each non-static data
/// member of `T`. Useful for schema generation where no instance is needed.
template <class T, class F>
    requires reflectable_class<T>
constexpr void template_for_each_field(F&& fn) {
    using U = std::remove_cvref_t<T>;
    constexpr auto N = field_count_of<U>();
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (fn.template operator()<typename [: std::meta::type_of(members_of<U>()[Is]) :]> (
            std::string_view{std::meta::identifier_of(members_of<U>()[Is])}), ...);
    }(std::make_index_sequence<N>{});
}


template <class T>
    requires reflectable_class<T>
constexpr auto to_tuple(T&& obj) {
    using U = std::remove_cvref_t<T>;
    constexpr auto N = field_count_of<U>();
    return [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        return std::tie(obj.[: members_of<U>()[Is] :] ...);
    }(std::make_index_sequence<N>{});
}

template <reflectable_class T, class Tuple>
    requires std::is_aggregate_v<T>
constexpr T from_tuple(Tuple&& t) {
    return std::apply(
        [](auto&&... args) -> T { return T{std::forward<decltype(args)>(args)...}; },
        std::forward<Tuple>(t));
}

template <class T>
struct type_name_helper {
    static consteval std::string_view name() {
        constexpr auto r = ^^T;
        if (std::meta::has_identifier(r))
            return std::meta::identifier_of(r);
        return std::meta::display_string_of(r);
    }
};

template <>
struct type_name_helper<std::string> {
    static consteval std::string_view name() { return "string"; }
};

/// The spelled identifier of `T`, or `display_string_of` for types without
/// one (built-ins, unnamed types).
template <class T>
consteval std::string_view type_name_of() {
    return type_name_helper<std::remove_cvref_t<T>>::name();
}

} // namespace rflcpp
