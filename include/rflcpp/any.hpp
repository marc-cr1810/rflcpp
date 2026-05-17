// rflcpp/any.hpp - Reflected 'any' type for type-erasure.
// SPDX-License-Identifier: MIT

#pragma once

#include <memory>
#include <string_view>
#include <typeindex>

#include <rflcpp/reflect.hpp>
#include <nlohmann/json.hpp>

namespace rflcpp {

using njson = nlohmann::json;

namespace detail::json {
template <class T>
njson write_dispatch(const T& v);
}

namespace detail::any {

struct vtable {
    std::string_view (*type_name)();
    std::type_index  (*type_id)();
    njson            (*to_json)(const void* ptr);
};

template <class T>
const vtable* get_vtable() {
    static constexpr vtable vt = {
        .type_name = []() -> std::string_view { return type_name_of<T>(); },
        .type_id   = []() -> std::type_index  { return typeid(T); },
        .to_json   = [](const void* ptr) -> njson {
            return rflcpp::detail::json::write_dispatch(*static_cast<const T*>(ptr));
        }
    };
    return &vt;
}

} // namespace detail::any

class any {
public:
    any() : ptr_(nullptr), vtable_(nullptr) {}

    template <class T>
        requires (!std::is_same_v<std::decay_t<T>, any>)
    any(T&& val) {
        using U = std::decay_t<T>;
        ptr_ = std::make_shared<U>(std::forward<T>(val));
        vtable_ = detail::any::get_vtable<U>();
    }

    [[nodiscard]] bool empty() const noexcept { return ptr_ == nullptr; }
    [[nodiscard]] std::string_view type_name() const noexcept {
        return vtable_ ? vtable_->type_name() : "empty";
    }
    [[nodiscard]] std::type_index type_id() const noexcept {
        return vtable_ ? vtable_->type_id() : typeid(void);
    }

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

    [[nodiscard]] njson to_json() const {
        if (vtable_) return vtable_->to_json(ptr_.get());
        return nullptr;
    }

private:
    std::shared_ptr<void> ptr_;
    const detail::any::vtable* vtable_;
};

} // namespace rflcpp
