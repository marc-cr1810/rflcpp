// rflcpp/json_direct.hpp - Zero-copy direct-to-buffer JSON serializer.
// SPDX-License-Identifier: MIT

#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <variant>
#include <vector>

#include <rflcpp/attributes.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/policy.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/detail/serialization_common.hpp>

namespace rflcpp::json::direct {

struct string_writer {
    std::string& str;

    constexpr void push_back(char c) {
        str.push_back(c);
    }

    constexpr void append(std::string_view s) {
        str.append(s);
    }
};

struct span_writer {
    std::span<char> buffer;
    size_t cursor = 0;
    bool overflowed = false;

    constexpr void push_back(char c) {
        if (cursor < buffer.size()) {
            buffer[cursor++] = c;
        } else {
            overflowed = true;
        }
    }

    constexpr void append(std::string_view s) {
        if (cursor + s.size() <= buffer.size()) {
            std::copy(s.begin(), s.end(), buffer.data() + cursor);
            cursor += s.size();
        } else {
            overflowed = true;
            size_t count = (buffer.size() > cursor) ? (buffer.size() - cursor) : 0;
            if (count > 0) {
                std::copy(s.begin(), s.begin() + count, buffer.data() + cursor);
                cursor = buffer.size();
            }
        }
    }
};

using namespace rflcpp::detail::serialization;

template <class Writer, class T>
void write_dispatch(Writer& writer, const T& value);

template <class Writer, class T>
void write_members_impl(Writer& writer, const T& obj, bool& first);

template <class Writer, class T>
void write_members(Writer& writer, const T& obj);

template <class Writer, class Variant>
void write_variant(Writer& writer, const Variant& v);

template <class Writer>
void write_escaped_string(Writer& writer, std::string_view s) {
    writer.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"':  writer.append(std::string_view{"\\\"", 2}); break;
            case '\\': writer.append(std::string_view{"\\\\", 2}); break;
            case '\n': writer.append(std::string_view{"\\n", 2}); break;
            case '\r': writer.append(std::string_view{"\\r", 2}); break;
            case '\t': writer.append(std::string_view{"\\t", 2}); break;
            case '\b': writer.append(std::string_view{"\\b", 2}); break;
            case '\f': writer.append(std::string_view{"\\f", 2}); break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    writer.append(std::string_view{"\\u00", 4});
                    static constexpr char hex_chars[] = "0123456789abcdef";
                    char hex[2] = {
                        hex_chars[(static_cast<unsigned char>(c) >> 4) & 0x0F],
                        hex_chars[static_cast<unsigned char>(c) & 0x0F]
                    };
                    writer.append(std::string_view{hex, 2});
                } else {
                    writer.push_back(c);
                }
                break;
        }
    }
    writer.push_back('"');
}

template <class Writer, class T>
void write_number(Writer& writer, T val) {
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + 64, val);
    if (ec == std::errc{}) {
        writer.append(std::string_view{buf, static_cast<size_t>(ptr - buf)});
    }
}

template <class Writer, class T>
void write_dispatch(Writer& writer, const T& value) {
    using U = std::remove_cvref_t<T>;

    if constexpr (is_wrapped_v<U>) {
        write_dispatch(writer, value.value);
    }
    else if constexpr (optional_like<U>) {
        if (value.has_value()) {
            write_dispatch(writer, *value);
        } else {
            writer.append(std::string_view{"null", 4});
        }
    }
    else if constexpr (variant_like<U>) {
        write_variant(writer, value);
    }
    else if constexpr (std::is_enum_v<U>) {
        if constexpr (rflcpp::enum_flags_policy<U>::is_flags) {
            auto names = rflcpp::enum_flag_names(value);
            writer.push_back('[');
            bool first = true;
            for (auto const& n : names) {
                if (!first) writer.push_back(',');
                first = false;
                write_escaped_string(writer, n);
            }
            writer.push_back(']');
        } else {
            write_escaped_string(writer, rflcpp::enum_name(value));
        }
    }
    else if constexpr (boolean_like<U>) {
        if (value) {
            writer.append(std::string_view{"true", 4});
        } else {
            writer.append(std::string_view{"false", 5});
        }
    }
    else if constexpr (numeric_like<U>) {
        write_number(writer, value);
    }
    else if constexpr (string_like<U>) {
        write_escaped_string(writer, std::string_view{value});
    }
    else if constexpr (map_like<U>) {
        writer.push_back('{');
        bool first = true;
        for (auto const& [k, val] : value) {
            if (!first) writer.push_back(',');
            first = false;
            write_escaped_string(writer, k);
            writer.push_back(':');
            write_dispatch(writer, val);
        }
        writer.push_back('}');
    }
    else if constexpr (sequence_like<U>) {
        writer.push_back('[');
        bool first = true;
        for (auto const& e : value) {
            if (!first) writer.push_back(',');
            first = false;
            write_dispatch(writer, e);
        }
        writer.push_back(']');
    }
    else if constexpr (reflectable_class<U>) {
        writer.push_back('{');
        write_members(writer, value);
        writer.push_back('}');
    }
}

