// rflcpp/cbor.hpp - CBOR serialization using nlohmann/json.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef RFLCPP_ENABLE_JSON

#include <rflcpp/json.hpp>
#include <span>
#include <vector>

namespace rflcpp {

template <class T>
[[nodiscard]] std::vector<uint8_t> to_cbor(const T& value) {
    return njson::to_cbor(detail::json::write_dispatch(value));
}

template <class T>
[[nodiscard]] result<T> from_cbor(std::span<const uint8_t> data) {
    try {
        njson j = njson::from_cbor(data.begin(), data.end());
        return detail::json::read_dispatch<T>(j, "$");
    } catch (const njson::parse_error& e) {
        return fail(error{error_kind::parse_error, e.what(), "$"});
    }
}

} // namespace rflcpp

#endif // RFLCPP_ENABLE_JSON
