// rflcpp/result.hpp - result<T> alias and small helpers.
// SPDX-License-Identifier: MIT
#pragma once

#include <expected>
#include <utility>

#include <rflcpp/error.hpp>

namespace rflcpp {

template <class T>
using result = std::expected<T, error>;

template <class T>
[[nodiscard]] constexpr result<std::remove_cvref_t<T>> ok(T&& value) {
    return result<std::remove_cvref_t<T>>{std::forward<T>(value)};
}

/// Construct an error-only result that implicitly converts to any `result<T>`.
[[nodiscard]] inline std::unexpected<error> fail(error e) {
    return std::unexpected<error>{std::move(e)};
}

} // namespace rflcpp