template <class Writer, class T>
void write_members_impl(Writer& writer, const T& obj, bool& first) {
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
                    write_members_impl(writer, base_ref, first);
                } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                    if (!first) writer.push_back(',');
                    first = false;
                    write_escaped_string(writer, type_name_of<B>());
                    writer.push_back(':');
                    writer.push_back('{');
                    bool base_first = true;
                    write_members_impl(writer, base_ref, base_first);
                    writer.push_back('}');
                }
            }(), ...);
        }(std::make_index_sequence<B_COUNT>{});
    }

    for_each_field(obj, [&](std::string_view member_name, auto const& field_ref) {
        using FT = std::remove_cvref_t<decltype(field_ref)>;

        if constexpr (skip_on_write_v<FT>()) return;

        if constexpr (flatten_v<FT>()) {
            auto const& inner = unwrap_ref<FT>(field_ref);
            if constexpr (reflectable_class<std::remove_cvref_t<decltype(inner)>>) {
                write_members_impl(writer, inner, first);
            }
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

        if (!first) writer.push_back(',');
        first = false;

        std::string key = effective_key<U, FT>(member_name);
        write_escaped_string(writer, key);
        writer.push_back(':');

        if constexpr (sensitive_v<FT>()) {
            writer.append(std::string_view{"\"***\"", 5});
        } else {
            write_dispatch(writer, inner);
        }
    });
}

template <class Writer, class T>
void write_members(Writer& writer, const T& obj) {
    bool first = true;
    write_members_impl(writer, obj, first);
}

template <class Writer, class Variant>
void write_variant(Writer& writer, const Variant& v) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    std::visit([&](auto const& alt) {
        using A = std::remove_cvref_t<decltype(alt)>;
        if constexpr (tagging == variant_tagging::external) {
            writer.push_back('{');
            write_escaped_string(writer, type_name_of<A>());
            writer.push_back(':');
            write_dispatch(writer, alt);
            writer.push_back('}');
        } else if constexpr (tagging == variant_tagging::internal) {
            writer.push_back('{');
            write_escaped_string(writer, variant_policy<Variant>::tag_field);
            writer.push_back(':');
            write_escaped_string(writer, type_name_of<A>());
            if constexpr (reflectable_class<A>) {
                bool first = false;
                write_members_impl(writer, alt, first);
            } else {
                writer.push_back(',');
                write_escaped_string(writer, std::string_view{"value"});
                writer.push_back(':');
                write_dispatch(writer, alt);
            }
            writer.push_back('}');
        } else if constexpr (tagging == variant_tagging::adjacent) {
            writer.push_back('{');
            write_escaped_string(writer, variant_policy<Variant>::tag_field);
            writer.push_back(':');
            write_escaped_string(writer, type_name_of<A>());
            writer.push_back(',');
            write_escaped_string(writer, variant_policy<Variant>::content_field);
            writer.push_back(':');
            write_dispatch(writer, alt);
            writer.push_back('}');
        } else { // untagged
            write_dispatch(writer, alt);
        }
    }, v);
}

template <class T>
std::string write(const T& val) {
    std::string str;
    str.reserve(256);
    string_writer writer{str};
    write_dispatch(writer, val);
    return str;
}

template <class T>
rflcpp::result<size_t> write_to_span(std::span<char> buffer, const T& val) {
    span_writer writer{buffer};
    write_dispatch(writer, val);
    if (writer.overflowed) {
        return fail(error{error_kind::out_of_range, "Buffer space exceeded"});
    }
    return writer.cursor;
}

} // namespace rflcpp::json::direct
