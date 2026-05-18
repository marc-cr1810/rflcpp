// rflcpp/schema.hpp - JSON Schema generation from reflectable types.
// SPDX-License-Identifier: MIT
#pragma once

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

struct size_counter {
    std::size_t size = 0;

    constexpr void write(char c) { size++; }
    constexpr void write(std::string_view s) { size += s.size(); }

    constexpr void write_escaped(std::string_view s) {
        for (char c : s) {
            switch (c) {
                case '"':  size += 2; break;
                case '\\': size += 2; break;
                case '\n': size += 2; break;
                case '\t': size += 2; break;
                default:   size++;
            }
        }
    }

    constexpr void write_int(long long val) {
        if (val == 0) { size++; return; }
        if (val < 0) { size++; val = -val; }
        while (val > 0) { size++; val /= 10; }
    }

    constexpr void write_double(double val) {
        if (val < 0) { size++; val = -val; }
        auto int_part = static_cast<long long>(val);
        write_int(int_part);
        double frac = val - int_part;
        if (frac > 0.000001) {
            size++; // '.'
            for (int i = 0; i < 6; ++i) {
                frac *= 10;
                int digit = static_cast<int>(frac);
                size++;
                frac -= digit;
                if (frac < 0.000001) break;
            }
        }
    }
};

template <std::size_t Capacity>
struct constexpr_writer {
    char data[Capacity + 1]{};
    std::size_t size = 0;

    constexpr void write(char c) {
        if (size < Capacity) data[size++] = c;
    }

    constexpr void write(std::string_view s) {
        for (char c : s) write(c);
    }

    constexpr void write_escaped(std::string_view s) {
        for (char c : s) {
            switch (c) {
                case '"':  write("\\\""); break;
                case '\\': write("\\\\"); break;
                case '\n': write("\\n");  break;
                case '\t': write("\\t");  break;
                default:   write(c);
            }
        }
    }

    constexpr void write_int(long long val) {
        if (val == 0) { write('0'); return; }
        if (val < 0) { write('-'); val = -val; }
        char buf[32];
        int pos = 32;
        while (val > 0) {
            buf[--pos] = char('0' + (val % 10));
            val /= 10;
        }
        write(std::string_view{buf + pos, std::size_t(32 - pos)});
    }

    constexpr void write_double(double val) {
        if (val < 0) { write('-'); val = -val; }
        auto int_part = static_cast<long long>(val);
        write_int(int_part);
        double frac = val - int_part;
        if (frac > 0.000001) {
            write('.');
            for (int i = 0; i < 6; ++i) {
                frac *= 10;
                int digit = static_cast<int>(frac);
                write(char('0' + digit));
                frac -= digit;
                if (frac < 0.000001) break;
            }
        }
    }
};

template <class T, class Writer>
constexpr void emit_schema_for_type(Writer& w, bool root = false);

