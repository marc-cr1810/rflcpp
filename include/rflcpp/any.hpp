// rflcpp/any.hpp - Reflected 'any' type for type-erasure.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <type_traits>

#include <rflcpp/reflect.hpp>

namespace rflcpp {

namespace detail::any {

struct json_format_tag;
struct xml_format_tag;
struct yaml_format_tag;
struct toml_format_tag;
struct msgpack_format_tag;

struct vtable {
    std::string_view (*type_name)();
    std::type_index  (*type_id)();
};

template <class T>
const vtable* get_vtable() {
    static constexpr vtable vt = {
        .type_name = []() -> std::string_view { return type_name_of<T>(); },
        .type_id   = []() -> std::type_index  { return typeid(T); }
    };
    return &vt;
}

template <class FormatTag>
struct any_serializer_registry {
    using serialize_fn = void (*)(const void* ptr, void* output);
    static inline std::unordered_map<std::type_index, serialize_fn> map;

    static void register_type(std::type_index id, serialize_fn fn) {
        map[id] = fn;
    }

    static serialize_fn get(std::type_index id) {
        auto it = map.find(id);
        return it != map.end() ? it->second : nullptr;
    }
};

} // namespace detail::any

template <class T, class Enable = void>
struct json_populator_helper {
    static void populate() {}
};

template <class T, class Enable = void>
struct xml_populator_helper {
    static void populate() {}
};

template <class T, class Enable = void>
struct yaml_populator_helper {
    static void populate() {}
};

template <class T, class Enable = void>
struct toml_populator_helper {
    static void populate() {}
};

template <class T, class Enable = void>
struct msgpack_populator_helper {
    static void populate() {}
};

#ifdef RFLCPP_ENABLE_JSON
namespace detail::any {
template <class T>
struct json_serializer_holder {
    static inline void (*serialize_fn)(const void* ptr, void* output) = nullptr;
};
template <class T>
struct json_serializer_populator;
}
#endif

#ifdef RFLCPP_ENABLE_XML
namespace detail::any {
template <class T>
struct xml_serializer_holder {
    static inline void (*serialize_fn)(const void* ptr, void* output) = nullptr;
};
template <class T>
struct xml_serializer_populator;
}
#endif

#ifdef RFLCPP_ENABLE_YAML
namespace detail::any {
template <class T>
struct yaml_serializer_holder {
    static inline void (*serialize_fn)(const void* ptr, void* output) = nullptr;
};
template <class T>
struct yaml_serializer_populator;
}
#endif

#ifdef RFLCPP_ENABLE_TOML
namespace detail::any {
template <class T>
struct toml_serializer_holder {
    static inline void (*serialize_fn)(const void* ptr, void* output) = nullptr;
};
template <class T>
struct toml_serializer_populator;
}
#endif

#ifdef RFLCPP_ENABLE_MSGPACK
namespace detail::any {
template <class T>
struct msgpack_serializer_holder {
    static inline void (*serialize_fn)(const void* ptr, void* output) = nullptr;
};
template <class T>
struct msgpack_serializer_populator;
}
#endif

template <class T> struct is_shared_ptr : std::false_type {};
template <class T> struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};
template <class T> inline constexpr bool is_shared_ptr_v = is_shared_ptr<std::remove_cvref_t<T>>::value;

template <class T> struct is_unique_ptr : std::false_type {};
template <class T> struct is_unique_ptr<std::unique_ptr<T>> : std::true_type {};
template <class T> inline constexpr bool is_unique_ptr_v = is_unique_ptr<std::remove_cvref_t<T>>::value;

template <class T, class Enable = void>
struct smart_ptr_target {
    using type = T;
};

template <class T>
struct smart_ptr_target<T, std::enable_if_t<is_shared_ptr_v<T>>> {
    using type = typename T::element_type;
};

template <class T>
struct smart_ptr_target<T, std::enable_if_t<is_unique_ptr_v<T>>> {
    using type = typename T::element_type;
};

template <class Target>
void register_serializers_once() {
    static const bool registered = []() {
#ifdef RFLCPP_ENABLE_JSON
        json_populator_helper<Target>::populate();
        if (detail::any::json_serializer_holder<Target>::serialize_fn) {
            detail::any::any_serializer_registry<detail::any::json_format_tag>::register_type(
                typeid(Target), detail::any::json_serializer_holder<Target>::serialize_fn
            );
        }
#endif
#ifdef RFLCPP_ENABLE_XML
        xml_populator_helper<Target>::populate();
        if (detail::any::xml_serializer_holder<Target>::serialize_fn) {
            detail::any::any_serializer_registry<detail::any::xml_format_tag>::register_type(
                typeid(Target), detail::any::xml_serializer_holder<Target>::serialize_fn
            );
        }
#endif
#ifdef RFLCPP_ENABLE_YAML
        yaml_populator_helper<Target>::populate();
        if (detail::any::yaml_serializer_holder<Target>::serialize_fn) {
            detail::any::any_serializer_registry<detail::any::yaml_format_tag>::register_type(
                typeid(Target), detail::any::yaml_serializer_holder<Target>::serialize_fn
            );
        }
#endif
#ifdef RFLCPP_ENABLE_TOML
        toml_populator_helper<Target>::populate();
        if (detail::any::toml_serializer_holder<Target>::serialize_fn) {
            detail::any::any_serializer_registry<detail::any::toml_format_tag>::register_type(
                typeid(Target), detail::any::toml_serializer_holder<Target>::serialize_fn
            );
        }
#endif
#ifdef RFLCPP_ENABLE_MSGPACK
        msgpack_populator_helper<Target>::populate();
        if (detail::any::msgpack_serializer_holder<Target>::serialize_fn) {
            detail::any::any_serializer_registry<detail::any::msgpack_format_tag>::register_type(
                typeid(Target), detail::any::msgpack_serializer_holder<Target>::serialize_fn
            );
        }
#endif
        return true;
    }();
    (void)registered;
}

class any {
public:
    any() : ptr_(nullptr), vtable_(nullptr) {}

    template <class T>
        requires (!std::is_same_v<std::decay_t<T>, any>)
    any(T&& val) {
        using U = std::decay_t<T>;
        using Target = typename smart_ptr_target<U>::type;

        if constexpr (is_shared_ptr_v<U>) {
            ptr_ = std::static_pointer_cast<void>(std::forward<T>(val));
        } else if constexpr (is_unique_ptr_v<U>) {
            ptr_ = std::shared_ptr<typename U::element_type>(std::forward<T>(val));
        } else {
            ptr_ = std::make_shared<U>(std::forward<T>(val));
        }

        vtable_ = detail::any::get_vtable<Target>();
        register_serializers_once<Target>();
    }

    [[nodiscard]] bool empty() const noexcept { return ptr_ == nullptr; }
    [[nodiscard]] std::string_view type_name() const noexcept {
        return vtable_ ? vtable_->type_name() : "empty";
    }
    [[nodiscard]] std::type_index type_id() const noexcept {
        return vtable_ ? vtable_->type_id() : typeid(void);
    }

    [[nodiscard]] const void* ptr() const noexcept { return ptr_.get(); }

    template <class T>
    T* cast() {
        if (type_id() == typeid(T)) return static_cast<T*>(ptr_.get());
        return nullptr;
    }

    template <class T>
    const T* cast() const {
        if (type_id() == typeid(T)) return static_cast<const T*>(ptr_.get());
        return nullptr;
    }

private:
    std::shared_ptr<void> ptr_;
    const detail::any::vtable* vtable_;
};

} // namespace rflcpp
