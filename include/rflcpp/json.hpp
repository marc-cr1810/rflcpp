// rflcpp/json.hpp - JSON serialization built on the reflection façade.
// SPDX-License-Identifier: MIT
//
//   std::string        rflcpp::to_json(const T&, json_options = {});
//   rflcpp::result<T>  rflcpp::from_json<T>(std::string_view);
//   T                  rflcpp::from_json_or_throw<T>(std::string_view);
//
// Specialize `rflcpp::json_codec<T>` to add support for custom types.

#pragma once

#include <array>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <rflcpp/attributes.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/enum_meta.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/policy.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>

namespace rflcpp {

struct json_options {
    /// If non-zero, pretty-print with this indent width.
    unsigned indent = 0;
    /// Override `strict_policy<T>::strict` for this call.
    bool strict = false;
};

/// Specialize this to add custom-type support for `to_json` / `from_json`.
template <class T, class = void>
struct json_codec;

namespace detail::json {

class writer {
public:
    explicit writer(json_options opts) : opts_(opts) {}

    [[nodiscard]] std::string release() && { return std::move(out_); }

    template <class T> void write(const T& v) { write_dispatch(v); }

    // Low-level helpers exposed for user codecs.
    void raw(char c)              { out_.push_back(c); }
    void raw(std::string_view s)  { out_.append(s); }

    void open_object()  { raw('{'); ++depth_; first_at_depth_.push_back(true); }
    void close_object() { --depth_; first_at_depth_.pop_back(); newline_indent(); raw('}'); }
    void open_array()   { raw('['); ++depth_; first_at_depth_.push_back(true); }
    void close_array()  { --depth_; first_at_depth_.pop_back(); newline_indent(); raw(']'); }

    void item_separator() {
        if (first_at_depth_.back()) first_at_depth_.back() = false;
        else                        raw(',');
        newline_indent();
    }

    void key(std::string_view k) {
        item_separator();
        write_string(k);
        raw(':');
        if (opts_.indent) raw(' ');
    }

    void write_string(std::string_view s) {
        raw('"');
        for (char ch : s) {
            switch (ch) {
                case '"':  raw("\\\""); break;
                case '\\': raw("\\\\"); break;
                case '\b': raw("\\b");  break;
                case '\f': raw("\\f");  break;
                case '\n': raw("\\n");  break;
                case '\r': raw("\\r");  break;
                case '\t': raw("\\t");  break;
                default:
                    if (static_cast<unsigned char>(ch) < 0x20) {
                        char buf[8];
                        auto n = std::snprintf(buf, sizeof(buf), "\\u%04x",
                                               static_cast<unsigned>(ch));
                        out_.append(buf, static_cast<std::size_t>(n));
                    } else {
                        raw(ch);
                    }
                    break;
            }
        }
        raw('"');
    }

    void write_null() { raw("null"); }

private:
    void newline_indent() {
        if (!opts_.indent) return;
        raw('\n');
        out_.append(static_cast<std::size_t>(depth_) * opts_.indent, ' ');
    }

    template <class T> void write_dispatch(const T& v);

    // Write a reflectable object's members into the currently-open object.
    template <class T> void write_members(const T& obj);

    // Trampoline -- definition is at the bottom of this file, after
    // `write_variant_dispatch` has been fully declared.
    template <class Variant> void write_variant(const Variant& v);

