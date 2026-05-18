// rflcpp/schema.hpp - JSON Schema generation from reflectable types.
// SPDX-License-Identifier: MIT
#pragma once

#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include <rflcpp/attributes.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/enum_meta.hpp>
#include <rflcpp/policy.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/validation.hpp>
#include <rflcpp/detail/serialization_common.hpp>

namespace rflcpp {

namespace detail::schema {

inline void emit_string(std::ostringstream& os, std::string_view s) {
    os << '"';
    for (char c : s) {
        switch (c) {
            case '"':  os << "\\\""; break;
            case '\\': os << "\\\\"; break;
            case '\n': os << "\\n";  break;
            case '\t': os << "\\t";  break;
            default:   os << c;
        }
    }
    os << '"';
}

template <class T> void emit_schema(std::ostringstream& os);

template <class T>
void emit_schema_for_type(std::ostringstream& os) {
    using U = std::remove_cvref_t<T>;

    if constexpr (is_wrapped_v<U> || is_validated_v<U>) {
        std::ostringstream inner;
        if constexpr (is_wrapped_v<U>) emit_schema_for_type<unwrap_t<U>>(inner);
        else emit_schema_for_type<typename U::value_type>(inner);

        std::string body = inner.str();
        if (body.starts_with('{') && body.ends_with('}'))
            body = body.substr(1, body.size() - 2);

        os << '{' << body;

        if constexpr (is_wrapped_v<U>) {
            using attrs_tuple = typename U::attributes;
            constexpr auto desc = []<class... A>(std::tuple<A...>*) {
                return rflcpp::detail::description_of<A...>();
            }(static_cast<attrs_tuple*>(nullptr));
            constexpr auto titl = []<class... A>(std::tuple<A...>*) {
                return rflcpp::detail::title_of<A...>();
            }(static_cast<attrs_tuple*>(nullptr));

            if (!desc.empty()) { os << ",\"description\":"; emit_string(os, desc); }
            if (!titl.empty()) { os << ",\"title\":";       emit_string(os, titl); }
        }

        if constexpr (is_validated_v<U>) {
            using traits = validated_traits<U>;
            if (auto min = traits::min_value())  os << ",\"minimum\":" << *min;
            if (auto max = traits::max_value())  os << ",\"maximum\":" << *max;
            if (auto minl = traits::min_length()) os << ",\"minLength\":" << *minl;
            if (auto maxl = traits::max_length()) os << ",\"maxLength\":" << *maxl;
        }
        os << '}';
    }
    else if constexpr (optional_like<U>) {
        os << "{\"anyOf\":[";
        emit_schema_for_type<typename U::value_type>(os);
        os << ",{\"type\":\"null\"}]}";
    }
    else if constexpr (variant_like<U>) {
        os << "{\"oneOf\":[";
        bool first = true;
        []<class... As>(std::ostringstream& os, bool& first, std::variant<As...>*) {
            (void)((first ? (first = false, true) : (os << ',', true),
                    emit_schema_for_type<As>(os), true), ...);
        }(os, first, static_cast<U*>(nullptr));
        os << "]}";
    }
    else if constexpr (enum_like<U>) {
        static constexpr auto entries = enum_entries<U>();
        os << "{\"type\":\"string\",\"enum\":[";
        bool first = true;
        for (auto const& [v, n] : entries) {
            if (!first) os << ',';
            first = false;
            emit_string(os, n);
        }
        os << "]}";
    }
    else if constexpr (std::is_same_v<U, bool>)     os << "{\"type\":\"boolean\"}";
    else if constexpr (std::is_integral_v<U>)       os << "{\"type\":\"integer\"}";
    else if constexpr (std::is_floating_point_v<U>) os << "{\"type\":\"number\"}";
    else if constexpr (string_like<U>)              os << "{\"type\":\"string\"}";
    else if constexpr (sequence_like<U>) {
        os << "{\"type\":\"array\",\"items\":";
        emit_schema_for_type<typename U::value_type>(os);
        os << "}";
    }
    else if constexpr (map_like<U>) {
        os << "{\"type\":\"object\",\"additionalProperties\":";
        emit_schema_for_type<typename U::mapped_type>(os);
        os << "}";
    }
    else if constexpr (reflectable_class<U>) {
        os << "{\"type\":\"object\",\"properties\":{";
        bool first = true;
        std::vector<std::string> required;
        template_for_each_field<U>([&]<class FT>(std::string_view name) {
            if (!first) os << ',';
            first = false;

            std::string key = rflcpp::detail::serialization::effective_key<U, FT>(name);

            emit_string(os, key);
            os << ':';
            emit_schema_for_type<FT>(os);

            if constexpr (!optional_like<unwrap_t<FT>>) required.push_back(key);
        });
        os << "}";
        if (!required.empty()) {
            os << ",\"required\":[";
            bool first2 = true;
            for (auto const& r : required) {
                if (!first2) os << ',';
                first2 = false;
                emit_string(os, r);
            }
            os << "]";
        }
        os << "}";
    } else {
        os << "{\"type\":\"object\",\"description\":\"unsupported\"}";
    }
}

template <class T>
void emit_schema(std::ostringstream& os) {
    os << "{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\","
          "\"title\":";
    emit_string(os, type_name_of<T>());
    os << ",\"$id\":\"rflcpp://" << type_name_of<T>() << "\",";

    // Splice the inner schema into this envelope by stripping its outer braces.
    std::ostringstream inner;
    emit_schema_for_type<T>(inner);
    std::string body = inner.str();
    if (!body.empty() && body.front() == '{' && body.back() == '}')
        body = body.substr(1, body.size() - 2);
    os << body << "}";
}

} // namespace detail::schema

/// Returns a JSON Schema (Draft 2020-12) description of `T`.
template <class T>
[[nodiscard]] std::string to_json_schema() {
    std::ostringstream os;
    detail::schema::emit_schema<T>(os);
    return os.str();
}

} // namespace rflcpp
