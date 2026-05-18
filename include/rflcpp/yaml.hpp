// rflcpp/yaml.hpp - YAML serialization using yaml-cpp.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef RFLCPP_ENABLE_YAML

#include <yaml-cpp/yaml.h>
#include <rflcpp/detail/serialization_common.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>
#include <rflcpp/patch.hpp>
#include <rflcpp/registry.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace rflcpp {

struct yaml_options {
    /// YAML indentation.
    int indent = 2;
};

/// Specialize this to add custom-type support for `to_yaml` / `from_yaml`.
template <class T, class = void>
struct yaml_codec;

namespace detail::yaml {

using namespace rflcpp::detail::serialization;

template <class T>
YAML::Node write_dispatch(const T& v);

template <class T>
void write_members(YAML::Node& node, const T& obj) {
    using U = std::remove_cvref_t<T>;

    if constexpr (base_count_of<U>() > 0 &&
                  base_policy<U>::mode != base_mode::skip) {
        constexpr auto B_COUNT = base_count_of<U>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                constexpr auto base = std::meta::bases_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                using B = typename [: std::meta::type_of(base) :];
                const B& base_ref = static_cast<const B&>(obj);
                if constexpr (base_policy<U>::mode == base_mode::flatten) {
                    write_members(node, base_ref);
                } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                    node[std::string{rflcpp::type_name_of<B>()}] = write_dispatch(base_ref);
                }
            }(), ...);
        }(std::make_index_sequence<B_COUNT>{});
    }

    for_each_field(obj, [&](std::string_view member_name, auto const& field_ref) {
        using FT = std::remove_cvref_t<decltype(field_ref)>;

        if constexpr (skip_on_write_v<FT>()) return;

        if constexpr (flatten_v<FT>()) {
            auto const& inner = unwrap_ref<FT>(field_ref);
            if constexpr (reflectable_class<std::remove_cvref_t<decltype(inner)>>)
                write_members(node, inner);
            return;
        }

        auto const& inner = unwrap_ref<FT>(field_ref);
        using InnerT = std::remove_cvref_t<decltype(inner)>;

        if constexpr (skip_if_null_v<FT>() && optional_like<InnerT>) {
            if (!inner.has_value()) return;
        }
        if constexpr (skip_if_default_v<FT>() &&
                      std::equality_comparable<InnerT> &&
                      std::default_initializable<InnerT>) {
            if (inner == InnerT{}) return;
        }

        std::string key = effective_key<U, FT>(member_name);

        if constexpr (sensitive_v<FT>()) {
            node[key] = "***";
        } else {
            node[key] = detail::yaml::write_dispatch(inner);
        }
    });
}

template <class Variant>
YAML::Node write_variant(const Variant& v) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    return std::visit([&](auto const& alt) -> YAML::Node {
        using A = std::remove_cvref_t<decltype(alt)>;
        if constexpr (tagging == variant_tagging::external) {
            YAML::Node node;
            node[std::string{rflcpp::type_name_of<A>()}] = write_dispatch(alt);
            return node;
        } else if constexpr (tagging == variant_tagging::internal) {
            YAML::Node node;
            node[std::string{variant_policy<Variant>::tag_field}] = std::string{rflcpp::type_name_of<A>()};
            if constexpr (reflectable_class<A>) write_members(node, alt);
            else node["value"] = write_dispatch(alt);
            return node;
        } else if constexpr (tagging == variant_tagging::adjacent) {
            YAML::Node node;
            node[std::string{variant_policy<Variant>::tag_field}] = std::string{rflcpp::type_name_of<A>()};
            node[std::string{variant_policy<Variant>::content_field}] = write_dispatch(alt);
            return node;
        } else { // untagged
            return write_dispatch(alt);
        }
    }, v);
}