    json_options       opts_;
    std::string        out_;
    int                depth_ = 0;
    std::vector<bool>  first_at_depth_{};
};

// Forward declaration so the order of definitions below doesn't matter.
template <class Owner, class AttrType>
std::string resolve_attr_name(std::string_view member_name);

/// Resolve the effective wire name for a member: explicit `field<"name">` >
/// `attrs::rename<"...">` > naming_policy > reflected identifier.
template <class Owner, class FieldType>
inline std::string effective_key(std::string_view member_name) {
    if constexpr (is_field_v<FieldType>) {
        return std::string{FieldType::name()};
    } else if constexpr (is_attr_v<FieldType>) {
        return resolve_attr_name<Owner, FieldType>(member_name);
    } else {
        return naming_policy<Owner>::transform(member_name);
    }
}

template <class Owner, class AttrType>
inline std::string resolve_attr_name(std::string_view member_name) {
    using attrs_tuple = typename AttrType::attributes;
    constexpr auto rn = [&]<class... A>(std::tuple<A...>*) consteval {
        return rflcpp::detail::rename_of<A...>();
    }(static_cast<attrs_tuple*>(nullptr));

    if (!rn.empty()) return std::string{rn};
    return naming_policy<Owner>::transform(member_name);
}

template <class FieldType>
consteval bool skip_on_write_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        return F::template has<attrs::skip>
            || F::template has<attrs::skip_on_write>
            || F::template has<attrs::read_only>;
    } else return false;
}

template <class FieldType>
consteval bool sensitive_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::sensitive>;
    else                                         return false;
}

template <class FieldType>
consteval bool skip_if_null_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::skip_if_null>;
    else                                         return false;
}

template <class FieldType>
consteval bool skip_if_default_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::skip_if_default>;
    else                                         return false;
}

template <class FieldType>
consteval bool flatten_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::flatten>;
    else                                         return false;
}

/// Peel an `attr` / `field` wrapper to access its inner value.
template <class FieldType, class Ref>
constexpr auto& unwrap_ref(Ref& r) noexcept {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return r.value;
    else                                         return r;
}

// Dispatch order: user codec > validated > attr/field wrapper > optional >
// variant > enum > bool > number > string > map > sequence > reflectable.
template <class T>
void writer::write_dispatch(const T& v) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { json_codec<U>::write(*this, v); }) {
        json_codec<U>::write(*this, v);
    }
    else if constexpr (requires { v.get(); typename U::value_type;
                                  requires std::same_as<U,
                                      validated<typename U::value_type>>; }) {
        write_dispatch(v.get());
    }
    else if constexpr (is_wrapped_v<U>) {
        write_dispatch(v.value);
    }
    else if constexpr (optional_like<U>) {
        if (v.has_value()) write_dispatch(*v);
        else               write_null();
    }
    else if constexpr (variant_like<U>) {
        write_variant(v);
    }
    else if constexpr (enum_like<U>) {
        if constexpr (enum_flags_policy<U>::is_flags) {
            auto names = enum_flag_names(v);
            open_array();
            for (auto const& n : names) { item_separator(); write_string(n); }
            close_array();
        } else {
            write_string(enum_name(v));
        }
    }
    else if constexpr (boolean_like<U>) {
        raw(v ? "true" : "false");
    }
    else if constexpr (numeric_like<U>) {
        if constexpr (std::is_floating_point_v<U>) {
            if (!std::isfinite(v)) { write_null(); return; }
        }
        char buf[32];
        auto [p, ec] = std::to_chars(buf, buf + sizeof(buf), v);
        out_.append(buf, static_cast<std::size_t>(p - buf));
    }
    else if constexpr (string_like<U>) {
        write_string(std::string_view{v});
    }
    else if constexpr (map_like<U>) {
        open_object();
        for (auto const& [k, val] : v) { key(std::string_view{k}); write_dispatch(val); }
        close_object();
    }
    else if constexpr (sequence_like<U>) {
        open_array();
        for (auto const& e : v) { item_separator(); write_dispatch(e); }
        close_array();
    }
    else if constexpr (reflectable_class<U>) {
        open_object();
        write_members(v);
        close_object();
    } else {
        static_assert(sizeof(U) == 0,
            "rflcpp::to_json: no codec for this type. Specialize "
            "rflcpp::json_codec<T> to add support.");
    }
}

