// rflcpp/invoke.hpp - Dynamic method invocation via C++26 static reflection.
// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <expected>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#if __has_include(<meta>)
#  include <meta>
#elif __has_include(<experimental/meta>)
#  include <experimental/meta>
#endif

#include <rflcpp/any.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/json.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/result.hpp>

namespace rflcpp {

namespace detail::invoke {

template <std::meta::info F>
consteval auto get_member_ptr() {
    return &[: F :];
}

template <std::size_t N>
consteval std::array<std::meta::info, N> get_params(std::meta::info f) {
    if constexpr (N == 0) {
        return std::array<std::meta::info, 0>{};
    } else {
        auto vec = std::meta::parameters_of(f);
        std::array<std::meta::info, N> arr{};
        for (std::size_t i = 0; i < N; ++i) {
            arr[i] = vec[i];
        }
        return arr;
    }
}

template <class T>
rflcpp::result<T> coerce_arg(const rflcpp::any& arg, std::string_view method_name, std::size_t arg_idx) {
    if (arg.empty()) {
        return rflcpp::fail(error(error_kind::type_mismatch,
            "Argument is empty", std::string(method_name) + "[" + std::to_string(arg_idx) + "]"));
    }

    // 1. Direct type match (fast path)
    if (arg.type_id() == typeid(T)) {
        if (auto* ptr = arg.cast<T>()) {
            return *ptr;
        }
    }

    // 2. Format-agnostic arithmetic coercion
    if constexpr (std::is_arithmetic_v<T>) {
        #define TRY_CAST(SrcT) \
            if (arg.type_id() == typeid(SrcT)) { \
                if (auto* ptr = arg.cast<SrcT>()) { \
                    return static_cast<T>(*ptr); \
                } \
            }
        TRY_CAST(bool)
        TRY_CAST(char)
        TRY_CAST(signed char)
        TRY_CAST(unsigned char)
        TRY_CAST(short)
        TRY_CAST(unsigned short)
        TRY_CAST(int)
        TRY_CAST(unsigned int)
        TRY_CAST(long)
        TRY_CAST(unsigned long)
        TRY_CAST(long long)
        TRY_CAST(unsigned long long)
        TRY_CAST(float)
        TRY_CAST(double)
        TRY_CAST(long double)
        #undef TRY_CAST
    }

    // 3. Format-agnostic string-to-enum coercion
    if constexpr (enum_like<T>) {
        if (arg.type_id() == typeid(std::string)) {
            if (auto* ptr = arg.cast<std::string>()) {
                if (auto enum_val = rflcpp::enum_value<T>(*ptr)) {
                    return *enum_val;
                }
            }
        }
        if (arg.type_id() == typeid(std::string_view)) {
            if (auto* ptr = arg.cast<std::string_view>()) {
                if (auto enum_val = rflcpp::enum_value<T>(*ptr)) {
                    return *enum_val;
                }
            }
        }
    }

    // 4. JSON fallback (if enabled)
#ifdef RFLCPP_ENABLE_JSON
    try {
        auto js_str = arg.to_json().dump();
        auto res = rflcpp::from_json<T>(js_str);
        if (res.has_value()) {
            return std::move(*res);
        }
    } catch (...) {}
#endif

    return rflcpp::fail(error(error_kind::type_mismatch,
        "Failed to convert argument to expected type",
        std::string(method_name) + "[" + std::to_string(arg_idx) + "]"));
}

template <class T>
consteval std::size_t count_methods() {
    auto mems = std::meta::members_of(
        ^^T, std::meta::access_context::unchecked());
    std::size_t count = 0;
    for (auto m : mems) {
        if (std::meta::is_function(m) && std::meta::has_identifier(m) &&
            !std::meta::is_special_member_function(m)) {
            count++;
        }
    }
    return count;
}

template <class T, std::size_t N>
consteval std::array<std::meta::info, N> get_methods() {
    auto mems = std::meta::members_of(
        ^^T, std::meta::access_context::unchecked());
    std::array<std::meta::info, N> arr{};
    std::size_t idx = 0;
    for (auto m : mems) {
        if (std::meta::is_function(m) && std::meta::has_identifier(m) &&
            !std::meta::is_special_member_function(m)) {
            if (idx < N) {
                arr[idx++] = m;
            }
        }
    }
    return arr;
}

template <class T, std::meta::info F, std::size_t... Is>
rflcpp::result<rflcpp::any> call_method_helper(T& obj, const std::vector<rflcpp::any>& args, std::string_view name, std::index_sequence<Is...>) {
    constexpr auto P_count = sizeof...(Is);
    constexpr auto params_refl = get_params<P_count>(F);
    
    // Coerce args to decayed (non-reference) types
    auto coerced = std::make_tuple(
        coerce_arg<std::decay_t<typename [: std::meta::type_of(params_refl[Is]) :]>> (args[Is], name, Is)...
    );

    error err;
    bool success = true;
    auto check_errors = [&](auto&& res) {
        if (!res.has_value()) {
            err = res.error();
            success = false;
        }
    };
    (check_errors(std::get<Is>(coerced)), ...);
    if (!success) {
        return rflcpp::fail(std::move(err));
    }

    // Call member function using get_member_ptr consteval helper evaluated in a constexpr variable
    constexpr auto m_ptr = get_member_ptr<F>();
    using Ret = typename [: std::meta::return_type_of(F) :];
    if constexpr (std::is_same_v<Ret, void>) {
        (obj.*m_ptr)(std::move(*std::get<Is>(coerced))...);
        return rflcpp::any();
    } else {
        return rflcpp::any((obj.*m_ptr)(std::move(*std::get<Is>(coerced))...));
    }
}

template <class T, std::meta::info F>
rflcpp::result<rflcpp::any> try_call_method(T& obj, const std::vector<rflcpp::any>& args, std::string_view name) {
    constexpr auto expected_args = std::meta::parameters_of(F).size();
    if (args.size() != expected_args) {
        return rflcpp::fail(error(error_kind::type_mismatch,
            "Method '" + std::string(name) + "' expects " + std::to_string(expected_args) +
            " arguments, but " + std::to_string(args.size()) + " were provided."));
    }
    
    return call_method_helper<T, F>(obj, args, name, std::make_index_sequence<expected_args>{});
}

template <class T, std::size_t N, std::size_t Is>
bool match_and_call(T& obj, std::string_view method_name, const std::vector<rflcpp::any>& args, rflcpp::result<rflcpp::any>& ret) {
    constexpr auto methods = get_methods<T, N>();
    constexpr auto F = methods[Is];
    constexpr auto name = std::meta::identifier_of(F);
    if (method_name == name) {
        ret = try_call_method<T, F>(obj, args, method_name);
        return true;
    }
    return false;
}

template <class T, std::size_t N, std::size_t... Is>
rflcpp::result<rflcpp::any> invoke_helper(T& obj, std::string_view method_name, const std::vector<rflcpp::any>& args, std::index_sequence<Is...>) {
    rflcpp::result<rflcpp::any> ret = rflcpp::fail(error(error_kind::method_not_found,
        "Method '" + std::string(method_name) + "' not found on type '" + std::string(type_name_of<T>()) + "'."));

    (match_and_call<T, N, Is>(obj, method_name, args, ret) || ...);

    return ret;
}

} // namespace detail::invoke

template <class T>
rflcpp::result<rflcpp::any> invoke(T& obj, std::string_view method_name, const std::vector<rflcpp::any>& args) {
    constexpr auto N = detail::invoke::count_methods<T>();
    if constexpr (N == 0) {
        return rflcpp::fail(error(error_kind::method_not_found,
            "No invocable member functions found on type '" + std::string(type_name_of<T>()) + "'."));
    } else {
        return detail::invoke::invoke_helper<T, N>(obj, method_name, args, std::make_index_sequence<N>{});
    }
}

} // namespace rflcpp
