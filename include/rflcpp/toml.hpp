// rflcpp/toml.hpp - TOML serialization using tomlplusplus.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef RFLCPP_ENABLE_TOML

#include <toml++/toml.hpp>
#include <rflcpp/detail/serialization_common.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace rflcpp {

struct toml_options {
    /// TOML indentation.
    bool indent = true;
};

/// Specialize this to add custom-type support for `to_toml` / `from_toml`.
template <class T, class = void>
struct toml_codec;

namespace detail::toml {

using namespace rflcpp::detail::serialization;

// tomlplusplus uses unique_ptr for node ownership.
using node_ptr = std::unique_ptr<::toml::node>;

template <class T>
node_ptr write_dispatch(const T& v);

template <class T>
void write_members(::toml::table& tbl, const T& obj) {
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
                    write_members(tbl, base_ref);
                } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                    auto inner_tbl = std::make_unique<::toml::table>();
                    write_members(*inner_tbl, base_ref);
                    tbl.insert(std::string{rflcpp::type_name_of<B>()}, std::move(*inner_tbl));
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
                write_members(tbl, inner);
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
            tbl.insert(key, "***");
        } else {
            node_ptr node = detail::toml::write_dispatch(inner);
            if (node) tbl.insert(key, std::move(*node));
        }
    });
}

template <class Variant>
node_ptr write_variant(const Variant& v) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    return std::visit([&](auto const& alt) -> node_ptr {
        using A = std::remove_cvref_t<decltype(alt)>;
        if constexpr (tagging == variant_tagging::external) {
            auto tbl = std::make_unique<::toml::table>();
            auto node = detail::toml::write_dispatch(alt);
            if (node) tbl->insert(std::string{rflcpp::type_name_of<A>()}, std::move(*node));
            return node_ptr(tbl.release());
        } else if constexpr (tagging == variant_tagging::internal) {
            auto tbl = std::make_unique<::toml::table>();
            tbl->insert(std::string{variant_policy<Variant>::tag_field}, std::string{rflcpp::type_name_of<A>()});
            if constexpr (reflectable_class<A>) write_members(*tbl, alt);
            else {
                auto node = detail::toml::write_dispatch(alt);
                if (node) tbl->insert("value", std::move(*node));
            }
            return node_ptr(tbl.release());
        } else if constexpr (tagging == variant_tagging::adjacent) {
            auto tbl = std::make_unique<::toml::table>();
            tbl->insert(std::string{variant_policy<Variant>::tag_field}, std::string{rflcpp::type_name_of<A>()});
            auto node = detail::toml::write_dispatch(alt);
            if (node) tbl->insert(std::string{variant_policy<Variant>::content_field}, std::move(*node));
            return node_ptr(tbl.release());
        } else { // untagged
            return detail::toml::write_dispatch(alt);
        }
    }, v);
}

template <class T>
node_ptr write_dispatch(const T& v) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { toml_codec<U>::write(v); }) {
        return toml_codec<U>::write(v);
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
            auto arr = std::make_unique<::toml::array>();
            auto names = rflcpp::enum_flag_names(v);
            for (auto const& n : names) arr->push_back(std::string{n});
            return node_ptr(arr.release());
        } else {
            return node_ptr(new ::toml::value<std::string>(std::string{rflcpp::enum_name(v)}));
        }
    }
    else if constexpr (boolean_like<U>) {
        return node_ptr(new ::toml::value<bool>(static_cast<bool>(v)));
    }
    else if constexpr (numeric_like<U>) {
        if constexpr (std::is_floating_point_v<U>)
            return node_ptr(new ::toml::value<double>(static_cast<double>(v)));
        else
            return node_ptr(new ::toml::value<int64_t>(static_cast<int64_t>(v)));
    }
    else if constexpr (string_like<U>) {
        return node_ptr(new ::toml::value<std::string>(std::string{v}));
    }
    else if constexpr (map_like<U>) {
        auto tbl = std::make_unique<::toml::table>();
        for (auto const& [k, val] : v) {
            auto node = write_dispatch(val);
            if (node) tbl->insert(std::string{k}, std::move(*node));
        }
        return node_ptr(tbl.release());
    }
    else if constexpr (sequence_like<U>) {
        auto arr = std::make_unique<::toml::array>();
        for (auto const& e : v) {
            auto node = write_dispatch(e);
            if (node) arr->push_back(std::move(*node));
        }
        return node_ptr(arr.release());
    }
    else if constexpr (reflectable_class<U>) {
        auto tbl = std::make_unique<::toml::table>();
        write_members(*tbl, v);
        return node_ptr(tbl.release());
    }
    return nullptr;
}