template <class T>
void writer::write_members(const T& obj) {
    using U = std::remove_cvref_t<T>;

    // Walk direct bases first (per base_policy). The base loop is expanded
    // inline here rather than via a generic helper -- see the note in
    // <rflcpp/reflect.hpp> about GCC 16's immediate-escalation behavior.
    if constexpr (base_count_of<U>() > 0 &&
                  base_policy<U>::mode != base_mode::skip) {
        static constexpr auto bases = rflcpp::detail::direct_bases<U>();
        template for (constexpr auto base : bases) {
            using B = typename [: std::meta::type_of(base) :];
            const B& base_ref = static_cast<const B&>(obj);
            if constexpr (base_policy<U>::mode == base_mode::flatten) {
                write_members(base_ref);
            } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                key(type_name_of<B>());
                open_object();
                write_members(base_ref);
                close_object();
            }
        }
    }

    for_each_field(obj, [&](std::string_view member_name, auto const& field_ref) {
        using FT = std::remove_cvref_t<decltype(field_ref)>;

        if constexpr (skip_on_write_v<FT>()) return;

        if constexpr (flatten_v<FT>()) {
            auto const& inner = unwrap_ref<FT>(field_ref);
            if constexpr (reflectable_class<std::remove_cvref_t<decltype(inner)>>)
                write_members(inner);
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

        key(effective_key<U, FT>(member_name));

        if constexpr (sensitive_v<FT>()) {
            if constexpr (string_like<InnerT>) write_string("***");
            else if constexpr (optional_like<InnerT>) {
                if (inner.has_value()) write_string("***");
                else                   write_null();
            } else {
                write_null();
            }
        } else {
            write_dispatch(inner);
        }
    });
}

template <class Variant>
void write_variant_external(writer& w, const Variant& v) {
    std::visit([&](auto const& alt) {
        using A = std::remove_cvref_t<decltype(alt)>;
        w.open_object();
        w.key(type_name_of<A>());
        w.write(alt);
        w.close_object();
    }, v);
}

template <class Variant>
void write_variant_internal(writer& w, const Variant& v) {
    std::visit([&](auto const& alt) {
        using A = std::remove_cvref_t<decltype(alt)>;
        w.open_object();
        w.key(variant_policy<Variant>::tag_field);
        w.write_string(type_name_of<A>());
        if constexpr (reflectable_class<A>) {
            // Inline `alt`'s members rather than nesting a sub-object.
            for_each_field(alt, [&](std::string_view name, auto const& fr) {
                using FT = std::remove_cvref_t<decltype(fr)>;
                if constexpr (skip_on_write_v<FT>()) return;
                w.key(effective_key<A, FT>(name));
                w.write(unwrap_ref<FT>(fr));
            });
        }
        w.close_object();
    }, v);
}

template <class Variant>
void write_variant_adjacent(writer& w, const Variant& v) {
    std::visit([&](auto const& alt) {
        using A = std::remove_cvref_t<decltype(alt)>;
        w.open_object();
        w.key(variant_policy<Variant>::tag_field);
        w.write_string(type_name_of<A>());
        w.key(variant_policy<Variant>::content_field);
        w.write(alt);
        w.close_object();
    }, v);
}

template <class Variant>
void write_variant_untagged(writer& w, const Variant& v) {
    std::visit([&](auto const& alt) { w.write(alt); }, v);
}

template <class Variant>
inline void write_variant_dispatch(writer& w, const Variant& v) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    if constexpr      (tagging == variant_tagging::external) write_variant_external(w, v);
    else if constexpr (tagging == variant_tagging::internal) write_variant_internal(w, v);
    else if constexpr (tagging == variant_tagging::adjacent) write_variant_adjacent(w, v);
    else                                                     write_variant_untagged(w, v);
}

} // namespace detail::json

template <class T>
[[nodiscard]] std::string to_json(const T& value, json_options opts = {}) {
    detail::json::writer w{opts};
    w.write(value);
    return std::move(w).release();
}

namespace detail::json {

/// Intermediate JSON value used during parsing.
struct value {
    enum class kind { null_, bool_, int_, double_, string_, array_, object_ };
    kind k = kind::null_;
    bool                          b = false;
    std::int64_t                  i = 0;
    double                        d = 0.0;
    std::string                   s;
    std::vector<value>            a;
    std::vector<std::pair<std::string, value>> o;

    const value* find(std::string_view key) const {
        for (auto const& [k_, v_] : o) if (k_ == key) return &v_;
        return nullptr;
    }
};

class parser {
public:
    explicit parser(std::string_view src) : src_(src) {}

