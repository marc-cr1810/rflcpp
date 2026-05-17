// rflcpp/cli.hpp - Automatic CLI parser generated from reflectable structs.
// SPDX-License-Identifier: MIT

#pragma once

#include <charconv>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <map>

#include <rflcpp/attributes.hpp>
#include <rflcpp/detail/serialization_common.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/result.hpp>

namespace rflcpp::cli {

namespace detail {

template <class T>
bool from_string(std::string_view s, T& out) {
    if constexpr (std::is_same_v<T, std::string>) {
        out = std::string{s};
        return true;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (s == "true" || s == "1" || s == "yes") { out = true; return true; }
        if (s == "false" || s == "0" || s == "no") { out = false; return true; }
        return false;
    } else if constexpr (std::is_integral_v<T>) {
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        return ec == std::errc{};
    } else if constexpr (std::is_floating_point_v<T>) {
        // Simple fallback as from_chars for float is not always available in all compilers
        try {
            if constexpr (std::is_same_v<T, float>) out = std::stof(std::string{s});
            else out = std::stod(std::string{s});
            return true;
        } catch (...) { return false; }
    }
    return false;
}

} // namespace detail

template <class T>
void print_help() {
    std::cout << "Usage: [options]" << std::endl;
    std::cout << "Options:" << std::endl;
    template_for_each_field<T>([&]<class FT>(std::string_view name) {
        using U = std::remove_cvref_t<T>;
        std::string key = rflcpp::detail::serialization::effective_key<U, FT>(name);
        
        std::cout << "  --" << key;
        
        if constexpr (rflcpp::is_wrapped_v<FT>) {
            using attrs_tuple = typename FT::attributes;
            constexpr auto desc = []<class... A>(std::tuple<A...>*) {
                return rflcpp::detail::description_of<A...>();
            }(static_cast<attrs_tuple*>(nullptr));
            if (!desc.empty()) std::cout << " : " << desc;
        }
        std::cout << std::endl;
    });
}

template <class T>
result<T> parse(int argc, char** argv) {
    using U = std::remove_cvref_t<T>;
    T obj;
    
    std::map<std::string, std::string_view> args;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_help<T>();
            return fail(error{error_kind::validation_failed, "help requested"});
        }
        
        if (arg.starts_with("--")) {
            auto pos = arg.find('=');
            if (pos != std::string_view::npos) {
                args[std::string{arg.substr(2, pos - 2)}] = arg.substr(pos + 1);
            } else if (i + 1 < argc && !std::string_view(argv[i + 1]).starts_with("-")) {
                args[std::string{arg.substr(2)}] = argv[++i];
            } else {
                args[std::string{arg.substr(2)}] = "true"; // Flag
            }
        } else if (arg.starts_with("-")) {
            // Short flags could be handled here if we extract them from attributes
        }
    }
    
    std::optional<error> failure;
    for_each_field(obj, [&](std::string_view member_name, auto& field_ref) {
        if (failure) return;
        using FT = std::remove_cvref_t<decltype(field_ref)>;
        std::string key = rflcpp::detail::serialization::effective_key<U, FT>(member_name);
        
        auto it = args.find(key);
        if (it != args.end()) {
            auto& inner = rflcpp::detail::serialization::unwrap_ref<FT>(field_ref);
            if (!detail::from_string(it->second, inner)) {
                failure = error{error_kind::type_mismatch, "invalid value for " + key};
            }
        }
    });
    
    if (failure) return fail(*failure);
    return obj;
}

} // namespace rflcpp::cli