template <class T>
result<T> read_dispatch(const ::toml::node& node, std::string_view path);

template <class Owner, class FieldType, class Out>
void read_member(const ::toml::table& parent, std::string_view member_name,
                 Out& out_field, std::string_view path,
                 std::optional<error>& failure)
{
    using F = std::remove_cvref_t<FieldType>;
    if (failure || skip_on_read_v<F>()) return;

    std::string canonical = effective_key<Owner, F>(member_name);
    const ::toml::node* found = parent.get(canonical);
    if (!found) {
        constexpr auto ap = aliases_pair<F>();
        for (std::size_t i = 0; i < ap.second; ++i) {
            std::string alias{ap.first[i]};
            if ((found = parent.get(alias))) break;
        }
    }

    auto& inner = unwrap_ref<F>(out_field);
    using InnerT = std::remove_cvref_t<decltype(inner)>;

    if (!found) {
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

    auto res = read_dispatch<InnerT>(*found, path);
    if (!res) failure = res.error();
    else      inner = std::move(*res);
}

template <class T>
void read_members(const ::toml::table& node, T& obj, std::string_view path, std::optional<error>& failure) {
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
                    if (auto found = node.get(key)) {
                        auto res = read_dispatch<B>(*found, path);
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
            if constexpr (reflectable_class<std::remove_cvref_t<decltype(inner)>>) {
                read_members(node, inner, path, failure);
            }
        } else {
            read_member<U, FT>(node, member_name, field_ref, path, failure);
        }
    });
}

template <class Variant>
result<Variant> read_variant(const ::toml::node& node, std::string_view path) {
    constexpr auto tagging = variant_policy<Variant>::tagging;

    if constexpr (tagging == variant_tagging::external) {
        auto tbl = node.as_table();
        if (!tbl || tbl->size() != 1)
            return fail({error_kind::type_mismatch, "expected table with single key for external variant", std::string{path}});
        
        std::string key;
        const ::toml::node* val = nullptr;
        for (auto const& [k, v] : *tbl) { key = std::string{k.str()}; val = &v; break; }

        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (key == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(*val, path);
                    if (res) found = Variant(std::move(*res));
                    else     found = fail(res.error());
                }
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "unknown variant tag: " + key, std::string{path}});
    }
    else if constexpr (tagging == variant_tagging::internal) {
        auto tbl = node.as_table();
        if (!tbl) return fail({error_kind::type_mismatch, "expected table for internal variant", std::string{path}});
        std::string tag_field{variant_policy<Variant>::tag_field};
        auto found_tag = tbl->get(tag_field);
        if (!found_tag || !found_tag->is_string())
            return fail({error_kind::missing_field, "missing tag field '" + tag_field + "'", std::string{path}});
        std::string tag = std::string{found_tag->as_string()->get()};
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
        auto tbl = node.as_table();
        if (!tbl) return fail({error_kind::type_mismatch, "expected table for adjacent variant", std::string{path}});
        std::string tag_field{variant_policy<Variant>::tag_field};
        std::string content_field{variant_policy<Variant>::content_field};
        auto it_tag = tbl->get(tag_field);
        auto it_val = tbl->get(content_field);
        if (!it_tag || !it_tag->is_string())
            return fail({error_kind::missing_field, "missing tag field '" + tag_field + "'", std::string{path}});
        if (!it_val)
            return fail({error_kind::missing_field, "missing content field '" + content_field + "'", std::string{path}});
        std::string tag = std::string{it_tag->as_string()->get()};
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
                auto res = read_dispatch<Alt>(node, path);
                if (res) found = Variant(std::move(*res));
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "no alternative matched untagged variant", std::string{path}});
    }
}

