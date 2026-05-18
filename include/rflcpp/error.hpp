// rflcpp/error.hpp - Error types reported by deserializers and validators.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace rflcpp {

enum class error_kind {
    parse_error,
    type_mismatch,
    missing_field,
    unknown_field,
    method_not_found,
    validation_failed,
    out_of_range,
    user_defined,
};

class error {
public:
    error() = default;
    error(error_kind kind, std::string message, std::string path = {})
        : kind_(kind), message_(std::move(message)), path_(std::move(path)) {}

    error_kind        kind()    const noexcept { return kind_; }
    std::string_view  message() const noexcept { return message_; }

    /// A dotted path describing where in the input the error occurred,
    /// e.g. "users[3].address.zip".
    std::string_view  path()    const noexcept { return path_; }

    [[nodiscard]] std::string what() const;

private:
    error_kind  kind_ = error_kind::user_defined;
    std::string message_;
    std::string path_;
};

/// Exception thrown by the `*_or_throw` overloads.
class rflcpp_error : public std::runtime_error {
public:
    explicit rflcpp_error(error e)
        : std::runtime_error(e.what()), err_(std::move(e)) {}

    // Named `value()` (not `error()`) to avoid shadowing the `error` type.
    const error& value() const noexcept { return err_; }

private:
    error err_;
};

} // namespace rflcpp
