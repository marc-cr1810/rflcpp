// rflcpp/registry.hpp - Compile-time type registries and type-erased dynamic deserialization.
// SPDX-License-Identifier: MIT

#pragma once

#include <string_view>
#include <string>
#include <type_traits>
#include <optional>
#include <utility>
#include <algorithm>
#include <tuple>

#include <rflcpp/any.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/attributes.hpp>

// Forward declarations of JSON type if needed for type alias.
#ifdef RFLCPP_ENABLE_JSON
#include <nlohmann/json_fwd.hpp>
namespace rflcpp {
using njson = nlohmann::json;
}
#endif

namespace rflcpp {

template <class... Types>
struct type_registry {
    using types = std::tuple<Types...>;
    
#ifdef RFLCPP_ENABLE_JSON
    template <class JsonType = njson>
    static rflcpp::result<rflcpp::any> deserialize_json(std::string_view tag, const JsonType& j, std::string_view path = "$");
#endif

#ifdef RFLCPP_ENABLE_YAML
    template <class YamlNodeType>
    static rflcpp::result<rflcpp::any> deserialize_yaml(std::string_view tag, const YamlNodeType& node, std::string_view path = "$");
#endif

#ifdef RFLCPP_ENABLE_TOML
    template <class TomlNodeType>
    static rflcpp::result<rflcpp::any> deserialize_toml(std::string_view tag, const TomlNodeType& node, std::string_view path = "$");
#endif

#ifdef RFLCPP_ENABLE_XML
    template <class XmlNodeType>
    static rflcpp::result<rflcpp::any> deserialize_xml(std::string_view tag, const XmlNodeType& node, std::string_view path = "$");
#endif

#ifdef RFLCPP_ENABLE_MSGPACK
    template <class MsgPackNodeType>
    static rflcpp::result<rflcpp::any> deserialize_msgpack(std::string_view tag, MsgPackNodeType node, std::string_view path = "$");
#endif
};

template <class Registry, fixed_string TagField = "type">
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