template <class T>
result<T> read_dispatch(const ::toml::node& node, std::string_view path) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { toml_codec<U>::read(node, path); }) {
        return toml_codec<U>::read(node, path);
    }
    else if constexpr (requires { typename U::value_type;
                                  requires std::same_as<U,
                                      validated<typename U::value_type>>; }) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        return U::make(std::move(*inner)).map_error([&](auto e) {
            return error{error_kind::validation_failed, e, std::string{path}};
        });
    }
    else if constexpr (is_wrapped_v<U>) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (optional_like<U>) {
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (variant_like<U>) {
        return read_variant<U>(node, path);
    }
    else if constexpr (boolean_like<U>) {
        if (auto b = node.as_boolean()) return b->get();
        return fail({error_kind::type_mismatch, "expected boolean", std::string{path}});
    }
    else if constexpr (numeric_like<U>) {
        if constexpr (std::is_integral_v<U>) {
            if (auto i = node.as_integer()) return static_cast<U>(i->get());
        } else {
            if (auto d = node.as_floating_point()) return static_cast<U>(d->get());
            if (auto i = node.as_integer()) return static_cast<U>(i->get());
        }
        return fail({error_kind::type_mismatch, "expected number", std::string{path}});
    }
    else if constexpr (string_like<U>) {
        if (auto s = node.as_string()) return std::string{s->get()};
        return fail({error_kind::type_mismatch, "expected string", std::string{path}});
    }
    else if constexpr (enum_like<U>) {
        if (auto s = node.as_string()) {
            auto val = rflcpp::enum_value<U>(s->get());
            if (val) return *val;
        }
        return fail({error_kind::type_mismatch, "expected valid enum string", std::string{path}});
    }
    else if constexpr (map_like<U>) {
        if (auto tbl = node.as_table()) {
            U out;
            for (auto const& [k, v] : *tbl) {
                auto res = read_dispatch<typename U::mapped_type>(v, path);
                if (!res) return fail(res.error());
                out[std::string{k.str()}] = std::move(*res);
            }
            return out;
        }
        return fail({error_kind::type_mismatch, "expected table for map", std::string{path}});
    }
    else if constexpr (sequence_like<U>) {
        if (auto arr = node.as_array()) {
            U out;
            for (auto const& e : *arr) {
                auto res = read_dispatch<typename U::value_type>(e, path);
                if (!res) return fail(res.error());
                out.push_back(std::move(*res));
            }
            return out;
        }
        return fail({error_kind::type_mismatch, "expected array for sequence", std::string{path}});
    }
    else if constexpr (reflectable_class<U>) {
        if (auto tbl = node.as_table()) {
            U val;
            std::optional<error> failure;
            read_members(*tbl, val, path, failure);
            if (failure) return fail(*failure);
            return val;
        }
        return fail({error_kind::type_mismatch, "expected table", std::string{path}});
    }
    return fail({error_kind::type_mismatch, "unsupported type", std::string{path}});
}

} // namespace detail::toml

template <class T>
[[nodiscard]] std::string to_toml(const T& value, toml_options opts = {}) {
    ::toml::table tbl;
    if constexpr (reflectable_class<T>) {
        detail::toml::write_members(tbl, value);
    } else {
        auto node = detail::toml::write_dispatch(value);
        if (node) tbl.insert("value", std::move(*node));
    }
    std::stringstream ss;
    ss << tbl;
    return ss.str();
}

template <class T>
[[nodiscard]] result<T> from_toml(std::string_view toml_str) {
    try {
        ::toml::table tbl = ::toml::parse(toml_str);
        if constexpr (reflectable_class<T>) {
            return detail::toml::read_dispatch<T>(tbl, "$");
        } else {
            if (auto found = tbl.get("value")) {
                return detail::toml::read_dispatch<T>(*found, "$");
            }
            return detail::toml::read_dispatch<T>(tbl, "$");
        }
    } catch (const ::toml::parse_error& e) {
        return fail(error{error_kind::parse_error, std::string{e.description()}, "$"});
    }
}

} // namespace rflcpp

#endif // RFLCPP_ENABLE_TOML
