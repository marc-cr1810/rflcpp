// rflcpp/registry.hpp - Compile-time type registries and type-erased dynamic deserialization.
// SPDX-License-Identifier: MIT

#pragma once

#include <string_view>
#include <string>
#include <type_traits>
#include <optional>
#include <utility>
#include <algorithm>

#include <rflcpp/any.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>

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

template <class... Types>
struct type_registry {
#ifdef RFLCPP_ENABLE_JSON
    static rflcpp::result<rflcpp::any> deserialize_json(std::string_view tag, const njson& j, std::string_view path = "$") {
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
#endif

#ifdef RFLCPP_ENABLE_YAML
    static rflcpp::result<rflcpp::any> deserialize_yaml(std::string_view tag, const YAML::Node& node, std::string_view path = "$") {
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
#endif
};

// Compile-time string NTTP helper for TagField
template <std::size_t N>
struct fixed_string_registry {
    char data[N]{};
    consteval fixed_string_registry(const char (&s)[N]) {
        std::copy_n(s, N, data);
    }
    [[nodiscard]] constexpr std::string_view view() const noexcept {
        return {data, N > 0 ? N - 1 : 0};
    }
};

template <class Registry, fixed_string_registry TagField = "type">
struct registered_any {
    rflcpp::any value;
    
    constexpr registered_any() = default;
    constexpr explicit registered_any(rflcpp::any v) : value(std::move(v)) {}
    
    template <class T>
    constexpr registered_any(T&& v) : value(std::forward<T>(v)) {}
    
    constexpr operator rflcpp::any&()             noexcept { return value; }
    constexpr operator const rflcpp::any&() const noexcept { return value; }
    
    constexpr rflcpp::any&       operator*()       noexcept { return value; }
    constexpr const rflcpp::any& operator*() const noexcept { return value; }
    constexpr rflcpp::any*       operator->()       noexcept { return &value; }
    constexpr const rflcpp::any* operator->() const noexcept { return &value; }
};

} // namespace rflcpp

#ifdef RFLCPP_ENABLE_JSON
namespace rflcpp {

template <class Registry, fixed_string_registry TagField>
struct json_codec<registered_any<Registry, TagField>> {
    static njson write(const registered_any<Registry, TagField>& ra) {
        if (ra.value.empty()) return nullptr;
        njson j = ra.value.to_json();
        if (j.is_object()) {
            j[std::string{TagField.view()}] = std::string{ra.value.type_name()};
        }
        return j;
    }
    
    static result<registered_any<Registry, TagField>> read(const njson& j, std::string_view path) {
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

} // namespace rflcpp
#endif

#ifdef RFLCPP_ENABLE_YAML
namespace rflcpp {

template <class Registry, fixed_string_registry TagField>
struct yaml_codec<registered_any<Registry, TagField>> {
    static YAML::Node write(const registered_any<Registry, TagField>& ra) {
        if (ra.value.empty()) return YAML::Node(YAML::NodeType::Null);
        njson j = rflcpp::detail::json::write_dispatch(ra.value);
        if (j.is_object()) {
            j[std::string{TagField.view()}] = std::string{ra.value.type_name()};
        }
        
        auto convert = [](const njson& j_val, auto& self) -> YAML::Node {
            if (j_val.is_null()) return YAML::Node(YAML::NodeType::Null);
            if (j_val.is_boolean()) return YAML::Node(j_val.get<bool>());
            if (j_val.is_number()) return YAML::Node(j_val.get<double>());
            if (j_val.is_string()) return YAML::Node(j_val.get<std::string>());
            if (j_val.is_array()) {
                YAML::Node node;
                for (const auto& item : j_val) node.push_back(self(item, self));
                return node;
            }
            if (j_val.is_object()) {
                YAML::Node node;
                for (auto it = j_val.begin(); it != j_val.end(); ++it) {
                    node[it.key()] = self(it.value(), self);
                }
                return node;
            }
            return YAML::Node(YAML::NodeType::Null);
        };
        return convert(j, convert);
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

} // namespace rflcpp
#endif
