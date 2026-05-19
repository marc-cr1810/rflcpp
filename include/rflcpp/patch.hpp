// rflcpp/patch.hpp - JSON Merge Patch and Struct Diffing for reflectable classes.
// SPDX-License-Identifier: MIT

#pragma once

#include <tuple>
#include <optional>
#include <utility>
#include <type_traits>
#include <string_view>
#include <string>

#include <rflcpp/reflect.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/attributes.hpp>
#include <rflcpp/detail/serialization_common.hpp>

namespace rflcpp {

namespace detail {
template <class T, class Seq>
struct patch_tuple_helper;

template <class T, std::size_t... Is>
struct patch_tuple_helper<T, std::index_sequence<Is...>> {
    using U = std::remove_cvref_t<T>;
    using type = std::tuple<std::optional<rflcpp::unwrap_t<typename [: std::meta::type_of(
        rflcpp::members_of<U>()[Is]) :]>>...>;
};
} // namespace detail

template <class T>
struct patch_type {
    using target_type = T;
    
    using tuple_type = typename detail::patch_tuple_helper<T, std::make_index_sequence<rflcpp::field_count_of<T>()>>::type;
    
    tuple_type values;
    
    constexpr patch_type() = default;
    constexpr explicit patch_type(tuple_type v) : values(std::move(v)) {}
};

template <class T>
using patch_type_t = patch_type<T>;

/// Computes the difference between before and after, returning a patch_type.
template <class T>
    requires reflectable_class<T>
constexpr patch_type_t<T> diff(const T& before, const T& after) {
    using U = std::remove_cvref_t<T>;
    constexpr auto N = field_count_of<U>();
    typename patch_type_t<T>::tuple_type t;
    
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            constexpr auto member = rflcpp::members_of<U>()[Is];
            const auto& b_val = before.[: member :];
            const auto& a_val = after.[: member :];
            
            using FT = typename [: std::meta::type_of(member) :];
            const auto& b_unwrapped = rflcpp::detail::serialization::unwrap_ref<FT>(b_val);
            const auto& a_unwrapped = rflcpp::detail::serialization::unwrap_ref<FT>(a_val);
            
            if (b_unwrapped != a_unwrapped) {
                std::get<Is>(t) = a_unwrapped;
            }
        }(), ...);
    }(std::make_index_sequence<N>{});
    
    return patch_type_t<T>(std::move(t));
}

/// Applies a patch_type in-place to target.
template <class T>
    requires reflectable_class<T>
constexpr void patch(T& target, const patch_type_t<T>& p) {
    using U = std::remove_cvref_t<T>;
    constexpr auto N = field_count_of<U>();
    
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        ([&] {
            const auto& opt = std::get<Is>(p.values);
            if (opt.has_value()) {
                constexpr auto member = rflcpp::members_of<U>()[Is];
                using FT = typename [: std::meta::type_of(member) :];
                auto& field_ref = target.[: member :];
                auto& inner = rflcpp::detail::serialization::unwrap_ref<FT>(field_ref);
                inner = *opt;
            }
        }(), ...);
    }(std::make_index_sequence<N>{});
}

} // namespace rflcpp
