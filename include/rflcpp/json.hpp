// rflcpp/json.hpp - JSON serialization using nlohmann/json.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef RFLCPP_ENABLE_JSON

#include <nlohmann/json.hpp>
#include <rflcpp/detail/serialization_common.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>
#include <rflcpp/patch.hpp>
#include <rflcpp/registry.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

namespace rflcpp {

using njson = nlohmann::json;

struct json_options {
    int indent = -1;
};

/// Specialize this to add custom-type support for `to_json` / `from_json`.
template <class T, class = void>
struct json_codec;

namespace detail::json {

using namespace rflcpp::detail::serialization;

template <class T>
njson write_dispatch(const T& v);

template <class T>
void write_members(njson& j, const T& obj) {
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
                    write_members(j, base_ref);
                } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                    njson inner_j = njson::object();
                    write_members(inner_j, base_ref);
                    j[std::string{type_name_of<B>()}] = std::move(inner_j);
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
                write_members(j, inner);
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
            j[key] = "***";
        } else {
            j[key] = detail::json::write_dispatch(inner);
        }
    });
}

template <class Variant>
njson write_variant(const Variant& v) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    return std::visit([&](auto const& alt) -> njson {
        using A = std::remove_cvref_t<decltype(alt)>;
        if constexpr (tagging == variant_tagging::external) {
            njson j = njson::object();
            j[std::string{type_name_of<A>()}] = write_dispatch(alt);
            return j;
        } else if constexpr (tagging == variant_tagging::internal) {
            njson j = njson::object();
            j[std::string{variant_policy<Variant>::tag_field}] = std::string{type_name_of<A>()};
            if constexpr (reflectable_class<A>) write_members(j, alt);
            else j["value"] = write_dispatch(alt);
            return j;
        } else if constexpr (tagging == variant_tagging::adjacent) {
            njson j = njson::object();
            j[std::string{variant_policy<Variant>::tag_field}] = std::string{type_name_of<A>()};
            j[std::string{variant_policy<Variant>::content_field}] = write_dispatch(alt);
            return j;
        } else { // untagged
            return write_dispatch(alt);
        }
    }, v);
}

template <class T>
njson write_dispatch(const T& v) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { json_codec<U>::write(v); }) {
        return json_codec<U>::write(v);
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
        return nullptr;
    }
    else if constexpr (variant_like<U>) {
        return write_variant(v);
    }
    else if constexpr (enum_like<U>) {
        if constexpr (rflcpp::enum_flags_policy<U>::is_flags) {
            auto names = rflcpp::enum_flag_names(v);
            njson j = njson::array();
            for (auto const& n : names) j.push_back(n);
            return j;
        } else {
            return std::string{rflcpp::enum_name(v)};
        }
    }
    else if constexpr (boolean_like<U>) {
        return bool(v);
    }
    else if constexpr (numeric_like<U>) {
        return v;
    }
    else if constexpr (string_like<U>) {
        return std::string{v};
    }
    else if constexpr (map_like<U>) {
        njson j = njson::object();
        for (auto const& [k, val] : v) j[std::string{k}] = write_dispatch(val);
        return j;
    }
    else if constexpr (sequence_like<U>) {
        njson j = njson::array();
        for (auto const& e : v) j.push_back(write_dispatch(e));
        return j;
    }
    else if constexpr (reflectable_class<U>) {
        njson j = njson::object();
        write_members(j, v);
        return j;
    }
    return nullptr;
}

template <class T>
result<T> read_dispatch(const njson& j, std::string_view path);

template <class Owner, class FieldType, class Out>
void read_member(const njson& parent, std::string_view member_name,
                 Out& out_field, std::string_view path,
                 std::optional<error>& failure)
{
    using F = std::remove_cvref_t<FieldType>;
    if (failure || skip_on_read_v<F>()) return;

    std::string canonical = effective_key<Owner, F>(member_name);
    auto it = parent.find(canonical);
    if (it == parent.end()) {
        constexpr auto ap = aliases_pair<F>();
        for (std::size_t i = 0; i < ap.second; ++i) {
            it = parent.find(std::string{ap.first[i]});
            if (it != parent.end()) break;
        }
    }

    auto& inner = unwrap_ref<F>(out_field);
    using InnerT = std::remove_cvref_t<decltype(inner)>;

    if (it == parent.end()) {
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

    auto res = read_dispatch<InnerT>(*it, path);
    if (!res) failure = res.error();
    else      inner = std::move(*res);
}

template <class T>
void read_members(const njson& j, T& obj, std::string_view path, std::optional<error>& failure) {
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
                    read_members(j, base_ref, path, failure);
                } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                    std::string key{type_name_of<B>()};
                    auto it = j.find(key);
                    if (it != j.end()) {
                        auto res = read_dispatch<B>(*it, path);
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
                read_members(j, inner, path, failure);
        } else {
            read_member<U, FT>(j, member_name, field_ref, path, failure);
        }
    });
}

