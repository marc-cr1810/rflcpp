// rflcpp/xml.hpp - XML serialization using pugixml.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef RFLCPP_ENABLE_XML

#include <pugixml.hpp>
#include <rflcpp/detail/serialization_common.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

namespace rflcpp {

struct xml_options {
    /// Root element name.
    std::string root = "root";
    /// XML indentation.
    std::string indent = "  ";
};

/// Specialize this to add custom-type support for `to_xml` / `from_xml`.
template <class T, class = void>
struct xml_codec;

namespace detail::xml {

using namespace rflcpp::detail::serialization;

template <class T>
void write_dispatch(pugi::xml_node& node, const T& v, std::string_view name);

template <class T>
void write_members(pugi::xml_node& node, const T& obj) {
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
                    pugi::xml_node child = node.append_child(std::string{type_name_of<B>()}.c_str());
                    write_members(child, base_ref);
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
            node.append_child(key.c_str()).text().set("***");
        } else {
            detail::xml::write_dispatch(node, inner, key);
        }
    });
}

template <class Variant>
void write_variant(pugi::xml_node& node, const Variant& v, std::string_view name) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    std::visit([&](auto const& alt) {
        using A = std::remove_cvref_t<decltype(alt)>;
        if constexpr (tagging == variant_tagging::external) {
            pugi::xml_node child = node.append_child(std::string{name}.c_str());
            pugi::xml_node val_node = child.append_child(std::string{type_name_of<A>()}.c_str());
            if constexpr (reflectable_class<A>) write_members(val_node, alt);
            else {
                // For primitives in external variant, we just set the text of the tag node
                if constexpr (boolean_like<A>) val_node.text().set(alt ? "true" : "false");
                else if constexpr (numeric_like<A>) val_node.text().set(alt);
                else if constexpr (string_like<A>) val_node.text().set(std::string{alt}.c_str());
                else if constexpr (enum_like<A>) val_node.text().set(std::string{rflcpp::enum_name(alt)}.c_str());
                else write_dispatch(val_node, alt, "value"); 
            }
        } else if constexpr (tagging == variant_tagging::internal) {
            pugi::xml_node child = node.append_child(std::string{name}.c_str());
            child.append_attribute(std::string{variant_policy<Variant>::tag_field}.c_str()).set_value(std::string{type_name_of<A>()}.c_str());
            if constexpr (reflectable_class<A>) write_members(child, alt);
            else write_dispatch(child, alt, "value");
        } else if constexpr (tagging == variant_tagging::adjacent) {
            pugi::xml_node child = node.append_child(std::string{name}.c_str());
            child.append_child(std::string{variant_policy<Variant>::tag_field}.c_str()).text().set(std::string{type_name_of<A>()}.c_str());
            pugi::xml_node content = child.append_child(std::string{variant_policy<Variant>::content_field}.c_str());
            if constexpr (reflectable_class<A>) write_members(content, alt);
            else write_dispatch(content, alt, "value");
        } else { // untagged
            if constexpr (reflectable_class<A>) {
                pugi::xml_node child = node.append_child(std::string{name}.c_str());
                write_members(child, alt);
            } else {
                write_dispatch(node, alt, name);
            }
        }
    }, v);
}