template <class T>
YAML::Node write_dispatch(const T& v) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { yaml_codec<U>::write(v); }) {
        return yaml_codec<U>::write(v);
    }
    else if constexpr (requires { v.get(); typename U::value_type;
                                  requires std::same_as<U,
                                      validated<typename U::value_type>>; }) {
        return write_dispatch(v.get());
    }
    else if constexpr (is_wrapped_v<U>) {
        return write_dispatch(v.value);
    }
    else if constexpr (optional_like<U>) {
        if (v.has_value()) return write_dispatch(*v);
        return YAML::Node(YAML::NodeType::Null);
    }
    else if constexpr (variant_like<U>) {
        return write_variant(v);
    }
    else if constexpr (enum_like<U>) {
        if constexpr (rflcpp::enum_flags_policy<U>::is_flags) {
            YAML::Node node;
            auto names = rflcpp::enum_flag_names(v);
            for (auto const& n : names) node.push_back(std::string{n});
            return node;
        } else {
            return YAML::Node(std::string{rflcpp::enum_name(v)});
        }
    }
    else if constexpr (boolean_like<U>) {
        return YAML::Node(bool(v));
    }
    else if constexpr (numeric_like<U>) {
        return YAML::Node(v);
    }
    else if constexpr (string_like<U>) {
        return YAML::Node(std::string{v});
    }
    else if constexpr (map_like<U>) {
        YAML::Node node;
        for (auto const& [k, val] : v) node[std::string{k}] = write_dispatch(val);
        return node;
    }
    else if constexpr (sequence_like<U>) {
        YAML::Node node;
        for (auto const& e : v) node.push_back(write_dispatch(e));
        return node;
    }
    else if constexpr (reflectable_class<U>) {
        YAML::Node node;
        write_members(node, v);
        return node;
    }
    return YAML::Node(YAML::NodeType::Null);
}

template <class T>
result<T> read_dispatch(const YAML::Node& node, std::string_view path);

template <class Owner, class FieldType, class Out>
void read_member(const YAML::Node& parent, std::string_view member_name,
                 Out& out_field, std::string_view path,
                 std::optional<error>& failure)
{
    using F = std::remove_cvref_t<FieldType>;
    if (failure || skip_on_read_v<F>()) return;

    std::string canonical = effective_key<Owner, F>(member_name);
    YAML::Node found = parent[canonical];
    if (!found.IsDefined()) {
        constexpr auto ap = aliases_pair<F>();
        for (std::size_t i = 0; i < ap.second; ++i) {
            std::string alias{ap.first[i]};
            found = parent[alias];
            if (found.IsDefined()) break;
        }
    }

    auto& inner = unwrap_ref<F>(out_field);
    using InnerT = std::remove_cvref_t<decltype(inner)>;

    if (!found.IsDefined()) {
        if constexpr (has_default_v<F>()) {
            inner = default_v<F>();
            return;
        }
        if constexpr (!required_v<F>() && optional_like<InnerT>) return;
        failure = error{error_kind::missing_field,
                        "missing required field '" + canonical + "'",
                        std::string{path}};
        return;
    }

    auto res = read_dispatch<InnerT>(found, path);
    if (!res) failure = res.error();
    else      inner = std::move(*res);
}

template <class T>
void read_members(const YAML::Node& node, T& obj, std::string_view path, std::optional<error>& failure) {
    using U = std::remove_cvref_t<T>;

    if constexpr (base_count_of<U>() > 0 &&
                  base_policy<U>::mode != base_mode::skip) {
        constexpr auto B_COUNT = base_count_of<U>();
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (failure) return;
                constexpr auto base = std::meta::bases_of(^^U, rflcpp::detail::rfl_ctx_for<U>())[Is];
                using B = typename [: std::meta::type_of(base) :];
                B& base_ref = static_cast<B&>(obj);
                if constexpr (base_policy<U>::mode == base_mode::flatten) {
                    read_members(node, base_ref, path, failure);
                } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                    std::string key{rflcpp::type_name_of<B>()};
                    if (auto found = node[key]) {
                        auto res = read_dispatch<B>(found, path);
                        if (!res) failure = res.error();
                        else      base_ref = std::move(*res);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<B_COUNT>{});
    }

    for_each_field(obj, [&](std::string_view member_name, auto& field_ref) {
        if (failure) return;
        using FT = std::remove_cvref_t<decltype(field_ref)>;
        if constexpr (flatten_v<FT>()) {
            auto& inner = unwrap_ref<FT>(field_ref);
            if constexpr (reflectable_class<std::remove_cvref_t<decltype(inner)>>)
                read_members(node, inner, path, failure);
        } else {
            read_member<U, FT>(node, member_name, field_ref, path, failure);
        }
    });
}