template <class Variant>
result<Variant> read_variant(const njson& j, std::string_view path) {
    constexpr auto tagging = variant_policy<Variant>::tagging;

    auto try_read_alt = [&]<class Alt>() -> std::optional<result<Variant>> {
        auto res = read_dispatch<Alt>(j, path);
        if (res) return Variant(std::move(*res));
        return std::nullopt;
    };

    if constexpr (tagging == variant_tagging::external) {
        if (!j.is_object() || j.size() != 1)
            return fail({error_kind::type_mismatch, "expected object with single key for external variant", std::string{path}});
        std::string key = j.begin().key();
        const auto& val = j.begin().value();
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
        if (!j.is_object()) return fail({error_kind::type_mismatch, "expected object for internal variant", std::string{path}});
        std::string tag_field{variant_policy<Variant>::tag_field};
        auto it = j.find(tag_field);
        if (it == j.end() || !it->is_string())
            return fail({error_kind::missing_field, "missing tag field '" + tag_field + "'", std::string{path}});
        std::string tag = it->get<std::string>();
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (tag == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(j, path);
                    if (res) found = Variant(std::move(*res));
                    else     found = fail(res.error());
                }
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "unknown variant tag: " + tag, std::string{path}});
    }
    else if constexpr (tagging == variant_tagging::adjacent) {
        if (!j.is_object()) return fail({error_kind::type_mismatch, "expected object for adjacent variant", std::string{path}});
        std::string tag_field{variant_policy<Variant>::tag_field};
        std::string content_field{variant_policy<Variant>::content_field};
        auto it_tag = j.find(tag_field);
        auto it_val = j.find(content_field);
        if (it_tag == j.end() || !it_tag->is_string())
            return fail({error_kind::missing_field, "missing tag field '" + tag_field + "'", std::string{path}});
        if (it_val == j.end())
            return fail({error_kind::missing_field, "missing content field '" + content_field + "'", std::string{path}});
        std::string tag = it_tag->get<std::string>();
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (tag == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(*it_val, path);
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
                auto res = read_dispatch<Alt>(j, path);
                if (res) found = Variant(std::move(*res));
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "no alternative matched untagged variant", std::string{path}});
    }
}

