// rflcpp/policy.hpp - Per-type customization knobs.
// SPDX-License-Identifier: MIT

#pragma once

#include <cctype>
#include <concepts>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>

#include <rflcpp/attributes.hpp>

namespace rflcpp {

enum class access_mode {
    all,
    public_only,
};

enum class base_mode {
    flatten,
    nested,
    skip,
};

enum class variant_tagging { external, internal, adjacent, untagged };

enum class case_style {
    as_written,
    snake_case,
    upper_snake_case,
    camel_case,
    pascal_case,
    kebab_case,
    lower_case,
};

// --- Namespace Policy Tags for Class-Embedded Policies ---
namespace policy {

struct strict {
    static constexpr bool value = true;
};
struct lenient {
    static constexpr bool value = false;
};

struct access_all {
    static constexpr access_mode value = access_mode::all;
};
struct access_public {
    static constexpr access_mode value = access_mode::public_only;
};

struct base_flatten {
    static constexpr base_mode value = base_mode::flatten;
};
struct base_nested {
    static constexpr base_mode value = base_mode::nested;
};
struct base_skip {
    static constexpr base_mode value = base_mode::skip;
};

template <variant_tagging Tagging, fixed_string TagField = "type", fixed_string ContentField = "content">
struct variant {
    static constexpr variant_tagging tagging = Tagging;
    static constexpr std::string_view tag_field = TagField.view();
    static constexpr std::string_view content_field = ContentField.view();
};

using external_variant = variant<variant_tagging::external>;
using internal_variant = variant<variant_tagging::internal>;
using adjacent_variant = variant<variant_tagging::adjacent>;
using untagged_variant = variant<variant_tagging::untagged>;

template <case_style Style>
struct naming {
    static constexpr case_style style = Style;
};

using as_written = naming<case_style::as_written>;
using snake_case = naming<case_style::snake_case>;
using upper_snake_case = naming<case_style::upper_snake_case>;
using camel_case = naming<case_style::camel_case>;
using pascal_case = naming<case_style::pascal_case>;
using kebab_case = naming<case_style::kebab_case>;
using lower_case = naming<case_style::lower_case>;

} // namespace policy

// --- Compile-Time Helper for Extracting Policies from Tuples ---

template <class T, class = void>
struct is_complete : std::false_type {};

template <class T>
struct is_complete<T, std::void_t<decltype(sizeof(T))>> : std::true_type {};

template <class T>
inline constexpr bool is_complete_v = is_complete<T>::value;

template <class T>
concept has_embedded_policies = is_complete_v<T> && requires {
    { T::rflcpp_policies };
};

namespace detail {

struct default_strict_tag { static constexpr bool value = false; };
struct default_access_tag { static constexpr access_mode value = access_mode::public_only; };
struct default_base_tag { static constexpr base_mode value = base_mode::flatten; };
struct default_variant_tag {
    static constexpr variant_tagging tagging = variant_tagging::external;
    static constexpr std::string_view tag_field = "type";
    static constexpr std::string_view content_field = "content";
};
struct default_naming_tag { static constexpr case_style style = case_style::as_written; };

template <class T, bool HasPolicies>
struct extract_policies_helper {
    using type = std::tuple<>;
};

template <class T>
struct extract_policies_helper<T, true> {
    using type = decltype(T::rflcpp_policies);
};

template <class T>
using extract_policies_t = typename extract_policies_helper<T, has_embedded_policies<T>>::type;

template <class T>
struct is_strict_tag {
    static constexpr bool value = requires { { T::value } -> std::convertible_to<bool>; };
};

template <class T>
struct is_access_tag {
    static constexpr bool value = requires { { T::value } -> std::convertible_to<access_mode>; };
};

template <class T>
struct is_base_tag {
    static constexpr bool value = requires { { T::value } -> std::convertible_to<base_mode>; };
};

template <class T>
struct is_variant_tag {
    static constexpr bool value = requires { { T::tagging } -> std::convertible_to<variant_tagging>; };
};

template <class T>
struct is_naming_tag {
    static constexpr bool value = requires { { T::style } -> std::convertible_to<case_style>; };
};

template <class T>
struct type_holder {
    using type = T;
};

template <class F>
struct chain_node {
    F f;
};

template <class F, class G>
consteval auto operator+(chain_node<F> lhs, chain_node<G> rhs) {
    return chain_node{[f = lhs.f, g = rhs.f]() {
        auto res_f = f();
        using FT = typename decltype(res_f)::type;
        if constexpr (!std::is_same_v<FT, void>) {
            return res_f;
        } else {
            return g();
        }
    }};
}

template <class Tuple, template <class> class Predicate, class Default>
struct find_policy;

template <class... Ps, template <class> class Predicate, class Default>
struct find_policy<std::tuple<Ps...>, Predicate, Default> {
    consteval static auto get() {
        if constexpr (sizeof...(Ps) == 0) {
            return type_holder<Default>{};
        } else {
            auto make_node = []<class P>() {
                return chain_node{[]() {
                    if constexpr (Predicate<P>::value) {
                        return type_holder<P>{};
                    } else {
                        return type_holder<void>{};
                    }
                }};
            };
            
            auto folded = (make_node.template operator()<Ps>() + ... + chain_node{[]() {
                return type_holder<Default>{};
            }});
            
            return folded.f();
        }
    }

    using type = typename decltype(get())::type;
};

template <class Tuple, template <class> class Predicate, class Default>
using find_policy_t = typename find_policy<std::remove_cvref_t<Tuple>, Predicate, Default>::type;

} // namespace detail

// --- Trait Structures ---

template <class T>
struct strict_policy {
    using Tuple = detail::extract_policies_t<T>;
    using Match = detail::find_policy_t<Tuple, detail::is_strict_tag, detail::default_strict_tag>;
    static constexpr bool strict = Match::value;
};

template <class T>
struct access_policy {
    using Tuple = detail::extract_policies_t<T>;
    using Match = detail::find_policy_t<Tuple, detail::is_access_tag, detail::default_access_tag>;
    static constexpr access_mode mode = Match::value;
};

template <class T>
struct base_policy {
    using Tuple = detail::extract_policies_t<T>;
    using Match = detail::find_policy_t<Tuple, detail::is_base_tag, detail::default_base_tag>;
    static constexpr base_mode mode = Match::value;
};

template <class T>
struct variant_policy {
private:
    using Tuple = detail::extract_policies_t<T>;
    using Match = detail::find_policy_t<Tuple, detail::is_variant_tag, detail::default_variant_tag>;
public:
    static constexpr variant_tagging   tagging       = Match::tagging;
    static constexpr std::string_view  tag_field     = Match::tag_field;
    static constexpr std::string_view  content_field = Match::content_field;
};

namespace detail {

constexpr bool is_upper(char c) noexcept { return c >= 'A' && c <= 'Z'; }
constexpr bool is_lower(char c) noexcept { return c >= 'a' && c <= 'z'; }
constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
constexpr char to_upper(char c) noexcept { return is_lower(c) ? char(c - ('a'-'A')) : c; }
constexpr char to_lower(char c) noexcept { return is_upper(c) ? char(c + ('a'-'A')) : c; }

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
private:
    using Tuple = detail::extract_policies_t<T>;
    using Match = detail::find_policy_t<Tuple, detail::is_naming_tag, detail::default_naming_tag>;
public:
    static constexpr case_style style = Match::style;

    static constexpr std::string transform(std::string_view member_name) {
        return detail::apply_case(member_name, style);
    }
};

} // namespace rflcpp
