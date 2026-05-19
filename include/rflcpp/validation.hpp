// rflcpp/validation.hpp - Compile-time field-level validators.
// SPDX-License-Identifier: MIT
//
// `validated<T, Rules...>` wraps a `T` and applies a pack of validation
// rules at construction time. Use the throwing constructor for direct
// construction, or `validated::make(v)` for an error-returning factory.

#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <rflcpp/error.hpp>
#include <rflcpp/result.hpp>

namespace rflcpp {

/// A rule is a default-constructible type providing:
///   static constexpr std::string_view name();
///   template <class T> static std::optional<std::string> check(const T&);
template <class R, class T>
concept validation_rule = requires(const T& v) {
    { R::name() } -> std::convertible_to<std::string_view>;
    { R::check(v) } -> std::convertible_to<std::optional<std::string>>;
};

namespace rules {

template <auto Min>
struct min_value {
    static constexpr std::string_view name() noexcept { return "min_value"; }
    template <class T>
    static std::optional<std::string> check(const T& v) {
        if (!(v >= Min)) return std::string{"value must be >= "} + std::to_string(Min);
        return std::nullopt;
    }
};

template <auto Max>
struct max_value {
    static constexpr std::string_view name() noexcept { return "max_value"; }
    template <class T>
    static std::optional<std::string> check(const T& v) {
        if (!(v <= Max)) return std::string{"value must be <= "} + std::to_string(Max);
        return std::nullopt;
    }
};

template <std::size_t Min>
struct min_length {
    static constexpr std::string_view name() noexcept { return "min_length"; }
    template <class T>
    static std::optional<std::string> check(const T& v) {
        using std::size;
        if (size(v) < Min)
            return std::string{"length must be >= "} + std::to_string(Min);
        return std::nullopt;
    }
};

template <std::size_t Max>
struct max_length {
    static constexpr std::string_view name() noexcept { return "max_length"; }
    template <class T>
    static std::optional<std::string> check(const T& v) {
        using std::size;
        if (size(v) > Max)
            return std::string{"length must be <= "} + std::to_string(Max);
        return std::nullopt;
    }
};

struct non_empty {
    static constexpr std::string_view name() noexcept { return "non_empty"; }
    template <class T>
    static std::optional<std::string> check(const T& v) {
        using std::empty;
        if (empty(v)) return std::string{"must not be empty"};
        return std::nullopt;
    }
};

} // namespace rules

namespace detail {
inline thread_local int g_bypass_validation = 0;
}

template <class T, class... Rules>
class validated {
    static_assert((validation_rule<Rules, T> && ...),
                  "All Rules must satisfy validation_rule<R, T>.");
public:
    validated() : value_() {
        if (!detail::g_bypass_validation) {
            if (auto e = check_all(value_)) throw rflcpp_error(std::move(*e));
        }
    }
    using value_type = T;

    explicit validated(T v) : value_(std::move(v)) {
        if (!detail::g_bypass_validation) {
            if (auto e = check_all(value_)) throw rflcpp_error(std::move(*e));
        }
    }

    static result<validated> make(T v) {
        if (auto e = check_all(v)) return fail(std::move(*e));
        return validated{std::move(v), construct_unchecked{}};
    }

    const T& get()          const noexcept { return value_; }
    const T& operator*()    const noexcept { return value_; }
    const T* operator->()   const noexcept { return &value_; }
    operator const T&()     const noexcept { return value_; }

private:
    struct construct_unchecked {};
    validated(T v, construct_unchecked) : value_(std::move(v)) {}

    static std::optional<error> check_all(const T& v) {
        std::optional<error> err;
        [&]<class... R>(std::tuple<R...>*) {
            ([&] {
                if (err) return;
                run_rule<R>(v, err);
            }(), ...);
        }(static_cast<std::tuple<Rules...>*>(nullptr));
        return err;
    }

    template <class Rule>
    static bool run_rule(const T& v, std::optional<error>& sink) {
        if (auto msg = Rule::check(v)) {
            sink.emplace(error_kind::validation_failed,
                         std::string{Rule::name()} + ": " + *msg);
            return true;
        }
        return false;
    }

    T value_;
};

template <class> struct is_validated : std::false_type {};
template <class T, class... R> struct is_validated<validated<T, R...>> : std::true_type {};
template <class T> inline constexpr bool is_validated_v = is_validated<std::remove_cvref_t<T>>::value;

namespace detail {

template <class Rule> struct rule_traits_base {
    static constexpr std::optional<double> min_v() { return std::nullopt; }
    static constexpr std::optional<double> max_v() { return std::nullopt; }
    static constexpr std::optional<std::size_t> min_l() { return std::nullopt; }
    static constexpr std::optional<std::size_t> max_l() { return std::nullopt; }
};

template <class Rule>
struct rule_traits : rule_traits_base<Rule> {};

template <auto V>
struct rule_traits<rules::min_value<V>> : rule_traits_base<rules::min_value<V>> {
    static constexpr std::optional<double> min_v() { return static_cast<double>(V); }
};

template <auto V>
struct rule_traits<rules::max_value<V>> : rule_traits_base<rules::max_value<V>> {
    static constexpr std::optional<double> max_v() { return static_cast<double>(V); }
};

template <std::size_t V>
struct rule_traits<rules::min_length<V>> : rule_traits_base<rules::min_length<V>> {
    static constexpr std::optional<std::size_t> min_l() { return V; }
};

template <std::size_t V>
struct rule_traits<rules::max_length<V>> : rule_traits_base<rules::max_length<V>> {
    static constexpr std::optional<std::size_t> max_l() { return V; }
};

} // namespace detail

template <class T> struct validated_traits;
template <class T, class... R> struct validated_traits<validated<T, R...>> {
    static constexpr auto min_value() {
        std::optional<double> res;
        (..., (res = res ? res : detail::rule_traits<R>::min_v()));
        return res;
    }
    static constexpr auto max_value() {
        std::optional<double> res;
        (..., (res = res ? res : detail::rule_traits<R>::max_v()));
        return res;
    }
    static constexpr auto min_length() {
        std::optional<std::size_t> res;
        (..., (res = res ? res : detail::rule_traits<R>::min_l()));
        return res;
    }
    static constexpr auto max_length() {
        std::optional<std::size_t> res;
        (..., (res = res ? res : detail::rule_traits<R>::max_l()));
        return res;
    }
};

} // namespace rflcpp