template <class Variant>
result<Variant> read_variant(const YAML::Node& node, std::string_view path) {
    constexpr auto tagging = variant_policy<Variant>::tagging;

    if constexpr (tagging == variant_tagging::external) {
        if (!node.IsMap() || node.size() != 1)
            return fail({error_kind::type_mismatch, "expected map with single key for external variant", std::string{path}});
        std::string key = node.begin()->first.as<std::string>();
        const YAML::Node& val = node.begin()->second;
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (key == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(val, path);
                    if (res) found = Variant(std::move(*res));
                    else     found = fail(res.error());
                }
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "unknown variant tag: " + key, std::string{path}});
    }
    else if constexpr (tagging == variant_tagging::internal) {
        if (!node.IsMap()) return fail({error_kind::type_mismatch, "expected map for internal variant", std::string{path}});
        std::string tag_field{variant_policy<Variant>::tag_field};
        if (!node[tag_field])
            return fail({error_kind::missing_field, "missing tag field '" + tag_field + "'", std::string{path}});
        std::string tag = node[tag_field].as<std::string>();
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (tag == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(node, path);
                    if (res) found = Variant(std::move(*res));
                    else     found = fail(res.error());
                }
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "unknown variant tag: " + tag, std::string{path}});
    }
    else if constexpr (tagging == variant_tagging::adjacent) {
        if (!node.IsMap()) return fail({error_kind::type_mismatch, "expected map for adjacent variant", std::string{path}});
        std::string tag_field{variant_policy<Variant>::tag_field};
        std::string content_field{variant_policy<Variant>::content_field};
        if (!node[tag_field])
            return fail({error_kind::missing_field, "missing tag field '" + tag_field + "'", std::string{path}});
        if (!node[content_field])
            return fail({error_kind::missing_field, "missing content field '" + content_field + "'", std::string{path}});
        std::string tag = node[tag_field].as<std::string>();
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (tag == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(node[content_field], path);
                    if (res) found = Variant(std::move(*res));
                    else     found = fail(res.error());
                }
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "unknown variant tag: " + tag, std::string{path}});
    }
    else { // untagged
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                auto res = read_dispatch<Alt>(node, path);
                if (res) found = Variant(std::move(*res));
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "no alternative matched untagged variant", std::string{path}});
    }
}

template <class T>
result<T> read_dispatch(const YAML::Node& node, std::string_view path) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { yaml_codec<U>::read(node); }) {
        return yaml_codec<U>::read(node);
    }
    else if constexpr (is_validated_v<U>) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        auto res = U::make(std::move(*inner));
        if (!res) return fail(error{error_kind::validation_failed, std::string{res.error().message()}, std::string{path}});
        return res;
    }
    else if constexpr (is_wrapped_v<U>) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (optional_like<U>) {
        if (node.IsNull() || !node.IsDefined()) return U{std::nullopt};
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (variant_like<U>) {
        return read_variant<U>(node, path);
    }
    else if constexpr (boolean_like<U>) {
        try { return node.as<bool>(); }
        catch (...) { return fail({error_kind::type_mismatch, "expected boolean", std::string{path}}); }
    }
    else if constexpr (numeric_like<U>) {
        try { return node.as<U>(); }
        catch (...) { return fail({error_kind::type_mismatch, "expected number", std::string{path}}); }
    }
    else if constexpr (string_like<U>) {
        try { return node.as<std::string>(); }
        catch (...) { return fail({error_kind::type_mismatch, "expected string", std::string{path}}); }
    }
    else if constexpr (enum_like<U>) {
        try {
            std::string s = node.as<std::string>();
            auto val = rflcpp::enum_value<U>(s);
            if (val) return *val;
        } catch (...) {}
        return fail({error_kind::type_mismatch, "invalid enum", std::string{path}});
    }
    else if constexpr (map_like<U>) {
        if (!node.IsMap()) return fail({error_kind::type_mismatch, "expected map", std::string{path}});
        U out;
        for (auto const& item : node) {
            auto val = read_dispatch<typename U::mapped_type>(item.second, std::string{path} + "." + item.first.as<std::string>());
            if (!val) return fail(val.error());
            out[item.first.as<std::string>()] = std::move(*val);
        }
        return out;
    }
    else if constexpr (sequence_like<U>) {
        if (!node.IsSequence()) return fail({error_kind::type_mismatch, "expected sequence", std::string{path}});
        U out;
        int i = 0;
        for (auto const& item : node) {
            auto val = read_dispatch<typename U::value_type>(item, std::string{path} + "[" + std::to_string(i++) + "]");
            if (!val) return fail(val.error());
            if constexpr (requires { out.push_back(std::move(*val)); })
                out.push_back(std::move(*val));
            else if constexpr (requires { out.insert(std::move(*val)); })
                out.insert(std::move(*val));
        }
        return out;
    }
    else if constexpr (reflectable_class<U>) {
        if (!node.IsMap()) return fail({error_kind::type_mismatch, "expected map", std::string{path}});
        if constexpr (strict_policy<U>::strict) {
            for (auto const& item : node) {
                std::string k = item.first.as<std::string>();
                if (!rflcpp::detail::serialization::is_valid_key<U>(k)) {
                    return fail({error_kind::unknown_field, "unknown field '" + k + "'", std::string{path}});
                }
            }
        }
        struct validation_guard {
            validation_guard() { rflcpp::detail::g_bypass_validation = true; }
            ~validation_guard() { rflcpp::detail::g_bypass_validation = false; }
        } guard;
        U val;
        std::optional<error> failure;
        read_members(node, val, path, failure);
        if (failure) return fail(*failure);
        return val;
    }
    return fail({error_kind::type_mismatch, "unsupported type", std::string{path}});
}

} // namespace detail::yaml

