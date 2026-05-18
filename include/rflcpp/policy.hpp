// rflcpp/policy.hpp - Per-type customization knobs.
// SPDX-License-Identifier: MIT
//
// Each policy is a primary template users specialize for their own types:
//
//   rflcpp::access_policy<T>     - whether to walk private/protected members
//   rflcpp::naming_policy<T>     - default member-name transformation
//   rflcpp::strict_policy<T>     - whether unknown fields are errors
//   rflcpp::variant_policy<T>    - how std::variant<...> is encoded
//   rflcpp::base_policy<T>       - whether to traverse base-class members

#pragma once

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

#include <rflcpp/attributes.hpp>

namespace rflcpp {

enum class access_mode {
    all,
    public_only,
};

template <class T>
struct access_policy {
    static constexpr access_mode mode = access_mode::public_only;
};

template <class T>
struct strict_policy {
    static constexpr bool strict = false;
};

enum class base_mode {
    flatten,
    nested,
    skip,
};

template <class T>
struct base_policy {
    static constexpr base_mode mode = base_mode::flatten;
};

/// Mirrors serde's four variant tagging strategies:
///   external  -  { "Alt": payload }                       (default)
///   internal  -  { "tag": "Alt", ...fields_of_alt... }
///   adjacent  -  { "tag": "Alt", "content": payload }
///   untagged  -  payload directly; on read, try each alternative in order
enum class variant_tagging { external, internal, adjacent, untagged };

template <class>
struct variant_policy {
    static constexpr variant_tagging   tagging       = variant_tagging::external;
    static constexpr std::string_view  tag_field     = "type";
    static constexpr std::string_view  content_field = "content";
};

enum class case_style {
    as_written,
    snake_case,
    upper_snake_case,
    camel_case,
    pascal_case,
    kebab_case,
    lower_case,
};

namespace detail {

constexpr bool is_upper(char c) noexcept { return c >= 'A' && c <= 'Z'; }
constexpr bool is_lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
constexpr char to_upper(char c) noexcept { return is_lower(c) ? char(c - ('a'-'A')) : c; }
constexpr char to_lower(char c) noexcept { return is_upper(c) ? char(c + ('a'-'A')) : c; }

/// Splits `in` into words on underscore/hyphen/space and on camelCase /
/// PascalCase boundaries (with "XMLHttp" -> "XML","Http").
template <class Sink>
constexpr void split_words(std::string_view in, Sink&& sink) {
    std::string buf;
    auto flush = [&] { if (!buf.empty()) { sink(buf); buf.clear(); } };

    for (std::size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '_' || c == '-' || c == ' ') { flush(); continue; }
        if (!buf.empty()) {
            char p = buf.back();
            bool boundary = false;
            if ((is_lower(p) || is_digit(p)) && is_upper(c)) boundary = true;
            else if (is_upper(p) && is_upper(c) && i + 1 < in.size() && is_lower(in[i + 1]))
                boundary = true;
            if (boundary) flush();
        }
        buf.push_back(c);
    }
    flush();
}

constexpr std::string apply_case(std::string_view in, case_style style) {
    if (style == case_style::as_written) return std::string{in};

    std::string out;
    out.reserve(in.size() + 8);
    bool first_word = true;

    split_words(in, [&](std::string_view word) {
        switch (style) {
            case case_style::snake_case:
                if (!first_word) out.push_back('_');
                for (char c : word) out.push_back(to_lower(c));
                break;
            case case_style::upper_snake_case:
                if (!first_word) out.push_back('_');
                for (char c : word) out.push_back(to_upper(c));
                break;
            case case_style::kebab_case:
                if (!first_word) out.push_back('-');
                for (char c : word) out.push_back(to_lower(c));
                break;
            case case_style::camel_case:
                if (first_word) {
                    for (char c : word) out.push_back(to_lower(c));
                } else {
                    bool first_char = true;
                    for (char c : word) {
                        out.push_back(first_char ? to_upper(c) : to_lower(c));
                        first_char = false;
                    }
                }
                break;
            case case_style::pascal_case: {
                bool first_char = true;
                for (char c : word) {
                    out.push_back(first_char ? to_upper(c) : to_lower(c));
                    first_char = false;
                }
                break;
            }
            case case_style::lower_case:
                for (char c : word) out.push_back(to_lower(c));
                break;
            case case_style::as_written:
                out.append(word);
                break;
        }
        first_word = false;
    });
    return out;
}

} // namespace detail

template <class T>
struct naming_policy {
    static constexpr case_style style = case_style::as_written;

    static std::string transform(std::string_view member_name) {
        return detail::apply_case(member_name, style);
    }
};

} // namespace rflcpp
