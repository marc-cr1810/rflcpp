// rflcpp/attributes.hpp - Wrapper-type field attributes.
// SPDX-License-Identifier: MIT
//
// Field metadata is expressed via transparent wrapper types because P3394
// annotations aren't yet in mainstream toolchains. Two forms:
//
//   attr<T, Attrs...>              - keep the reflected name, attach attrs
//   field<"name", T, Attrs...>     - explicit wire name, optional attrs
//
// Attribute tag types live in `rflcpp::attrs::*` and are empty markers.

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

namespace rflcpp {

/// A compile-time string usable as an NTTP.
template <std::size_t N>
struct fixed_string {
    char data[N]{};

    consteval fixed_string(const char (&s)[N]) {
        std::copy_n(s, N, data);
    }

    [[nodiscard]] constexpr std::string_view view() const noexcept {
        return {data, N > 0 ? N - 1 : 0};
    }

    constexpr auto operator<=>(const fixed_string&) const = default;

    template <std::size_t M>
    constexpr bool operator==(const fixed_string<M>& other) const noexcept {
        return view() == other.view();
    }

    constexpr bool operator==(std::string_view sv) const noexcept {
        return view() == sv;
    }

    friend constexpr bool operator==(std::string_view sv, const fixed_string& fs) noexcept {
        return sv == fs.view();
    }
};

template <std::size_t N>
fixed_string(const char (&)[N]) -> fixed_string<N>;

namespace attrs {

/// Excluded from both serialization and deserialization.
struct skip {};

/// Excluded from serialization only.
struct skip_on_write {};

/// Excluded from deserialization only.
struct skip_on_read {};

/// Aliases for readability.
using write_only = skip_on_read;
using read_only  = skip_on_write;

/// Replaced with `"***"` (or `null`) in output; behaves normally on read.
struct sensitive {};

/// Optional fields tagged with this become mandatory on input.
struct required {};

/// Hoist the wrapped struct's fields into the surrounding object.
struct flatten {};

/// Omit empty `optional<T>` fields entirely rather than emitting `null`.
struct skip_if_null {};

/// Omit fields equal to `T{}` from output.
struct skip_if_default {};

/// Override the wire key name.
template <fixed_string Name>
struct rename {};

/// Accept alternative input keys (the canonical name still wins on write).
template <fixed_string... Names>
struct aliases {};

/// Substitute `V` when the key is missing on input.
template <auto V>
struct default_to {};

/// Human-readable description for documentation (e.g. JSON Schema).
template <fixed_string S>
struct description {};

/// Human-readable title for documentation.
template <fixed_string S>
struct title {};

} // namespace attrs

namespace detail {

template <class A, class... Attrs>
inline constexpr bool has_attr_v = (std::is_same_v<A, Attrs> || ...);

// Attribute tag types are empty -- we extract NTTPs back out via overload
// resolution on `T*` arguments. Only the partial specialization matching
// the actual attribute type is viable.

template <fixed_string N>
consteval std::string_view rename_name_for(::rflcpp::attrs::rename<N>*) {
    return N.view();
}

template <fixed_string S>
consteval std::string_view description_for(::rflcpp::attrs::description<S>*) {
    return S.view();
}

template <fixed_string S>
consteval std::string_view title_for(::rflcpp::attrs::title<S>*) {
    return S.view();
}

template <class... Attrs>
consteval std::string_view rename_of() {
    std::string_view out{};
    (
        [&]<class A>() {
            if constexpr (requires(A* p) { rename_name_for(p); })
                out = rename_name_for(static_cast<A*>(nullptr));
        }.template operator()<Attrs>(),
        ...
    );
    return out;
}

template <class... Attrs>
consteval std::string_view description_of() {
    std::string_view out{};
    (
        [&]<class A>() {
            if constexpr (requires(A* p) { description_for(p); })
                out = description_for(static_cast<A*>(nullptr));
        }.template operator()<Attrs>(),
        ...
    );
    return out;
}

template <class... Attrs>
consteval std::string_view title_of() {
    std::string_view out{};
    (
        [&]<class A>() {
            if constexpr (requires(A* p) { title_for(p); })
                out = title_for(static_cast<A*>(nullptr));
        }.template operator()<Attrs>(),
        ...
    );
    return out;
}

template <fixed_string... Ns>
consteval auto aliases_for(::rflcpp::attrs::aliases<Ns...>*) {
    return std::array<std::string_view, sizeof...(Ns)>{ Ns.view()... };
}

template <class... Attrs>
consteval auto aliases_of() {
    constexpr std::size_t MaxAliases = 8;
    std::array<std::string_view, MaxAliases> out{};
    std::size_t count = 0;
    (
        [&]<class A>() {
            if constexpr (requires(A* p) { aliases_for(p); }) {
                constexpr auto entries = aliases_for(static_cast<A*>(nullptr));
                for (auto sv : entries) if (count < MaxAliases) out[count++] = sv;
            }
        }.template operator()<Attrs>(),
        ...
    );
    return std::pair{out, count};
}

template <auto V>
consteval auto default_value_for(::rflcpp::attrs::default_to<V>*) { return V; }

template <class T, class... Attrs>
consteval bool has_default_value() {
    bool found = false;
    (
        [&]<class A>() {
            if constexpr (requires(A* p) { default_value_for(p); })
                found = true;
        }.template operator()<Attrs>(),
        ...
    );
    return found;
}

template <class T, class... Attrs>
constexpr T default_value() {
    T out{};
    (
        [&]<class A>() {
            if constexpr (requires(A* p) { default_value_for(p); }) {
                auto val = default_value_for(static_cast<A*>(nullptr));
                if constexpr (requires { val.view(); }) {
                    out = T{val.view()};
                } else {
                    out = T{val};
                }
            }
        }.template operator()<Attrs>(),
        ...
    );
    return out;
}

} // namespace detail

template <class T, class... Attrs>
struct attr {
    using value_type = T;
    using attributes = std::tuple<Attrs...>;