    result<value> parse() {
        skip_ws();
        auto v = parse_value();
        if (!v) return v;
        skip_ws();
        if (pos_ != src_.size())
            return fail({error_kind::parse_error, "trailing characters", path()});
        return v;
    }

private:
    std::string_view src_;
    std::size_t      pos_ = 0;

    std::string path() const { return {}; }

    void skip_ws() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++pos_;
            else break;
        }
    }
    bool consume(char c) {
        if (pos_ < src_.size() && src_[pos_] == c) { ++pos_; return true; }
        return false;
    }
    bool match_kw(std::string_view kw) {
        if (src_.substr(pos_, kw.size()) == kw) { pos_ += kw.size(); return true; }
        return false;
    }

    result<value> parse_value() {
        skip_ws();
        if (pos_ >= src_.size())
            return fail({error_kind::parse_error, "unexpected end of input", path()});
        char c = src_[pos_];
        switch (c) {
            case '{': return parse_object();
            case '[': return parse_array();
            case '"': return parse_string_value();
            case 't': case 'f': return parse_bool();
            case 'n': return parse_null();
            default:  return parse_number();
        }
    }
    result<value> parse_null() {
        if (match_kw("null")) { value v; v.k = value::kind::null_; return v; }
        return fail({error_kind::parse_error, "expected 'null'", path()});
    }
    result<value> parse_bool() {
        value v; v.k = value::kind::bool_;
        if (match_kw("true"))  { v.b = true;  return v; }
        if (match_kw("false")) { v.b = false; return v; }
        return fail({error_kind::parse_error, "expected boolean", path()});
    }
    result<value> parse_number() {
        std::size_t start = pos_;
        if (consume('-')) {}
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        bool is_float = false;
        if (pos_ < src_.size() && src_[pos_] == '.') {
            is_float = true; ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            is_float = true; ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        }
        std::string_view tok = src_.substr(start, pos_ - start);
        if (tok.empty())
            return fail({error_kind::parse_error, "expected number", path()});

        value v;
        if (is_float) {
            v.k = value::kind::double_;
            auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v.d);
            if (ec != std::errc{} || p != tok.data() + tok.size())
                return fail({error_kind::parse_error, "invalid float", path()});
        } else {
            v.k = value::kind::int_;
            auto [p, ec] = std::from_chars(tok.data(), tok.data() + tok.size(), v.i);
            if (ec != std::errc{} || p != tok.data() + tok.size())
                return fail({error_kind::parse_error, "invalid integer", path()});
        }
        return v;
    }
    result<std::string> parse_string_raw() {
        if (!consume('"'))
            return fail({error_kind::parse_error, "expected '\"'", path()});
        std::string out;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') return out;
            if (c == '\\') {
                if (pos_ >= src_.size())
                    return fail({error_kind::parse_error, "bad escape", path()});
                char esc = src_[pos_++];
                switch (esc) {
                    case '"':  out += '"';  break;
                    case '\\': out += '\\'; break;
                    case '/':  out += '/';  break;
                    case 'b':  out += '\b'; break;
                    case 'f':  out += '\f'; break;
                    case 'n':  out += '\n'; break;
                    case 'r':  out += '\r'; break;
                    case 't':  out += '\t'; break;
                    case 'u': {
                        if (pos_ + 4 > src_.size())
                            return fail({error_kind::parse_error, "bad \\u escape", path()});
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            char hex = src_[pos_++];
                            cp <<= 4;
                            if      (hex >= '0' && hex <= '9') cp |= unsigned(hex - '0');
                            else if (hex >= 'a' && hex <= 'f') cp |= unsigned(hex - 'a' + 10);
                            else if (hex >= 'A' && hex <= 'F') cp |= unsigned(hex - 'A' + 10);
                            else return fail({error_kind::parse_error, "bad hex digit", path()});
                        }
                        if (cp < 0x80) out += static_cast<char>(cp);
                        else if (cp < 0x800) {
                            out += static_cast<char>(0xC0 | (cp >> 6));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (cp >> 12));
                            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (cp & 0x3F));
                        }
                        break;
                    }
                    default:
                        return fail({error_kind::parse_error, "unknown escape", path()});
                }
            } else out += c;
        }
        return fail({error_kind::parse_error, "unterminated string", path()});
    }
    result<value> parse_string_value() {
        auto s = parse_string_raw();
        if (!s) return fail(s.error());
        value v; v.k = value::kind::string_; v.s = std::move(*s);
        return v;
    }
    result<value> parse_array() {
        if (!consume('[')) return fail({error_kind::parse_error, "expected '['", path()});
        value v; v.k = value::kind::array_;
        skip_ws();
        if (consume(']')) return v;
        while (true) {
            auto e = parse_value();
            if (!e) return e;
            v.a.push_back(std::move(*e));
            skip_ws();
            if (consume(',')) { skip_ws(); continue; }
            if (consume(']')) return v;
            return fail({error_kind::parse_error, "expected ',' or ']'", path()});
        }
    }
    result<value> parse_object() {
        if (!consume('{')) return fail({error_kind::parse_error, "expected '{'", path()});
        value v; v.k = value::kind::object_;
        skip_ws();
        if (consume('}')) return v;
        while (true) {
            skip_ws();
            auto k = parse_string_raw();
            if (!k) return fail(k.error());
            skip_ws();
            if (!consume(':'))
                return fail({error_kind::parse_error, "expected ':'", path()});
            auto val = parse_value();
            if (!val) return val;
            v.o.emplace_back(std::move(*k), std::move(*val));
            skip_ws();
            if (consume(',')) continue;
            if (consume('}')) return v;
            return fail({error_kind::parse_error, "expected ',' or '}'", path()});
        }
    }
};