template <class T>
result<T> read_dispatch(const njson& j, std::string_view path) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { json_codec<U>::read(j, path); }) {
        return json_codec<U>::read(j, path);
    }
    else if constexpr (is_validated_v<U>) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(j, path);
        if (!inner) return fail(inner.error());
        auto res = U::make(std::move(*inner));
        if (!res) return fail(error{error_kind::validation_failed, std::string{res.error().message()}, std::string{path}});
        return res;
    }
    else if constexpr (is_wrapped_v<U>) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(j, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (optional_like<U>) {
        if (j.is_null()) return U{std::nullopt};
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(j, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (variant_like<U>) {
        return read_variant<U>(j, path);
    }
    else if constexpr (boolean_like<U>) {
        if (j.is_boolean()) return j.get<bool>();
        return fail({error_kind::type_mismatch, "expected boolean", std::string{path}});
    }
    else if constexpr (numeric_like<U>) {
        if (j.is_number()) return j.get<U>();
        return fail({error_kind::type_mismatch, "expected number", std::string{path}});
    }
    else if constexpr (string_like<U>) {
        if (j.is_string()) return j.get<std::string>();
        return fail({error_kind::type_mismatch, "expected string", std::string{path}});
    }
    else if constexpr (enum_like<U>) {
        if (j.is_string()) {
            std::string s = j.get<std::string>();
            auto val = rflcpp::enum_value<U>(s);
            if (val) return *val;
            return fail({error_kind::type_mismatch, "invalid enum name: " + s, std::string{path}});
        } else if (j.is_number_integer()) {
            return static_cast<U>(j.get<long long>());
        } else if (j.is_array() && rflcpp::enum_flags_policy<U>::is_flags) {
            U out{};
            for (auto const& item : j) {
                if (item.is_string()) {
                    auto val = rflcpp::enum_value<U>(item.get<std::string>());
                    if (val) out = static_cast<U>(static_cast<std::underlying_type_t<U>>(out) | static_cast<std::underlying_type_t<U>>(*val));
                }
            }
            return out;
        }
        return fail({error_kind::type_mismatch, "expected string or integer for enum", std::string{path}});
    }
    else if constexpr (map_like<U>) {
        if (!j.is_object()) return fail({error_kind::type_mismatch, "expected object", std::string{path}});
        U out;
        for (auto it = j.begin(); it != j.end(); ++it) {
            auto val = read_dispatch<typename U::mapped_type>(it.value(), std::string{path} + "." + it.key());
            if (!val) return fail(val.error());
            out[it.key()] = std::move(*val);
        }
        return out;
    }
    else if constexpr (sequence_like<U>) {
        if (!j.is_array()) return fail({error_kind::type_mismatch, "expected array", std::string{path}});
        U out;
        int i = 0;
        for (auto const& item : j) {
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
        if (j.is_object()) {
            if constexpr (strict_policy<U>::strict) {
                for (auto it = j.begin(); it != j.end(); ++it) {
                    if (!rflcpp::detail::serialization::is_valid_key<U>(it.key())) {
                        return fail({error_kind::unknown_field, "unknown field '" + it.key() + "'", std::string{path}});
                    }
                }
            }
            struct validation_guard {
                validation_guard() { rflcpp::detail::g_bypass_validation = true; }
                ~validation_guard() { rflcpp::detail::g_bypass_validation = false; }
            } guard;
            U val;
            std::optional<error> failure;
            read_members(j, val, path, failure);
            if (failure) return fail(*failure);
            return val;
        }
        return fail({error_kind::type_mismatch, "expected object", std::string{path}});
    }
    return fail({error_kind::type_mismatch, "unsupported type", std::string{path}});
}

} // namespace detail::json

template <class T>
njson to_json_dispatch_helper(const T& v) {
    return detail::json::write_dispatch(v);
}

template <>
struct json_codec<any, void> {
    static njson write(const any& a) {
        return a.to_json();
    }
};

template <class T>
[[nodiscard]] std::string to_json(const T& value, json_options opts = {}) {
    njson j = detail::json::write_dispatch(value);
    return j.dump(opts.indent);
}

template <class T>
[[nodiscard]] result<T> from_json(std::string_view json_str) {
    try {
        njson j = njson::parse(json_str);
        return detail::json::read_dispatch<T>(j, "$");
    } catch (const njson::parse_error& e) {
        return fail(error{error_kind::parse_error, e.what(), "$"});
    }
}

// Specializations for reflection-native modernisation features

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
    
    static result<patch_type<T>> read(const njson& j, std::string_view path = "$") {
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

template <class Registry, fixed_string TagField>
struct json_codec<registered_any<Registry, TagField>> {
    static njson write(const registered_any<Registry, TagField>& ra) {
        if (ra.value.empty()) return nullptr;
        
        njson j = nullptr;
        bool found = false;
        using Tuple = typename Registry::types;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using T = std::tuple_element_t<Is, Tuple>;
                if (ra.value.type_id() == typeid(T)) {
                    found = true;
                    if (auto ptr = ra.value.template cast<T>()) {
                        j = rflcpp::detail::json::write_dispatch(*ptr);
                    }
                }
            }(), ...);
        }(std::make_index_sequence<std::tuple_size_v<Tuple>>{});
        
        if (j.is_object()) {
            j[std::string{TagField.view()}] = std::string{ra.value.type_name()};
        }
        return j;
    }
    
    static result<registered_any<Registry, TagField>> read(const njson& j, std::string_view path = "$") {
        if (!j.is_object()) return fail({error_kind::type_mismatch, "expected object for registered_any", std::string{path}});
        auto it = j.find(std::string{TagField.view()});
        if (it == j.end() || !it->is_string()) {
            return fail({error_kind::missing_field, "missing tag field '" + std::string{TagField.view()} + "'", std::string{path}});
        }
        std::string tag = it->get<std::string>();
        auto res = Registry::deserialize_json(tag, j, path);
        if (!res) return fail(res.error());
        return registered_any<Registry, TagField>{std::move(*res)};
    }
};

template <class... Types>
template <class JsonType>
rflcpp::result<rflcpp::any> type_registry<Types...>::deserialize_json(std::string_view tag, const JsonType& j, std::string_view path) {
    rflcpp::result<rflcpp::any> out = fail(error{error_kind::type_mismatch, "unknown type tag: " + std::string{tag}, std::string{path}});
    bool found = false;
    ([&] {
        if (found) return;
        if (tag == type_name_of<Types>()) {
            found = true;
            auto res = rflcpp::detail::json::read_dispatch<Types>(j, path);
            if (res) out = rflcpp::any(std::move(*res));
            else     out = fail(res.error());
        }
    }(), ...);
    return out;
}

} // namespace rflcpp

#endif // RFLCPP_ENABLE_JSON
