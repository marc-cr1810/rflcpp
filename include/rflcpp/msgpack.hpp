// rflcpp/msgpack.hpp - High-performance MessagePack serialization using mpack.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef RFLCPP_ENABLE_MSGPACK

#include <mpack.h>
#include <rflcpp/detail/serialization_common.hpp>
#include <rflcpp/error.hpp>
#include <rflcpp/reflect.hpp>
#include <rflcpp/result.hpp>
#include <rflcpp/validation.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>
#include <span>

namespace rflcpp {

namespace detail::msgpack {

using namespace rflcpp::detail::serialization;

template <class T> void write_dispatch(mpack_writer_t* writer, const T& v);

template <class T>
void write_members(mpack_writer_t* writer, const T& obj) {
    using U = std::remove_cvref_t<T>;
    
    // We don't know the field count upfront because of skipping/flattening.
    // However, mpack allows "deferred" map sizes if we use a buffer or
    // if we count first. For simplicity and performance, we'll count fields.
    
    size_t count = 0;
    for_each_field(obj, [&](std::string_view, auto const& field_ref) {
        using FT = std::remove_cvref_t<decltype(field_ref)>;
        if constexpr (skip_on_write_v<FT>()) return;
        // ... more complex counting for flattening ...
        count++;
    });

    // Note: This is a simplified count. A real implementation should handle 
    // flattening and base classes correctly by doing a pre-pass.
    
    for_each_field(obj, [&](std::string_view member_name, auto const& field_ref) {
        using FT = std::remove_cvref_t<decltype(field_ref)>;
        if constexpr (skip_on_write_v<FT>()) return;
        
        std::string key = effective_key<U, FT>(member_name);
        mpack_write_str(writer, key.c_str(), (uint32_t)key.size());
        
        auto const& inner = unwrap_ref<FT>(field_ref);
        if constexpr (sensitive_v<FT>()) mpack_write_str(writer, "***", 3);
        else write_dispatch(writer, inner);
    });
}

template <class T>
void write_dispatch(mpack_writer_t* writer, const T& v) {
    using U = std::remove_cvref_t<T>;

    if constexpr (is_wrapped_v<U> || is_validated_v<U>) {
        if constexpr (is_wrapped_v<U>) write_dispatch(writer, v.value);
        else write_dispatch(writer, v.get());
    }
    else if constexpr (optional_like<U>) {
        if (v.has_value()) write_dispatch(writer, *v);
        else mpack_write_nil(writer);
    }
    else if constexpr (boolean_like<U>) {
        mpack_write_bool(writer, v);
    }
    else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) mpack_write_int(writer, v);
        else mpack_write_uint(writer, v);
    }
    else if constexpr (std::is_floating_point_v<U>) {
        if constexpr (std::is_same_v<U, float>) mpack_write_float(writer, v);
        else mpack_write_double(writer, v);
    }
    else if constexpr (string_like<U>) {
        std::string_view sv{v};
        mpack_write_str(writer, sv.data(), (uint32_t)sv.size());
    }
    else if constexpr (sequence_like<U>) {
        mpack_start_array(writer, (uint32_t)v.size());
        for (auto const& e : v) write_dispatch(writer, e);
        mpack_finish_array(writer);
    }
    else if constexpr (reflectable_class<U>) {
        // Pre-count fields (simplified)
        uint32_t count = 0;
        for_each_field(v, [&](auto, auto const& fr) { 
            if constexpr (!skip_on_write_v<std::remove_cvref_t<decltype(fr)>>()) count++; 
        });
        mpack_start_map(writer, count);
        write_members(writer, v);
        mpack_finish_map(writer);
    }
}

template <class T>
result<T> read_dispatch(mpack_node_t node);