template <class T>
result<T> shape_dispatch(const value& v, std::string_view path);

template <class T>
result<T> shape(const value& v, std::string_view path) {
    return shape_dispatch<T>(v, path);
}

inline std::string join_path(std::string_view a, std::string_view b) {
    if (a.empty()) return std::string{b};
    if (b.empty()) return std::string{a};
    std::string out{a};
    if (!b.starts_with('[')) out += '.';
    out += b;
    return out;
}

template <class Variant, std::size_t I = 0>
result<Variant> read_variant_external(const value& v, std::string_view path) {
    if constexpr (I == std::variant_size_v<Variant>) {
        return fail({error_kind::type_mismatch,
                     "no variant alternative matched the discriminator",
                     std::string{path}});
    } else {
        using A = std::variant_alternative_t<I, Variant>;
        if (v.k != value::kind::object_)
            return fail({error_kind::type_mismatch, "expected object", std::string{path}});
        auto alt_name = type_name_of<A>();
        for (auto const& [k, sub] : v.o) {
            if (k == alt_name) {
                auto inner = shape<A>(sub, join_path(path, k));
                if (!inner) return fail(inner.error());
                return Variant{std::move(*inner)};
            }
        }
        return read_variant_external<Variant, I + 1>(v, path);
    }
}

template <class Variant, std::size_t I = 0>
result<Variant> read_variant_untagged(const value& v, std::string_view path) {
    if constexpr (I == std::variant_size_v<Variant>) {
        return fail({error_kind::type_mismatch,
                     "no untagged variant alternative matched",
                     std::string{path}});
    } else {
        using A = std::variant_alternative_t<I, Variant>;
        auto attempt = shape<A>(v, path);
        if (attempt) return Variant{std::move(*attempt)};
        return read_variant_untagged<Variant, I + 1>(v, path);
    }
}

template <class Variant, std::size_t I = 0>
result<Variant> read_variant_adjacent_for(std::string_view tag_value,
                                          const value& content,
                                          std::string_view path) {
    if constexpr (I == std::variant_size_v<Variant>) {
        return fail({error_kind::type_mismatch,
                     "unknown variant tag '" + std::string{tag_value} + "'",
                     std::string{path}});
    } else {
        using A = std::variant_alternative_t<I, Variant>;
        if (type_name_of<A>() == tag_value) {
            auto inner = shape<A>(content, path);
            if (!inner) return fail(inner.error());
            return Variant{std::move(*inner)};
        }
        return read_variant_adjacent_for<Variant, I + 1>(tag_value, content, path);
    }
}