template <class T>
void write_dispatch(pugi::xml_node& node, const T& v, std::string_view name) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { xml_codec<U>::write(node, v, name); }) {
        xml_codec<U>::write(node, v, name);
    }
    else if constexpr (requires { v.get(); typename U::value_type;
                                  requires std::same_as<U,
                                      validated<typename U::value_type>>; }) {
        write_dispatch(node, v.get(), name);
    }
    else if constexpr (is_wrapped_v<U>) {
        write_dispatch(node, v.value, name);
    }
    else if constexpr (optional_like<U>) {
        if (v.has_value()) write_dispatch(node, *v, name);
    }
    else if constexpr (variant_like<U>) {
        write_variant(node, v, name);
    }
    else if constexpr (enum_like<U>) {
        if constexpr (rflcpp::enum_flags_policy<U>::is_flags) {
            pugi::xml_node child = node.append_child(std::string{name}.c_str());
            auto names = rflcpp::enum_flag_names(v);
            for (auto const& n : names) child.append_child("flag").text().set(std::string{n}.c_str());
        } else {
            node.append_child(std::string{name}.c_str()).text().set(std::string{rflcpp::enum_name(v)}.c_str());
        }
    }
    else if constexpr (boolean_like<U>) {
        node.append_child(std::string{name}.c_str()).text().set(bool(v) ? "true" : "false");
    }
    else if constexpr (numeric_like<U>) {
        if constexpr (std::is_integral_v<U>)
            node.append_child(std::string{name}.c_str()).text().set((long long)v);
        else
            node.append_child(std::string{name}.c_str()).text().set((double)v);
    }
    else if constexpr (string_like<U>) {
        node.append_child(std::string{name}.c_str()).text().set(std::string{v}.c_str());
    }
    else if constexpr (map_like<U>) {
        pugi::xml_node child = node.append_child(std::string{name}.c_str());
        for (auto const& [k, val] : v) write_dispatch(child, val, k);
    }
    else if constexpr (sequence_like<U>) {
        pugi::xml_node child = node.append_child(std::string{name}.c_str());
        for (auto const& e : v) write_dispatch(child, e, "item");
    }
    else if constexpr (reflectable_class<U>) {
        pugi::xml_node child = node.append_child(std::string{name}.c_str());
        write_members(child, v);
    }
}

template <class T>
result<T> read_dispatch(const pugi::xml_node& node, std::string_view path);

