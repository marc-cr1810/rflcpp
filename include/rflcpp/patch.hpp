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

#ifdef RFLCPP_ENABLE_JSON
#include <rflcpp/json.hpp>
#endif
#ifdef RFLCPP_ENABLE_YAML
#include <rflcpp/yaml.hpp>
#endif
#ifdef RFLCPP_ENABLE_XML
#include <rflcpp/xml.hpp>
#endif
#ifdef RFLCPP_ENABLE_TOML
#include <rflcpp/toml.hpp>
#endif
#ifdef RFLCPP_ENABLE_MSGPACK
#include <rflcpp/msgpack.hpp>
#endif

namespace rflcpp {
namespace detail {
template <class T, class Seq>
struct patch_tuple_helper;

template <class T, std::size_t... Is>
struct patch_tuple_helper<T, std::index_sequence<Is...>> {
    using U = std::remove_cvref_t<T>;
    using type = std::tuple<std::optional<rflcpp::unwrap_t<typename [: std::meta::type_of(
        std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is]) :]>>...>;
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
            constexpr auto member = std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
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
                constexpr auto member = std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                using FT = typename [: std::meta::type_of(member) :];
                auto& field_ref = target.[: member :];
                auto& inner = rflcpp::detail::serialization::unwrap_ref<FT>(field_ref);
                inner = *opt;
            }
        }(), ...);
    }(std::make_index_sequence<N>{});
}

} // namespace rflcpp

#ifdef RFLCPP_ENABLE_JSON
namespace rflcpp {

template <class T>
struct json_codec<patch_type<T>> {
    static njson write(const patch_type<T>& p) {
        njson j = njson::object();
        using U = std::remove_cvref_t<T>;
        constexpr auto N = rflcpp::field_count_of<U>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto member = std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                constexpr auto member_name = std::meta::identifier_of(member);
                using FT = typename [: std::meta::type_of(member) :];
                std::string key = rflcpp::detail::serialization::effective_key<U, FT>(member_name);
                
                const auto& opt = std::get<Is>(p.values);
                if (opt.has_value()) {
                    if constexpr (rflcpp::optional_like<typename std::decay_t<decltype(opt)>::value_type>) {
                        if (!opt->has_value()) {
                            j[key] = nullptr;
                        } else {
                            j[key] = rflcpp::detail::json::write_dispatch(**opt);
                        }
                    } else {
                        j[key] = rflcpp::detail::json::write_dispatch(*opt);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<N>{});
        return j;
    }
    
    static result<patch_type<T>> read(const njson& j, std::string_view path) {
        if (!j.is_object()) return fail({error_kind::type_mismatch, "expected object for patch", std::string{path}});
        patch_type<T> p;
        std::optional<error> failure;
        using U = std::remove_cvref_t<T>;
        constexpr auto N = rflcpp::field_count_of<U>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (failure) return;
                constexpr auto member = std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                constexpr auto member_name = std::meta::identifier_of(member);
                using FT = typename [: std::meta::type_of(member) :];
                std::string key = rflcpp::detail::serialization::effective_key<U, FT>(member_name);
                
                auto it = j.find(key);
                if (it != j.end()) {
                    using ValueType = typename std::decay_t<decltype(std::get<Is>(p.values))>::value_type;
                    if constexpr (rflcpp::optional_like<ValueType>) {
                        if (it->is_null()) {
                            std::get<Is>(p.values) = ValueType(std::nullopt);
                        } else {
                            using InnerValType = typename ValueType::value_type;
                            auto res = rflcpp::detail::json::read_dispatch<InnerValType>(*it, std::string{path} + "." + key);
                            if (!res) failure = res.error();
                            else      std::get<Is>(p.values) = ValueType(std::move(*res));
                        }
                    } else {
                        auto res = rflcpp::detail::json::read_dispatch<ValueType>(*it, std::string{path} + "." + key);
                        if (!res) failure = res.error();
                        else      std::get<Is>(p.values) = std::move(*res);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<N>{});
        
        if (failure) return fail(*failure);
        return p;
    }
};

} // namespace rflcpp
#endif

#ifdef RFLCPP_ENABLE_YAML
namespace rflcpp {

template <class T>
struct yaml_codec<patch_type<T>> {
    static YAML::Node write(const patch_type<T>& p) {
        YAML::Node node;
        using U = std::remove_cvref_t<T>;
        constexpr auto N = rflcpp::field_count_of<U>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto member = std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                constexpr auto member_name = std::meta::identifier_of(member);
                using FT = typename [: std::meta::type_of(member) :];
                std::string key = rflcpp::detail::serialization::effective_key<U, FT>(member_name);
                
                const auto& opt = std::get<Is>(p.values);
                if (opt.has_value()) {
                    if constexpr (rflcpp::optional_like<typename std::decay_t<decltype(opt)>::value_type>) {
                        if (!opt->has_value()) {
                            node[key] = YAML::Node(YAML::NodeType::Null);
                        } else {
                            node[key] = rflcpp::detail::yaml::write_dispatch(**opt);
                        }
                    } else {
                        node[key] = rflcpp::detail::yaml::write_dispatch(*opt);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<N>{});
        return node;
    }
    
    static result<patch_type<T>> read(const YAML::Node& node, std::string_view path = "$") {
        if (!node.IsMap()) return fail({error_kind::type_mismatch, "expected map for patch", std::string{path}});
        patch_type<T> p;
        std::optional<error> failure;
        using U = std::remove_cvref_t<T>;
        constexpr auto N = rflcpp::field_count_of<U>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (failure) return;
                constexpr auto member = std::meta::nonstatic_data_members_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                constexpr auto member_name = std::meta::identifier_of(member);
                using FT = typename [: std::meta::type_of(member) :];
                std::string key = rflcpp::detail::serialization::effective_key<U, FT>(member_name);
                
                YAML::Node found = node[key];
                if (found.IsDefined()) {
                    using ValueType = typename std::decay_t<decltype(std::get<Is>(p.values))>::value_type;
                    if constexpr (rflcpp::optional_like<ValueType>) {
                        if (found.IsNull()) {
                            std::get<Is>(p.values) = ValueType(std::nullopt);
                        } else {
                            using InnerValType = typename ValueType::value_type;
                            auto res = rflcpp::detail::yaml::read_dispatch<InnerValType>(found, std::string{path} + "." + key);
                            if (!res) failure = res.error();
                            else      std::get<Is>(p.values) = ValueType(std::move(*res));
                        }
                    } else {
                        auto res = rflcpp::detail::yaml::read_dispatch<ValueType>(found, std::string{path} + "." + key);
                        if (!res) failure = res.error();
                        else      std::get<Is>(p.values) = std::move(*res);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<N>{});
        
        if (failure) return fail(*failure);
        return p;
    }
};

} // namespace rflcpp
#endif