template <class T>
[[nodiscard]] std::string to_yaml(const T& value, yaml_options opts = {}) {
    YAML::Node node = detail::yaml::write_dispatch(value);
    YAML::Emitter out;
    out << node;
    return out.c_str();
}

template <class T>
[[nodiscard]] result<T> from_yaml(std::string_view yaml_str) {
    try {
        YAML::Node node = YAML::Load(std::string{yaml_str});
        return detail::yaml::read_dispatch<T>(node, "$");
    } catch (const YAML::Exception& e) {
        return fail({error_kind::parse_error, e.what(), "$"});
    }
}

// Specializations for reflection-native modernisation features

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

template <class Registry, fixed_string TagField>
struct yaml_codec<registered_any<Registry, TagField>> {
    static YAML::Node write(const registered_any<Registry, TagField>& ra) {
        if (ra.value.empty()) return YAML::Node(YAML::NodeType::Null);
        
        YAML::Node node;
        bool found = false;
        using Tuple = typename Registry::types;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using T = std::tuple_element_t<Is, Tuple>;
                if (ra.value.type_id() == typeid(T)) {
                    found = true;
                    if (auto ptr = ra.value.template cast<T>()) {
                        YAML::Node inner = rflcpp::detail::yaml::write_dispatch(*ptr);
                        if constexpr (reflectable_class<T>) {
                            node = inner;
                            node[std::string{TagField.view()}] = std::string{ra.value.type_name()};
                        } else {
                            node[std::string{TagField.view()}] = std::string{ra.value.type_name()};
                            node["value"] = inner;
                        }
                    }
                }
            }(), ...);
        }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        return node;
    }
    
    static result<registered_any<Registry, TagField>> read(const YAML::Node& node, std::string_view path = "$") {
        if (!node.IsMap()) return fail({error_kind::type_mismatch, "expected map for registered_any", std::string{path}});
        auto tag_node = node[std::string{TagField.view()}];
        if (!tag_node.IsDefined() || tag_node.IsNull()) {
            return fail({error_kind::missing_field, "missing tag field '" + std::string{TagField.view()} + "'", std::string{path}});
        }
        std::string tag = tag_node.as<std::string>();
        auto res = Registry::deserialize_yaml(tag, node, path);
        if (!res) return fail(res.error());
        return registered_any<Registry, TagField>{std::move(*res)};
    }
};

template <class... Types>
template <class YamlNodeType>
rflcpp::result<rflcpp::any> type_registry<Types...>::deserialize_yaml(std::string_view tag, const YamlNodeType& node, std::string_view path) {
    rflcpp::result<rflcpp::any> out = fail(error{error_kind::type_mismatch, "unknown type tag: " + std::string{tag}, std::string{path}});
    bool found = false;
    ([&] {
        if (found) return;
        if (tag == type_name_of<Types>()) {
            found = true;
            auto res = rflcpp::detail::yaml::read_dispatch<Types>(node, path);
            if (res) out = rflcpp::any(std::move(*res));
            else     out = fail(res.error());
        }
    }(), ...);
    return out;
}

} // namespace rflcpp

#endif // RFLCPP_ENABLE_YAML