template <class Variant, std::size_t I = 0>
result<Variant> read_variant_internal_for(std::string_view tag_value,
                                          const value& obj,
                                          std::string_view path) {
    if constexpr (I == std::variant_size_v<Variant>) {
        return fail({error_kind::type_mismatch,
                     "unknown variant tag '" + std::string{tag_value} + "'",
                     std::string{path}});
    } else {
        using A = std::variant_alternative_t<I, Variant>;
        if (type_name_of<A>() == tag_value) {
            // Re-shape the whole object (minus the tag field) into A.
            value clone = obj;
            constexpr auto tf = variant_policy<Variant>::tag_field;
            auto it = std::find_if(clone.o.begin(), clone.o.end(),
                [](auto const& kv){ return kv.first == tf; });
            if (it != clone.o.end()) clone.o.erase(it);
            auto inner = shape<A>(clone, path);
            if (!inner) return fail(inner.error());
            return Variant{std::move(*inner)};
        }
        return read_variant_internal_for<Variant, I + 1>(tag_value, obj, path);
    }
}

template <class Variant>
result<Variant> read_variant(const value& v, std::string_view path) {
    constexpr auto tagging = variant_policy<Variant>::tagging;
    if constexpr (tagging == variant_tagging::external)
        return read_variant_external<Variant>(v, path);
    else if constexpr (tagging == variant_tagging::untagged)
        return read_variant_untagged<Variant>(v, path);
    else if constexpr (tagging == variant_tagging::adjacent) {
        if (v.k != value::kind::object_)
            return fail({error_kind::type_mismatch, "expected object", std::string{path}});
        constexpr auto tf = variant_policy<Variant>::tag_field;
        constexpr auto cf = variant_policy<Variant>::content_field;
        const value* tag = v.find(tf);
        const value* content = v.find(cf);
        if (!tag || tag->k != value::kind::string_)
            return fail({error_kind::missing_field,
                "missing or non-string tag field '" + std::string{tf} + "'",
                std::string{path}});
        if (!content)
            return fail({error_kind::missing_field,
                "missing content field '" + std::string{cf} + "'",
                std::string{path}});
        return read_variant_adjacent_for<Variant>(tag->s, *content, path);
    }
    else { // internal
        if (v.k != value::kind::object_)
            return fail({error_kind::type_mismatch, "expected object", std::string{path}});
        constexpr auto tf = variant_policy<Variant>::tag_field;
        const value* tag = v.find(tf);
        if (!tag || tag->k != value::kind::string_)
            return fail({error_kind::missing_field,
                "missing or non-string tag field '" + std::string{tf} + "'",
                std::string{path}});
        return read_variant_internal_for<Variant>(tag->s, v, path);
    }
}

template <class FieldType>
consteval bool skip_on_read_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        return F::template has<attrs::skip> || F::template has<attrs::skip_on_read>
            || F::template has<attrs::write_only>;
    } else return false;
}

template <class FieldType>
consteval bool required_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>)
        return F::template has<attrs::required>;
    else return false;
}

template <class FieldType>
consteval auto aliases_pair() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        using attrs_tuple = typename F::attributes;
        return [&]<class... A>(std::tuple<A...>*) consteval {
            return rflcpp::detail::aliases_of<A...>();
        }(static_cast<attrs_tuple*>(nullptr));
    } else {
        return std::pair{std::array<std::string_view, 0>{}, std::size_t{0}};
    }
}

