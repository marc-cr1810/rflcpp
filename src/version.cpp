// src/version.cpp - Runtime-queryable version string.
// SPDX-License-Identifier: MIT
#include <rflcpp/rflcpp.hpp>

#include "internal_common.hpp"

namespace rflcpp::detail {

std::string_view library_version() noexcept {
    return RFLCPP_VERSION_STRING;
}

} // namespace rflcpp::detail