    T value{};

    constexpr attr() = default;
    constexpr attr(T v) : value(std::move(v)) {}

    constexpr operator T&()             noexcept { return value; }
    constexpr operator const T&() const noexcept { return value; }

    constexpr T&       operator*()       noexcept { return value; }
    constexpr const T& operator*() const noexcept { return value; }
    constexpr T*       operator->()       noexcept { return &value; }
    constexpr const T* operator->() const noexcept { return &value; }

    template <class A>
    static constexpr bool has = detail::has_attr_v<A, Attrs...>;
};

template <fixed_string Name, class T, class... Attrs>
struct field {
    using value_type = T;
    using attributes = std::tuple<Attrs...>;
    static constexpr std::string_view name() noexcept { return Name.view(); }

    T value{};

    constexpr field() = default;
    constexpr field(T v) : value(std::move(v)) {}

    constexpr operator T&()             noexcept { return value; }
    constexpr operator const T&() const noexcept { return value; }

    constexpr T&       operator*()       noexcept { return value; }
    constexpr const T& operator*() const noexcept { return value; }
    constexpr T*       operator->()       noexcept { return &value; }
    constexpr const T* operator->() const noexcept { return &value; }

    template <class A>
    static constexpr bool has = detail::has_attr_v<A, Attrs...>;
};

template <class>                                struct is_attr  : std::false_type {};
template <class T, class... A>                  struct is_attr<attr<T, A...>>  : std::true_type {};

template <class>                                struct is_field : std::false_type {};
template <fixed_string N, class T, class... A>  struct is_field<field<N, T, A...>> : std::true_type {};

template <class T> inline constexpr bool is_attr_v    = is_attr<std::remove_cvref_t<T>>::value;
template <class T> inline constexpr bool is_field_v   = is_field<std::remove_cvref_t<T>>::value;
template <class T> inline constexpr bool is_wrapped_v = is_attr_v<T> || is_field_v<T>;

/// Strip an `attr<>` / `field<>` wrapper down to the underlying value type.
template <class T>             struct unwrap                       { using type = T; };
template <class T, class... A> struct unwrap<attr<T, A...>>        { using type = T; };
template <fixed_string N, class T, class... A>
                               struct unwrap<field<N, T, A...>>    { using type = T; };

template <class T> using unwrap_t = typename unwrap<std::remove_cvref_t<T>>::type;

} // namespace rflcpp
