// src/internal_common.hpp - Internal helpers shared between rflcpp's .cpp files.
// SPDX-License-Identifier: MIT
#pragma once

#include <string_view>

namespace rflcpp::detail {

[[nodiscard]] std::string_view library_version() noexcept;

[[nodiscard]] std::string_view error_kind_tag(int kind_int) noexcept;

} // namespace rflcpp::detail