template <class Owner, class FieldType, class Out>
void read_member(const value& parent, std::string_view member_name,
                 Out& out_field, std::string_view path,
                 std::optional<error>& failure)
{
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (skip_on_read_v<F>()) return;

    std::string canonical = effective_key<Owner, F>(member_name);
    const value* found = parent.find(canonical);
    if (!found) {
        constexpr auto ap = aliases_pair<F>();
        constexpr auto arr = ap.first;
        constexpr auto count = ap.second;
        for (std::size_t i = 0; i < count; ++i)
            if ((found = parent.find(arr[i]))) break;
    }

    if (!found) {
        auto& inner = unwrap_ref<F>(out_field);
        using InnerT = std::remove_cvref_t<decltype(inner)>;

        // Missing optional with no `required` attribute is fine.
        if constexpr (!required_v<F>() && optional_like<InnerT>) return;

        // `default_to<V>` provides a fallback value.
        if constexpr (is_attr_v<F> || is_field_v<F>) {
            using attrs_tuple = typename F::attributes;
            constexpr bool has_dflt = [&]<class... A>(std::tuple<A...>*) consteval {
                return rflcpp::detail::has_default_value<InnerT, A...>();
            }(static_cast<attrs_tuple*>(nullptr));
            if constexpr (has_dflt) {
                inner = [&]<class... A>(std::tuple<A...>*) consteval {
                    return rflcpp::detail::default_value<InnerT, A...>();
                }(static_cast<attrs_tuple*>(nullptr));
                return;
            }
        }

        failure = error{error_kind::missing_field,
                        "required field missing: " + canonical,
                        join_path(path, canonical)};
        return;
    }

    auto& inner = unwrap_ref<F>(out_field);
    using InnerT = std::remove_cvref_t<decltype(inner)>;
    auto shaped = shape<InnerT>(*found, join_path(path, canonical));
    if (!shaped) { failure = shaped.error(); return; }
    inner = std::move(*shaped);
}

template <class T>
result<T> shape_object(const value& v, std::string_view path);

