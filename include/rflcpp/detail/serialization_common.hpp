#pragma once

#include <array>
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include <rflcpp/attributes.hpp>
#include <rflcpp/concepts.hpp>
#include <rflcpp/enum_meta.hpp>
#include <rflcpp/policy.hpp>
#include <rflcpp/reflect.hpp>

namespace rflcpp::detail::serialization {

template <class Owner, class AttrType>
std::string resolve_attr_name(std::string_view member_name) {
    using attrs_tuple = typename AttrType::attributes;
    constexpr auto rn = [&]<class... A>(std::tuple<A...>*) consteval {
        return rflcpp::detail::rename_of<A...>();
    }(static_cast<attrs_tuple*>(nullptr));

    if (!rn.empty()) return std::string{rn};
    return naming_policy<Owner>::transform(member_name);
}

template <class Owner, class FieldType>
inline std::string effective_key(std::string_view member_name) {
    if constexpr (is_field_v<FieldType>) {
        return std::string{FieldType::name()};
    } else if constexpr (is_attr_v<FieldType>) {
        return resolve_attr_name<Owner, FieldType>(member_name);
    } else {
        return naming_policy<Owner>::transform(member_name);
    }
}

template <class FieldType>
consteval bool skip_on_write_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        return F::template has<attrs::skip>
            || F::template has<attrs::skip_on_write>
            || F::template has<attrs::read_only>;
    } else return false;
}

template <class FieldType>
consteval bool sensitive_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::sensitive>;
    else                                         return false;
}

template <class FieldType>
consteval bool skip_if_null_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::skip_if_null>;
    else                                         return false;
}

template <class FieldType>
consteval bool skip_if_default_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::skip_if_default>;
    else                                         return false;
}

template <class FieldType>
consteval bool flatten_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return F::template has<attrs::flatten>;
    else                                         return false;
}

template <class FieldType, class Ref>
constexpr auto& unwrap_ref(Ref& r) noexcept {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) return r.value;
    else                                         return r;
}

template <class FieldType>
consteval bool skip_on_read_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        return F::template has<attrs::skip> || F::template has<attrs::skip_on_read>
            || F::template has<attrs::write_only>;
    } else return false;
}

template <class FieldType>
consteval bool required_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>)
        return F::template has<attrs::required>;
    else return false;
}

template <class FieldType>
consteval auto aliases_pair() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        using attrs_tuple = typename F::attributes;
        return [&]<class... A>(std::tuple<A...>*) consteval {
            return rflcpp::detail::aliases_of<A...>();
        }(static_cast<attrs_tuple*>(nullptr));
    } else {
        return std::pair{std::array<std::string_view, 0>{}, std::size_t{0}};
    }
}

template <class FieldType>
consteval bool has_default_v() {
    using F = std::remove_cvref_t<FieldType>;
    if constexpr (is_attr_v<F> || is_field_v<F>) {
        using attrs_tuple = typename F::attributes;
        return [&]<class... A>(std::tuple<A...>*) consteval {
            return rflcpp::detail::has_default_value<typename F::value_type, A...>();
        }(static_cast<attrs_tuple*>(nullptr));
    } else return false;
}

template <class FieldType>
constexpr auto default_v() {
    using F = std::remove_cvref_t<FieldType>;
    using T = typename F::value_type;
    using attrs_tuple = typename F::attributes;
    return [&]<class... A>(std::tuple<A...>*) constexpr {
        return rflcpp::detail::default_value<T, A...>();
    }(static_cast<attrs_tuple*>(nullptr));
}

} // namespace rflcpp::detail::serialization
