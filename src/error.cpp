// src/error.cpp - Out-of-line definitions for rflcpp::error.
// SPDX-License-Identifier: MIT
#include <rflcpp/error.hpp>

#include <string>
#include <string_view>

#include "internal_common.hpp"

namespace rflcpp {

namespace detail {

std::string_view error_kind_tag(error_kind kind) noexcept {
    switch (kind) {
        case error_kind::parse_error:       return "parse_error";
        case error_kind::type_mismatch:     return "type_mismatch";
        case error_kind::missing_field:     return "missing_field";
        case error_kind::unknown_field:     return "unknown_field";
        case error_kind::method_not_found:   return "method_not_found";
        case error_kind::validation_failed: return "validation_failed";
        case error_kind::out_of_range:      return "out_of_range";
        case error_kind::user_defined:      return "user_defined";
    }
    return "unknown";
}

} // namespace detail

std::string error::what() const {
    std::string out;
    out.reserve(message_.size() + path_.size() + 32);
    out += '[';
    out += detail::error_kind_tag(kind_);
    out += ']';
    if (!path_.empty()) {
        out += " at '";
        out += path_;
        out += "'";
    }
    out += ": ";
    out += message_;
    return out;
}

} // namespace rflcpp