template <class T>
result<T> shape_dispatch(const value& v, std::string_view path) {
    using U = std::remove_cvref_t<T>;

    if constexpr (requires { json_codec<U>::read(v, path); }) {
        return json_codec<U>::read(v, path);
    }
    else if constexpr (optional_like<U>) {
        if (v.k == value::kind::null_) return U{};
        auto inner = shape<typename U::value_type>(v, path);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (variant_like<U>) {
        return read_variant<U>(v, path);
    }
    else if constexpr (enum_like<U>) {
        if constexpr (enum_flags_policy<U>::is_flags) {
            if (v.k != value::kind::array_)
                return fail({error_kind::type_mismatch, "expected flag array", std::string{path}});
            std::vector<std::string_view> names;
            for (auto const& e : v.a) {
                if (e.k != value::kind::string_)
                    return fail({error_kind::type_mismatch, "flag must be a string", std::string{path}});
                names.push_back(e.s);
            }
            auto val = enum_flags_from<U>(names);
            if (!val)
                return fail({error_kind::type_mismatch, "unknown flag name", std::string{path}});
            return *val;
        } else {
            if (v.k != value::kind::string_)
                return fail({error_kind::type_mismatch, "expected enum-as-string", std::string{path}});
            auto val = enum_value<U>(v.s);
            if (!val)
                return fail({error_kind::type_mismatch,
                             "unknown enumerator '" + v.s + "'", std::string{path}});
            return *val;
        }
    }
    else if constexpr (boolean_like<U>) {
        if (v.k != value::kind::bool_)
            return fail({error_kind::type_mismatch, "expected bool", std::string{path}});
        return v.b;
    }
    else if constexpr (numeric_like<U>) {
        if (v.k == value::kind::int_)    return static_cast<U>(v.i);
        if (v.k == value::kind::double_) return static_cast<U>(v.d);
        return fail({error_kind::type_mismatch, "expected number", std::string{path}});
    }
    else if constexpr (std::same_as<U, std::string>) {
        if (v.k != value::kind::string_)
            return fail({error_kind::type_mismatch, "expected string", std::string{path}});
        return v.s;
    }
    else if constexpr (sequence_like<U>) {
        if (v.k != value::kind::array_)
            return fail({error_kind::type_mismatch, "expected array", std::string{path}});
        U out{};
        std::size_t idx = 0;
        for (auto const& e : v.a) {
            auto sub = shape<typename U::value_type>(
                e, join_path(path, "[" + std::to_string(idx) + "]"));
            if (!sub) return fail(sub.error());
            out.insert(out.end(), std::move(*sub));
            ++idx;
        }
        return out;
    }
    else if constexpr (map_like<U>) {
        if (v.k != value::kind::object_)
            return fail({error_kind::type_mismatch, "expected object", std::string{path}});
        U out{};
        for (auto const& [k, val] : v.o) {
            auto sub = shape<typename U::mapped_type>(val, join_path(path, k));
            if (!sub) return fail(sub.error());
            out.emplace(typename U::key_type{k}, std::move(*sub));
        }
        return out;
    }
    else if constexpr (reflectable_class<U>) {
        return shape_object<U>(v, path);
    } else {
        static_assert(sizeof(U) == 0,
            "rflcpp::from_json: no codec for this type. Specialize "
            "rflcpp::json_codec<T> to add support.");
    }
}

template <class T>
result<T> shape_object(const value& v, std::string_view path) {
    using U = std::remove_cvref_t<T>;

    if (v.k != value::kind::object_)
        return fail({error_kind::type_mismatch, "expected object", std::string{path}});

    U out{};
    std::optional<error> failure;

    // Base loop expanded inline; see <rflcpp/reflect.hpp> for the rationale.
    if constexpr (base_count_of<U>() > 0 &&
                  base_policy<U>::mode != base_mode::skip) {
        static constexpr auto bases = rflcpp::detail::direct_bases<U>();
        template for (constexpr auto base : bases) {
            if (failure) continue;
            using B = typename [: std::meta::type_of(base) :];
            B& base_ref = static_cast<B&>(out);
            if constexpr (base_policy<U>::mode == base_mode::flatten) {
                auto sub = shape_object<B>(v, path);
                if (!sub) { failure = sub.error(); continue; }
                base_ref = std::move(*sub);
            } else if constexpr (base_policy<U>::mode == base_mode::nested) {
                const value* slot = v.find(type_name_of<B>());
                if (!slot) {
                    failure = error{error_kind::missing_field,
                        "missing nested base '" + std::string{type_name_of<B>()} + "'",
                        join_path(path, type_name_of<B>())};
                    continue;
                }
                auto sub = shape_object<B>(*slot, join_path(path, type_name_of<B>()));
                if (!sub) { failure = sub.error(); continue; }
                base_ref = std::move(*sub);
            }
        }
        if (failure) return fail(std::move(*failure));
    }

    for_each_field(out, [&](std::string_view name, auto& field_ref) {
        if (failure) return;
        using FT = std::remove_cvref_t<decltype(field_ref)>;

        if constexpr (flatten_v<FT>()) {
            auto& inner = unwrap_ref<FT>(field_ref);
            using InnerT = std::remove_cvref_t<decltype(inner)>;
            if constexpr (reflectable_class<InnerT>) {
                auto sub = shape_object<InnerT>(v, path);
                if (!sub) { failure = sub.error(); return; }
                inner = std::move(*sub);
            }
            return;
        }
        read_member<U, FT>(v, name, field_ref, path, failure);
    });

    if (failure) return fail(std::move(*failure));
    return out;
}

} // namespace detail::json

// ---------------------------------------------------------------------------
// Public reader entry points
// ---------------------------------------------------------------------------

template <class T>
[[nodiscard]] result<T> from_json(std::string_view text) {
    detail::json::parser p{text};
    auto dom = p.parse();
    if (!dom) return fail(dom.error());
    return detail::json::shape<T>(*dom, std::string_view{});
}

template <class T>
[[nodiscard]] T from_json_or_throw(std::string_view text) {
    auto r = from_json<T>(text);
    if (!r) throw rflcpp_error(r.error());
    return std::move(*r);
}

} // namespace rflcpp

namespace rflcpp::detail::json {

// Defined here, after `write_variant_dispatch` is fully declared above.
template <class Variant>
inline void writer::write_variant(const Variant& v) {
    write_variant_dispatch(*this, v);
}

} // namespace rflcpp::detail::json