template <class T, class Writer>
constexpr void emit_schema_for_type(Writer& w, bool root) {
    using U = std::remove_cvref_t<T>;

    auto open_obj = [&] { if (!root) w.write('{'); };
    auto close_obj = [&] { if (!root) w.write('}'); };

    if constexpr (is_wrapped_v<U> || is_validated_v<U>) {
        open_obj();
        if constexpr (is_wrapped_v<U>) {
            emit_schema_for_type<unwrap_t<U>>(w, true);
        } else {
            emit_schema_for_type<typename U::value_type>(w, true);
        }

        if constexpr (is_wrapped_v<U>) {
            using attrs_tuple = typename U::attributes;
            constexpr auto desc = []<class... A>(std::tuple<A...>*) consteval {
                return rflcpp::detail::description_of<A...>();
            }(static_cast<attrs_tuple*>(nullptr));
            constexpr auto titl = []<class... A>(std::tuple<A...>*) consteval {
                return rflcpp::detail::title_of<A...>();
            }(static_cast<attrs_tuple*>(nullptr));

            if (!desc.empty()) { w.write(",\"description\":"); w.write('"'); w.write_escaped(desc); w.write('"'); }
            if (!titl.empty()) { w.write(",\"title\":");       w.write('"'); w.write_escaped(titl); w.write('"'); }
        }

        if constexpr (is_validated_v<U>) {
            using traits = validated_traits<U>;
            if constexpr (requires { traits::min_value(); }) {
                if (auto min = traits::min_value()) { w.write(",\"minimum\":"); w.write_double(*min); }
            }
            if constexpr (requires { traits::max_value(); }) {
                if (auto max = traits::max_value()) { w.write(",\"maximum\":"); w.write_double(*max); }
            }
            if constexpr (requires { traits::min_length(); }) {
                if (auto minl = traits::min_length()) { w.write(",\"minLength\":"); w.write_int(*minl); }
            }
            if constexpr (requires { traits::max_length(); }) {
                if (auto maxl = traits::max_length()) { w.write(",\"maxLength\":"); w.write_int(*maxl); }
            }
        }
        close_obj();
    }
    else if constexpr (optional_like<U>) {
        open_obj();
        w.write("\"anyOf\":[");
        emit_schema_for_type<typename U::value_type>(w);
        w.write(",{\"type\":\"null\"}]");
        close_obj();
    }
    else if constexpr (variant_like<U>) {
        open_obj();
        w.write("\"oneOf\":[");
        bool first = true;
        []<class... As>(Writer& w, bool& first, std::variant<As...>*) {
            (void)((first ? (first = false, true) : (w.write(','), true),
                    emit_schema_for_type<As>(w), true), ...);
        }(w, first, static_cast<U*>(nullptr));
        w.write("]");
        close_obj();
    }
    else if constexpr (enum_like<U>) {
        open_obj();
        w.write("\"type\":\"string\",\"enum\":[");
        static constexpr auto entries = enum_entries<U>();
        bool first = true;
        for (auto const& [v, n] : entries) {
            if (!first) w.write(',');
            first = false;
            w.write('"');
            w.write_escaped(n);
            w.write('"');
        }
        w.write("]");
        close_obj();
    }
    else if constexpr (std::is_same_v<U, bool>) {
        open_obj();
        w.write("\"type\":\"boolean\"");
        close_obj();
    }
    else if constexpr (std::is_integral_v<U>) {
        open_obj();
        w.write("\"type\":\"integer\"");
        close_obj();
    }
    else if constexpr (std::is_floating_point_v<U>) {
        open_obj();
        w.write("\"type\":\"number\"");
        close_obj();
    }
    else if constexpr (string_like<U>) {
        open_obj();
        w.write("\"type\":\"string\"");
        close_obj();
    }
    else if constexpr (sequence_like<U>) {
        open_obj();
        w.write("\"type\":\"array\",\"items\":");
        emit_schema_for_type<typename U::value_type>(w);
        close_obj();
    }
    else if constexpr (map_like<U>) {
        open_obj();
        w.write("\"type\":\"object\",\"additionalProperties\":");
        emit_schema_for_type<typename U::mapped_type>(w);
        close_obj();
    }
    else if constexpr (reflectable_class<U>) {
        open_obj();
        w.write("\"type\":\"object\",\"properties\":{");
        bool first = true;
        template_for_each_field<U>([&]<class FT>(std::string_view name) {
            if (!first) w.write(',');
            first = false;

            std::string key = rflcpp::detail::serialization::effective_key<U, FT>(name);
            w.write('"');
            w.write_escaped(key);
            w.write("\":");
            emit_schema_for_type<FT>(w);
        });
        w.write("}");

        bool req_first = true;
        template_for_each_field<U>([&]<class FT>(std::string_view name) {
            if constexpr (!optional_like<unwrap_t<FT>>) {
                if (req_first) {
                    w.write(",\"required\":[");
                    req_first = false;
                } else {
                    w.write(',');
                }
                std::string key = rflcpp::detail::serialization::effective_key<U, FT>(name);
                w.write('"');
                w.write_escaped(key);
                w.write('"');
            }
        });
        if (!req_first) {
            w.write("]");
        }

        close_obj();
    }
    else {
        open_obj();
        w.write("\"type\":\"object\",\"description\":\"unsupported\"");
        close_obj();
    }
}

template <class T, class Writer>
constexpr void emit_schema(Writer& w) {
    w.write("{\"$schema\":\"https://json-schema.org/draft/2020-12/schema\",\"title\":\"");
    w.write_escaped(type_name_of<T>());
    w.write("\",\"$id\":\"rflcpp://");
    w.write(type_name_of<T>());
    w.write("\",");
    emit_schema_for_type<T>(w, true);
    w.write("}");
}

template <class T, std::size_t Size>
struct schema_holder {
    static constexpr auto get_data() {
        constexpr_writer<Size> w;
        emit_schema<T>(w);
        return w;
    }
    static constexpr auto writer = get_data();
};

} // namespace detail::schema

/// Returns a compile-time JSON Schema of `T` as a `std::string_view`.
template <class T>
consteval std::string_view to_json_schema_view() {
    constexpr auto size = []() consteval {
        detail::schema::size_counter w;
        detail::schema::emit_schema<T>(w);
        return w.size;
    }();

    using Holder = detail::schema::schema_holder<T, size>;
    return std::string_view{&Holder::writer.data[0], size};
}

/// Returns a JSON Schema (Draft 2020-12) description of `T`.
template <class T>
[[nodiscard]] std::string to_json_schema() {
    return std::string{to_json_schema_view<T>()};
}

} // namespace rflcpp