template <class Owner, class FieldType, class Out>
void read_member(const pugi::xml_node& parent, std::string_view member_name,
                 Out& out_field, std::string_view path,
                 std::optional<error>& failure)
{
    using F = std::remove_cvref_t<FieldType>;
    if (failure || skip_on_read_v<F>()) return;

    std::string canonical = effective_key<Owner, F>(member_name);
    pugi::xml_node found = parent.child(canonical.c_str());
    if (found.empty()) {
        constexpr auto ap = aliases_pair<F>();
        for (std::size_t i = 0; i < ap.second; ++i) {
            std::string alias{ap.first[i]};
            found = parent.child(alias.c_str());
            if (!found.empty()) break;
        }
    }

    auto& inner = unwrap_ref<F>(out_field);
    using InnerT = std::remove_cvref_t<decltype(inner)>;

    if (found.empty()) {
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
void read_members(const pugi::xml_node& node, T& obj, std::string_view path, std::optional<error>& failure) {
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
                    pugi::xml_node child = node.child(key.c_str());
                    if (!child.empty()) {
                        auto res = read_dispatch<B>(child, path);
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
result<Variant> read_variant(const pugi::xml_node& node, std::string_view path) {
    constexpr auto tagging = variant_policy<Variant>::tagging;

    if constexpr (tagging == variant_tagging::external) {
        pugi::xml_node val_node = node.first_child();
        if (val_node.empty()) return fail({error_kind::missing_field, "missing variant tag", std::string{path}});
        std::string key = val_node.name();
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (key == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(val_node, path);
                    if (res) found = Variant(std::move(*res));
                    else     found = fail(res.error());
                }
            }(), ...);
        }(std::make_index_sequence<std::variant_size_v<Variant>>{});
        if (found) return *found;
        return fail({error_kind::type_mismatch, "unknown variant tag: " + key, std::string{path}});
    }
    else if constexpr (tagging == variant_tagging::internal) {
        std::string tag_field{variant_policy<Variant>::tag_field};
        pugi::xml_attribute attr = node.attribute(tag_field.c_str());
        if (attr.empty()) return fail({error_kind::missing_field, "missing tag attribute '" + tag_field + "'", std::string{path}});
        std::string tag = attr.value();
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
        std::string tag_field{variant_policy<Variant>::tag_field};
        std::string content_field{variant_policy<Variant>::content_field};
        pugi::xml_node tag_node = node.child(tag_field.c_str());
        pugi::xml_node val_node = node.child(content_field.c_str());
        if (tag_node.empty()) return fail({error_kind::missing_field, "missing tag node '" + tag_field + "'", std::string{path}});
        if (val_node.empty()) return fail({error_kind::missing_field, "missing content node '" + content_field + "'", std::string{path}});
        std::string tag = tag_node.text().as_string();
        std::optional<result<Variant>> found;
        [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            ([&] {
                if (found) return;
                using Alt = std::variant_alternative_t<Is, Variant>;
                if (tag == type_name_of<Alt>()) {
                    auto res = read_dispatch<Alt>(val_node, path);
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
result<T> read_dispatch(const pugi::xml_node& node, std::string_view path) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { xml_codec<U>::read(node); }) {
        return xml_codec<U>::read(node);
    }
    else if constexpr (requires { typename U::value_type;
                                  requires std::same_as<U,
                                      validated<typename U::value_type>>; }) {
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
        if (node.empty()) return U{std::nullopt};
        using V = typename U::value_type;
        auto inner = read_dispatch<V>(node, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (variant_like<U>) {
        return read_variant<U>(node, path);
    }
    else if constexpr (boolean_like<U>) {
        std::string s = node.text().as_string();
        if (s == "true" || s == "1") return true;
        if (s == "false" || s == "0") return false;
        return fail({error_kind::type_mismatch, "expected boolean", std::string{path}});
    }
    else if constexpr (numeric_like<U>) {
        if constexpr (std::is_integral_v<U>) return static_cast<U>(node.text().as_llong());
        else return static_cast<U>(node.text().as_double());
    }
    else if constexpr (string_like<U>) {
        return std::string{node.text().as_string()};
    }
    else if constexpr (enum_like<U>) {
        std::string s = node.text().as_string();
        auto val = rflcpp::enum_value<U>(s);
        if (val) return *val;
        return fail({error_kind::type_mismatch, "invalid enum", std::string{path}});
    }
    else if constexpr (map_like<U>) {
        U out;
        for (pugi::xml_node child : node.children()) {
            auto val = read_dispatch<typename U::mapped_type>(child, std::string{path} + "." + child.name());
            if (!val) return fail(val.error());
            out[child.name()] = std::move(*val);
        }
        return out;
    }
    else if constexpr (sequence_like<U>) {
        U out;
        int i = 0;
        for (pugi::xml_node child : node.children()) {
            auto val = read_dispatch<typename U::value_type>(child, std::string{path} + "[" + std::to_string(i++) + "]");
            if (!val) return fail(val.error());
            if constexpr (requires { out.push_back(std::move(*val)); })
                out.push_back(std::move(*val));
            else if constexpr (requires { out.insert(std::move(*val)); })
                out.insert(std::move(*val));
        }
        return out;
    }
    else if constexpr (reflectable_class<U>) {
        U val;
        std::optional<error> failure;
        read_members(node, val, path, failure);
        if (failure) return fail(*failure);
        return val;
    }
    return fail({error_kind::type_mismatch, "unsupported type", std::string{path}});
}

} // namespace detail::xml

template <class T>
[[nodiscard]] std::string to_xml(const T& value, xml_options opts = {}) {
    pugi::xml_document doc;
    pugi::xml_node root = doc.append_child(opts.root.c_str());
    if constexpr (reflectable_class<T>) {
        detail::xml::write_members(root, value);
    } else if constexpr (map_like<T>) {
        for (auto const& [k, val] : value) detail::xml::write_dispatch(root, val, k);
    } else if constexpr (sequence_like<T>) {
        for (auto const& e : value) detail::xml::write_dispatch(root, e, "item");
    } else {
        // For primitives, write directly into the root node's text
        using U = std::remove_cvref_t<T>;
        if constexpr (boolean_like<U>) root.text().set(value ? "true" : "false");
        else if constexpr (numeric_like<U>) root.text().set(value);
        else if constexpr (string_like<U>) root.text().set(std::string{value}.c_str());
        else if constexpr (enum_like<U>) root.text().set(std::string{rflcpp::enum_name(value)}.c_str());
        else detail::xml::write_dispatch(root, value, "value");
    }
    std::stringstream ss;
    doc.save(ss, opts.indent.c_str());
    return ss.str();
}

template <class T>
[[nodiscard]] result<T> from_xml(std::string_view xml_str) {
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_string(std::string{xml_str}.c_str());
    if (!res) return fail(error{error_kind::parse_error, res.description(), "$"});
    return detail::xml::read_dispatch<T>(doc.first_child(), "$");
}

} // namespace rflcpp

#endif // RFLCPP_ENABLE_XML