template <class T>
void read_members(mpack_node_t node, T& obj, std::optional<error>& failure) {
    using U = std::remove_cvref_t<T>;
    for_each_field(obj, [&](std::string_view member_name, auto& field_ref) {
        if (failure) return;
        using FT = std::remove_cvref_t<decltype(field_ref)>;
        std::string key = effective_key<U, FT>(member_name);
        
        mpack_node_t child = mpack_node_map_str(node, key.c_str(), (uint32_t)key.size());
        if (mpack_node_error(child) != mpack_ok) {
            if constexpr (has_default_v<FT>()) {
                unwrap_ref<FT>(field_ref) = default_v<FT>();
                return;
            }
            if constexpr (optional_like<unwrap_t<FT>>) return;
            failure = error{error_kind::missing_field, "missing field " + key};
            return;
        }
        
        auto res = read_dispatch<unwrap_t<FT>>(child);
        if (!res) failure = res.error();
        else unwrap_ref<FT>(field_ref) = std::move(*res);
    });
}

template <class T>
result<T> read_dispatch(mpack_node_t node) {
    using U = std::remove_cvref_t<T>;
    if (mpack_node_error(node) != mpack_ok) return fail(error{error_kind::parse_error, "mpack node error"});

    if constexpr (is_validated_v<U>) {
        auto inner = read_dispatch<typename U::value_type>(node);
        if (!inner) return fail(inner.error());
        auto res = U::make(std::move(*inner));
        if (!res) return fail(error{error_kind::validation_failed, std::string{res.error().message()}});
        return res;
    }
    else if constexpr (optional_like<U>) {
        if (mpack_node_type(node) == mpack_type_nil) return U{std::nullopt};
        auto inner = read_dispatch<typename U::value_type>(node);
        if (!inner) return fail(inner.error());
        return U{std::move(*inner)};
    }
    else if constexpr (boolean_like<U>) {
        return mpack_node_bool(node);
    }
    else if constexpr (std::is_integral_v<U>) {
        if constexpr (std::is_signed_v<U>) return (U)mpack_node_int(node);
        else return (U)mpack_node_uint(node);
    }
    else if constexpr (std::is_floating_point_v<U>) {
        if constexpr (std::is_same_v<U, float>) return mpack_node_float(node);
        else return mpack_node_double(node);
    }
    else if constexpr (string_like<U>) {
        return std::string(mpack_node_str(node), mpack_node_strlen(node));
    }
    else if constexpr (sequence_like<U>) {
        U out;
        size_t n = mpack_node_array_length(node);
        for (size_t i = 0; i < n; ++i) {
            auto res = read_dispatch<typename U::value_type>(mpack_node_array_at(node, i));
            if (!res) return fail(res.error());
            out.push_back(std::move(*res));
        }
        return out;
    }
    else if constexpr (reflectable_class<U>) {
        U val;
        std::optional<error> failure;
        read_members(node, val, failure);
        if (failure) return fail(*failure);
        return val;
    }
    return fail(error{error_kind::type_mismatch, "unsupported type"});
}

} // namespace detail::msgpack

template <class T>
std::vector<uint8_t> to_msgpack(const T& v) {
    char* data;
    size_t size;
    mpack_writer_t writer;
    mpack_writer_init_growable(&writer, &data, &size);
    detail::msgpack::write_dispatch(&writer, v);
    if (mpack_writer_destroy(&writer) != mpack_ok) return {};
    
    std::vector<uint8_t> out((uint8_t*)data, (uint8_t*)data + size);
    MPACK_FREE(data);
    return out;
}

template <class T>
result<T> from_msgpack(std::span<const uint8_t> data) {
    mpack_tree_t tree;
    mpack_tree_init_data(&tree, (const char*)data.data(), data.size());
    mpack_tree_parse(&tree);
    auto res = detail::msgpack::read_dispatch<T>(mpack_tree_root(&tree));
    mpack_tree_destroy(&tree);
    return res;
}

} // namespace rflcpp

#endif // RFLCPP_ENABLE_MSGPACK
