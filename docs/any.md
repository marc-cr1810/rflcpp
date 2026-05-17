# Reflected Any Container

`rflcpp::any` is a type-erased value container (similar to `std::any`) that retains compile-time static reflection capabilities at runtime. This enables dynamic inspection, safe type casting, heterogeneous collections, and out-of-the-box serialization.

## Public API

```cpp
#include <rflcpp/any.hpp>

namespace rflcpp {
    class any {
    public:
        // 1. Constructors
        constexpr any() noexcept;
        template <class T> any(T&& value);

        // 2. State & Metadata
        [[nodiscard]] bool empty() const noexcept;
        [[nodiscard]] std::string_view type_name() const noexcept;
        [[nodiscard]] const std::type_info& type_id() const noexcept;

        // 3. Type-safe casting
        template <class T> [[nodiscard]] T* cast() noexcept;
        template <class T> [[nodiscard]] const T* cast() const noexcept;
    };
}
```

---

## Detailed Example

Here is how you can use `rflcpp::any` to store arbitrary types, inspect their names, and cast them back safely at runtime:

```cpp
#include <iostream>
#include <vector>
#include <rflcpp/reflect.hpp>
#include <rflcpp/any.hpp>
#include <rflcpp/json.hpp>

struct Circle {
    double radius;
};

struct Square {
    double side;
};

int main() {
    // 1. Create a heterogeneous array using rflcpp::any
    std::vector<rflcpp::any> shapes;
    shapes.push_back(Circle{5.0});
    shapes.push_back(Square{4.0});
    shapes.push_back(100); // Primitive types are also supported!

    // 2. Iterate and inspect type names and metadata at runtime
    for (const auto& shape : shapes) {
        std::cout << "Stored Type Name: " << shape.type_name() << "\n";

        // Try casting to Circle
        if (auto* c = shape.cast<Circle>()) {
            std::cout << "  -> It's a Circle with radius=" << c->radius << "\n";
        }
        // Try casting to Square
        else if (auto* s = shape.cast<Square>()) {
            std::cout << "  -> It's a Square with side=" << s->side << "\n";
        }
        // Try casting to primitive int
        else if (auto* i = shape.cast<int>()) {
            std::cout << "  -> It's an int with value=" << *i << "\n";
        }
    }
}
```

---

## Serialization Integration

Because `rflcpp::any` retains internal reflection metadata, it is natively serializable out-of-the-box!
When you pass a collection or structure containing `rflcpp::any` fields to `to_json` (or any other format serialization function), the library automatically resolves the underlying type and serializes its fields directly:

```cpp
#include <iostream>
#include <rflcpp/json.hpp>
#include <rflcpp/any.hpp>

struct Canvas {
    std::string name;
    rflcpp::any root_shape;
};

int main() {
    Canvas c{"My Drawing", Circle{10.0}};
    
    // Stored rflcpp::any is resolved and serialized seamlessly!
    std::cout << rflcpp::to_json(c) << "\n";
    // Outputs: {"name":"My Drawing","root_shape":{"radius":10.0}}
}
```

For full heterogeneous *deserialization* (dynamic lookup by key tag), see the [Polymorphic Type Registry Guide](registry.md).
